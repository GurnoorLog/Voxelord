#include "server/GameServer.h"

#include <iostream>
#include <thread>

#include "game/BulkEdit.h"
#include "maths/Converter.h"
#include "util/Logger.h"

GameServer::GameServer(uint16_t port) : m_port{ port } {}

GameServer::~GameServer() {
	stop();
}

bool GameServer::start() {
	if (m_listener.listen(m_port) != sf::Socket::Done) {
		LOG(Level::ERROR) << "GameServer: cannot bind port " << m_port << std::endl;
		return false;
	}
	m_listener.setBlocking(false);
	m_started = true;
	LOG(Level::INFO) << "GameServer listening on port " << m_port << std::endl;
	return true;
}

void GameServer::stop() {
	m_stop = true;
}

void GameServer::run() {
	while (!m_stop) {
		acceptNewClients();

		for (std::unique_ptr<Client>& client : m_clients)
			if (!client->dead)
				receiveFrom(*client);
		pruneDeadClients();

		float elapsed = m_tickClock.restart().asSeconds();
		m_accumulator += elapsed;
		// Cap catch-up so one slow frame (heavy chunk gen, busy machine) never runs physics in one
		// big burst. Unlike the old "drop everything after 10 steps", we keep the residual so the
		// sim never jumps backward relative to itself, which used to widen the prediction gap.
		m_accumulator = std::min(m_accumulator, 4.f * TICK_TIME);
		while (m_accumulator >= TICK_TIME) {
			tick();
			m_accumulator -= TICK_TIME;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	disconnectAll("Server closed");
	LOG(Level::INFO) << "GameServer stopped" << std::endl;
}

void GameServer::acceptNewClients() {
	auto client = std::make_unique<Client>();
	sf::Socket::Status status = m_listener.accept(client->socket);
	if (status == sf::Socket::Done) {
		client->socket.setBlocking(false);
		m_clients.push_back(std::move(client));
		LOG(Level::INFO) << "Client connected (" << m_clients.size() << " total)" << std::endl;
	}
}

void GameServer::receiveFrom(Client& client) {
	// Drain every complete packet currently buffered so input never falls behind the tick.
	// sf::Socket::Partial just means the socket still holds an unfinished packet (SFML 2.6
	// buffers it internally) — retry next iteration, it is not a disconnect.
	sf::Packet packet;
	while (true) {
		sf::Socket::Status status = client.socket.receive(packet);
		if (status == sf::Socket::Done) {
			handlePacket(client, packet);
			packet.clear();
		} else if (status == sf::Socket::NotReady || status == sf::Socket::Partial) {
			break;
		} else {
			client.dead = true;
			break;
		}
	}
}

void GameServer::pruneDeadClients() {
	for (auto it = m_clients.begin(); it != m_clients.end();) {
		if (!(*it)->dead) {
			++it;
			continue;
		}
		Client& client = **it;
		if (client.welcomeSent && client.player) {
			sf::Packet p = Protocol::makePacket(Protocol::MSG_PLAYER_LEAVE);
			p << static_cast<uint32_t>(client.player->id);
			broadcast(p, &client);
			LOG(Level::INFO) << client.player->name << " disconnected" << std::endl;
		}
		client.socket.disconnect();
		it = m_clients.erase(it);
	}
}

void GameServer::handlePacket(Client& client, sf::Packet& packet) {
	Protocol::MessageType type;
	if (!Protocol::readType(packet, type))
		return;

	switch (type) {
	case Protocol::MSG_HELLO: {
		if (client.player != nullptr)
			break;
		std::string name;
		packet >> name;
		auto player = std::make_unique<PlayerEntity>();
		player->id = static_cast<int>(m_nextPlayerId++);
		player->name = name.empty() ? "Player" : name;
		player->controller.setWorldView(&m_world);
		client.player = std::move(player);
		client.welcomeSent = true;
		sendWelcome(client);

		sf::Packet jp = Protocol::makePacket(Protocol::MSG_PLAYER_JOIN);
		jp << static_cast<uint32_t>(client.player->id) << client.player->name;
		broadcast(jp, &client);
		LOG(Level::INFO) << client.player->name << " joined as id " << client.player->id << std::endl;
		break;
	}
case Protocol::MSG_INPUT:
		if (client.player != nullptr)
			client.player->latestInput = Protocol::readInput(packet);
		break;
	case Protocol::MSG_BLOCK_EDIT: {
		int x, y, z;
		uint8_t blockType;
		packet >> x >> y >> z >> blockType;
		handleBlockEdit(client, x, y, z, blockType);
		break;
	}
	case Protocol::MSG_BULK_EDIT: {
		int cx, cy, cz;
		uint16_t radius;
		uint8_t blockType;
		packet >> cx >> cy >> cz >> radius >> blockType;
		handleBulkEdit(client, cx, cy, cz, radius, blockType);
		break;
	}
	case Protocol::MSG_CHAT: {
		if (!client.welcomeSent)
			break;
		std::string text;
		packet >> text;
		if (text.empty())
			break;
		// Server-side sanitation: strip anything we would rather not echo back.
		if (text.size() > 256)
			text.resize(256);
		sf::Packet p = Protocol::makePacket(Protocol::MSG_CHAT_S);
		p << client.player->name << text;
		broadcast(p);
		break;
	}
	default:
		break;
	}
}

void GameServer::sendWelcome(Client& client) {
	vec3 spawn = m_world.findSpawn();
	// Scatter players around the base spawn so a fresh join does not appear inside an
	// existing player: a small grid, 1.5 m apart, keyed off the player id.
	const int id = client.player->id;
	spawn.x += static_cast<float>((id % 3) - 1) * 1.5f;
	spawn.z += static_cast<float>(((id / 3) % 3) - 1) * 1.5f;

	client.player->position = spawn;
	client.player->controller.setPosition(spawn);

	sf::Packet p = Protocol::makePacket(Protocol::MSG_WELCOME);
	p << Protocol::PROTOCOL_VERSION << 0u
		<< spawn.x << spawn.y << spawn.z
		<< m_timeOfDay << static_cast<uint32_t>(client.player->id);
	client.socket.send(p);
	LOG(Level::INFO) << client.player->name << " spawned at " << spawn.x << ',' << spawn.y << ',' << spawn.z << std::endl;
}

void GameServer::handleBlockEdit(Client& client, int x, int y, int z, uint8_t blockType) {
	if (client.player == nullptr || !client.welcomeSent)
		return;
	if (blockType >= BlockID::SIZE)
		return;

	vec3 blockCenter{ x + 0.5f, y + 0.5f, z + 0.5f };
	if (glm::distance(blockCenter, client.player->position) > REACH)
		return;
	if (client.lastEditClock.getElapsedTime().asMilliseconds() < EDIT_COOLDOWN_MS)
		return;
	client.lastEditClock.restart();

	Block target{ BlockID::_from_integral(blockType) };
	// Placing a solid block into an already-solid cell changes nothing; reject the redundant edit.
	if (target.id != +BlockID::AIR && m_world.blockData(target.id).isObstacle()) {
		Block current = m_world.getBlock({ x, y, z });
		if (m_world.blockData(current.id).isObstacle())
			return;
	}

	m_world.setBlock({ x, y, z }, target);

	sf::Packet p = Protocol::makePacket(Protocol::MSG_BLOCK_EDIT_S);
	p << static_cast<int32_t>(x) << static_cast<int32_t>(y) << static_cast<int32_t>(z) << blockType;
	// The sender already predicted it; everyone else needs it.
	broadcast(p, &client);
}

void GameServer::handleBulkEdit(Client& client, int cx, int cy, int cz, uint16_t radius, uint8_t blockType) {
	if (client.player == nullptr || !client.welcomeSent)
		return;
	if (blockType >= BlockID::SIZE || radius > MAX_BULK_RADIUS)
		return;

	vec3 center{ cx + 0.5f, cy + 0.5f, cz + 0.5f };
	if (glm::distance(center, client.player->position) > REACH + radius)
		return;
	if (client.lastEditClock.getElapsedTime().asMilliseconds() < EDIT_COOLDOWN_MS)
		return;
	client.lastEditClock.restart();

	std::vector<BlockEdit> edits = BulkEdit::smoothSphere({ cx, cy, cz }, radius, Block{ BlockID::_from_integral(blockType) });
	for (const BlockEdit& edit : edits)
		m_world.setBlock(edit.pos, edit.block);

	sf::Packet p = Protocol::makePacket(Protocol::MSG_BULK_EDIT_S);
	p << static_cast<int32_t>(cx) << static_cast<int32_t>(cy) << static_cast<int32_t>(cz) << radius << blockType;
	broadcast(p, &client);
}

void GameServer::applyInput(PlayerEntity& player, float dt) {
	const Protocol::PlayerInput& in = player.latestInput;
player.controller.setYawPitch(in.yaw, in.pitch);
	player.controller.setFlying(in.moveFlags & Protocol::FLAG_FLYING);
	player.controller.setPosition(player.position);

	if (in.moveFlags & Protocol::FLAG_FORWARD) player.controller.move(PlayerController::FORWARD, dt);
	if (in.moveFlags & Protocol::FLAG_BACKWARD) player.controller.move(PlayerController::BACKWARD, dt);
	if (in.moveFlags & Protocol::FLAG_LEFT) player.controller.move(PlayerController::LEFT, dt);
	if (in.moveFlags & Protocol::FLAG_RIGHT) player.controller.move(PlayerController::RIGHT, dt);
	if (in.moveFlags & Protocol::FLAG_UP) player.controller.move(PlayerController::UP, dt);
	if (in.moveFlags & Protocol::FLAG_DOWN) player.controller.move(PlayerController::DOWN, dt);
	if (in.sprint || (in.moveFlags & Protocol::FLAG_SPRINT))
		player.controller.setSprinting(true);

	player.controller.update(dt);
	vec3 shift = player.controller.getMoveAndReset(dt);
	player.position += shift;
	player.yaw = in.yaw;
	player.pitch = in.pitch;
	player.onGround = player.controller.isOnGround();
}

void GameServer::tick() {
	for (std::unique_ptr<Client>& client : m_clients) {
		if (client->player == nullptr)
			continue;
// Keep the physics neighborhood generated around every player. Budgeted so chunk
		// generation (center-first, a few per tick) never stalls the receive loop for seconds
		// while the world around a fresh spawn fills in.
		ivec2 chunk = Converter::globalToChunk(ivec3{ client->player->position.x, 0, client->player->position.z });
		m_world.generateAround(chunk, WORLD_RADIUS_CHUNKS, 4);
applyInput(*client->player, TICK_TIME);
		if (m_tickCounter % 60 == 0) {
			const Protocol::PlayerInput& in = client->player->latestInput;
			LOG(Level::INFO) << "tick p" << client->player->id
				<< " pos=" << client->player->position.x << ',' << client->player->position.y << ',' << client->player->position.z
				<< " flags=" << static_cast<int>(in.moveFlags)
				<< " yaw=" << in.yaw << " seq=" << in.seq << std::endl;
		}
	}

	m_timeOfDay = std::fmod(m_timeOfDay + TICK_TIME / 120.f, 1.f);

	broadcastPlayerStates();

	if (++m_tickCounter % 30 == 0) {
		sf::Packet p = Protocol::makePacket(Protocol::MSG_TIME);
		p << m_timeOfDay;
		broadcast(p);
	}
}

void GameServer::broadcast(sf::Packet& packet, const Client* except) {
	for (std::unique_ptr<Client>& client : m_clients) {
		if (client.get() == except)
			continue;
		sf::Socket::Status status = client->socket.send(packet);
		if (status == sf::Socket::Disconnected || status == sf::Socket::Error)
			client->dead = true;
	}
}

void GameServer::broadcastPlayerStates() {
	for (std::unique_ptr<Client>& target : m_clients) {
		if (!target->welcomeSent)
			continue;
		for (std::unique_ptr<Client>& source : m_clients) {
			if (source->player == nullptr)
				continue;
			PlayerEntity& player = *source->player;
			sf::Packet p = Protocol::makePacket(Protocol::MSG_PLAYER_STATE);
			p << static_cast<uint32_t>(player.id)
				<< player.position.x << player.position.y << player.position.z
				<< player.yaw << player.pitch
				<< static_cast<uint8_t>(player.onGround ? 1 : 0);
			target->socket.send(p);
		}
	}
}

void GameServer::disconnectAll(const std::string& reason) {
	for (std::unique_ptr<Client>& client : m_clients) {
		sf::Packet p = Protocol::makePacket(Protocol::MSG_DISCONNECT);
		p << reason;
		client->socket.send(p);
		client->socket.disconnect();
	}
	m_clients.clear();
}
