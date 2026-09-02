#include "Game.h"

#include "Game/BulkEdit.h"
#include "Generator/WorldGenerator.h"
#include "Maths/Converter.h"
#include "Maths/LineBlockFinder.h"
#include "Server/GameServer.h"
#include "Util/DebugGL.h"
#include "Util/Logger.h"
#include "World/WorldConstants.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "Block/BlockID.h"

namespace {
	vec3 playerColor(int id) {
		static const std::vector<vec3> palette{
			{ 0.90f, 0.30f, 0.30f }, { 0.30f, 0.80f, 0.30f }, { 0.30f, 0.55f, 0.90f },
			{ 0.90f, 0.80f, 0.20f }, { 0.80f, 0.40f, 0.80f }, { 0.20f, 0.90f, 0.90f },
			{ 0.90f, 0.60f, 0.30f }, { 0.60f, 0.90f, 0.30f }
		};
		return palette[static_cast<std::size_t>(id) % palette.size()];
	}
}

Game::Game(Window* const window)
	: m_player{ this }, p_window{ window },
	m_defaultRenderer{ m_player.getCamera(), m_dayCycle },
	m_waterRenderer{ p_window->size(), m_player.getCamera(), m_dayCycle },
	m_lavaRenderer{ m_player.getCamera(), m_dayCycle },
	m_postProcessingRenderer{ p_window->size(), m_dayCycle } {

	ResManager::initBlockDatas(std::vector<TextureArray*>{ &m_defaultRenderer.getTextureArray() });
	m_skyRenderer.load();

	unsigned int workerCount = ChunkMap::LOADING_WORKERS_COUNT;
	for (unsigned int i = 0; i < workerCount; ++i)
		m_workerThreads.emplace_back(&Game::runWorkerLoop, this);
	m_orchestratorThread = std::thread{ &Game::runOrchestratorLoop, this };
}

Game::~Game() {
	m_stopGeneratingThread = true;
	m_chunkMap.stop();
	m_orchestratorThread.join();
	for (std::thread& worker : m_workerThreads)
		worker.join();
}

void Game::runOrchestratorLoop() {
	while (!m_stopGeneratingThread) {
		m_chunkMap.refreshSelection(m_player.getCamera().getFrustum());
		m_chunkMap.unloadFarChunks();
		g_worldGenerator.unloadFarStructures(m_chunkMap.getCenter(), ChunkMap::VIEW_DISTANCE);
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
}

void Game::runWorkerLoop() {
	while (!m_stopGeneratingThread) {
		m_chunkMap.processNextTask();
	}
}

void Game::onChangedSize(ivec2 size) {
	glViewport(0, 0, size.x, size.y);
	p_window->setView(sf::View(sf::FloatRect(0.f, 0.f, static_cast<float>(size.x), static_cast<float>(size.y))));
	m_waterRenderer.onChangedSize(p_window->size());
	m_postProcessingRenderer.onChangedSize(p_window->size());
}

void Game::submitSphereEdit(int radius, Block block, bool explosive) {
	if (!m_player.getTarget().has_value())
		return;
	ivec3 center = m_player.getTarget().value();
	bool accepted = m_chunkMap.submitBulkEdit([center, radius, block]() {
		return BulkEdit::smoothSphere(center, radius, block);
	});
	if (accepted && explosive)
		m_explosionDrawer.spawn(vec3(center) + vec3(0.5f), float(radius));

	// The server caps bulk edits; send the capped radius so everyone's world matches.
	if (accepted && m_online) {
		uint16_t r = static_cast<uint16_t>(std::min(radius, static_cast<int>(Protocol::MAX_BULK_EDIT_RADIUS)));
		m_network.sendBulkEdit(center, r, block.id);
	}
}

void Game::setBlockNetwork(ivec3 pos, Block block) {
	m_chunkMap.setBlock(pos, block);
	if (m_online)
		m_network.sendBlockEdit(pos, block.id);
}

void Game::processKeyboard(sf::Time dt, Commands& commands) {
	// Mirror the held keys into the network input snapshot (sent each frame while online).
	m_netMoveFlags = 0;
	if (commands.isActive(Command::FORWARD)) m_netMoveFlags |= Protocol::FLAG_FORWARD;
	if (commands.isActive(Command::BACKWARD)) m_netMoveFlags |= Protocol::FLAG_BACKWARD;
	if (commands.isActive(Command::LEFT)) m_netMoveFlags |= Protocol::FLAG_LEFT;
	if (commands.isActive(Command::RIGHT)) m_netMoveFlags |= Protocol::FLAG_RIGHT;
	if (commands.isActive(Command::UP)) m_netMoveFlags |= Protocol::FLAG_UP;
	if (commands.isActive(Command::DOWN)) m_netMoveFlags |= Protocol::FLAG_DOWN;
	m_netSprint = commands.isActive(Command::SPRINT);

	if (commands.isActive(Command::FORWARD))
		m_player.move(PlayerController::FORWARD, dt);
	if (commands.isActive(Command::BACKWARD))
		m_player.move(PlayerController::BACKWARD, dt);
	if (commands.isActive(Command::LEFT))
		m_player.move(PlayerController::LEFT, dt);
	if (commands.isActive(Command::RIGHT))
		m_player.move(PlayerController::RIGHT, dt);
	if (commands.isActive(Command::UP))
		m_player.move(PlayerController::UP, dt);
	if (commands.isActive(Command::DOWN))
		m_player.move(PlayerController::DOWN, dt);
	if (commands.isActive(Command::SPRINT))
		m_player.setSprinting(true);
	// --test-move: hold W in online mode.
	if (m_testMove && m_online) {
		m_netMoveFlags = Protocol::FLAG_FORWARD;
		m_player.move(PlayerController::FORWARD, dt);
	}
	if (commands.isActive(Command::EXPLOSION))
		submitSphereEdit(15, +BlockID::AIR, true);
	if (commands.isActive(Command::HUGE_EXPLOSION))
		submitSphereEdit(100, +BlockID::AIR, true);
	if (commands.isActive(Command::BRUSH))
		if (m_player.getPickedBlock().has_value())
			submitSphereEdit(15, m_player.getPickedBlock().value());
	if (commands.isActive(Command::HUGE_BRUSH))
		if (m_player.getPickedBlock().has_value())
			submitSphereEdit(100, m_player.getPickedBlock().value());
	if (commands.isActive(Command::PLACE_BELOW))
		m_player.placeBlockBelow();
	// Local-only cheats, disabled while online.
	if (!m_online && commands.isActive(Command::TELEPORT))
		m_player.teleport();
	if (!m_online && commands.isActive(Command::NEXT_GAMEMODE))
		m_player.nextGameMode();
	// Tab/Shift+Tab step once per press.
	if (commands.isActive(Command::NEXT_BLOCK)) {
		commands.onKeyReleased(sf::Keyboard::Tab);
		cycleHotbarBlock(+1);
	}
	if (commands.isActive(Command::PREV_BLOCK)) {
		commands.onKeyReleased(sf::Keyboard::Tab);
		cycleHotbarBlock(-1);
	}
}

void Game::processMouseClick(sf::Time dt, Commands& commands) {
	m_player.processMouseClick(dt, commands);
}

void Game::processMouseMove(sf::Time dt) {
	m_player.processMouseMove(dt);
}

void Game::processMouseWheel(sf::Time dt, GLfloat delta) {
	m_player.processMouseWheel(dt, delta);
}

void Game::cycleHotbarBlock(int dir) {
	m_player.cycleBlock(dir);
}

void Game::selectHotbarSlot(int slot) {
	m_player.selectBlock(slot);
}

void Game::update(sf::Time dt) {
	pollNetwork();

	if (m_mcpActive) {
		drainMcp(dt);
		// Wire the bot in after the primary socket is live.
		if (m_online && !m_mcpBotIdMapped) {
			m_mcpBotIdMapped = true;
			connectMcpBot();
		}
		// The bot is its own connection; poll it and stream input at 30 Hz.
		pollMcpNetwork();
		updateBotTask(dt);
		applyOllamaResult();
		sendMcpInput();
	}

	if (m_online) {
		sendInput();
		// Interpolate remote players toward their latest server positions (30 Hz updates).
		for (auto& entry : m_remotePlayers) {
			RemotePlayer& remote = entry.second;
			remote.timer += dt.asSeconds();
			float t = std::min(remote.timer / 0.1f, 1.f);
			remote.position = glm::mix(remote.prevPosition, remote.serverPosition, t);
		}
		buildRemoteVisuals();
		if (m_netDiagCounter % 60 == 0)
			LOG(Level::INFO) << "net local pos=" << m_player.getPosition().x << ',' << m_player.getPosition().y
				<< ',' << m_player.getPosition().z << " flags=" << static_cast<int>(m_netMoveFlags) << std::endl;
		++m_netDiagCounter;
	}

	m_player.update(dt);

	// Ease the camera toward the last authoritative server position (smooth reconciliation).
	if (m_online && m_hasReconcile) {
		vec3 cur = m_player.getPosition();
		vec3 toTarget = m_reconcileTarget - cur;
		float dist = glm::length(toTarget);
		if (dist <= 0.05f) {
			m_hasReconcile = false;
		} else {
			float step = std::min(dist, m_reconcileSpeed * dt.asSeconds());
			m_player.getCamera().setPosition(cur + toTarget * (step / dist));
		}
	}

	m_dayCycle.update(dt);

	m_chunkMap.setCenter(Converter::globalToChunk(m_player.getPosition()));
	m_chunkMap.update();

	BlockID eyeBlock = m_chunkMap.getBlock(Converter::globalPosToBlock(m_player.getPosition())).id;
	m_waterRenderer.setUnderwater(eyeBlock == +BlockID::WATER);
	m_postProcessingRenderer.setUnderwater(eyeBlock == +BlockID::WATER);
	m_postProcessingRenderer.setInLava(eyeBlock == +BlockID::LAVA);

	m_defaultRenderer.update(dt);
	m_waterRenderer.update(dt);
	m_lavaRenderer.update(dt);
	m_postProcessingRenderer.update(dt);

	m_explosionDrawer.update(dt.asSeconds());
}

void Game::clearRenderTarget() {
	vec3 skyColor = m_dayCycle.getSkyColor();
	glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Skydome right after the clear, depth off, so terrain overdraws it.
	m_skyRenderer.render(m_player.getCamera(), std::fmod(m_dayCycle.getTimeOfDay() + 0.25f, 1.f));
}

namespace {
	// sf::RenderTexture hides its FBO, but activating one binds it, so read the current binding.
	GLuint getFramebufferHandle(sf::RenderTexture& renderTexture) {
		renderTexture.setActive(true);
		GLint handle = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &handle);
		return static_cast<GLuint>(handle);
	}
}

void Game::render() {
	ivec2 size = p_window->size();

	m_waterRenderer.prepare([this](bool refractionPass) {
		// Clip at sea level; below-surface objects aren't mirrored above it.
		vec4 clipPlane = refractionPass ? vec4(0.f, -1.f, 0.f, 10000.f)
		                                : vec4(0.f, 1.f, 0.f, -float(Const::SEA_LEVEL));
		m_lavaRenderer.getShader().use().set("clipPlane", clipPlane);
		m_chunkMap.render(m_player.getCamera().getFrustum(), { &m_defaultRenderer, &m_lavaRenderer });
		m_remotePlayerDrawer.render(m_remoteVisuals,
			m_player.getCamera().getViewMatrix(), m_player.getCamera().getProjMatrix());
		// Fireball goes into the scene textures so the water shader shows it through the surface.
		m_explosionDrawer.render(m_player.getCamera().getViewMatrix(),
			m_player.getCamera().getProjMatrix(), clipPlane, m_dayCycle.getSkyColor());
	}, [this]() {
		clearRenderTarget();
	}, m_defaultRenderer, m_player.getCamera(), size);

	m_postProcessingRenderer.prepare([this, size]() {
		// The refraction texture already contains the whole scene (color and depth)
		// rendered from the player's point of view, so we reuse it as the base image
		// instead of rendering the terrain a second time. We blit it into the
		// post-processing target (keeping the depth buffer so the water is correctly
		// occluded) and then only draw the water on top, which samples the refraction
		// texture in the water shader.
		GLuint refractionFbo = getFramebufferHandle(m_waterRenderer.getRefractionTexture());
		GLuint renderFbo = getFramebufferHandle(m_postProcessingRenderer.getRenderTexture());

		glBindFramebuffer(GL_READ_FRAMEBUFFER, refractionFbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderFbo);
		glBlitFramebuffer(0, 0, size.x, size.y, 0, 0, size.x, size.y,
			GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, renderFbo);

		m_chunkMap.render(m_player.getCamera().getFrustum(), { &m_waterRenderer });

		// Drawn into the post-processing FBO which still holds the world depth.
		m_blockContourDrawer.render(m_player.getTarget(),
			m_player.getCamera().getViewMatrix(), m_player.getCamera().getProjMatrix());
	}, [this]() {
		clearRenderTarget();
	});

	p_window->setActive(true);
	clearRenderTarget();
	m_postProcessingRenderer.render();

	m_pickedBlockDrawer.render(m_player.getPickedBlock(), p_window, m_defaultRenderer);
}

Player& Game::getPlayer() {
	return m_player;
}

ChunkMap & Game::getChunkMap() {
	return m_chunkMap;
}

const ChunkMap& Game::getChunkMap() const {
	return m_chunkMap;
}

Window& Game::getWindow() {
	return *p_window;
}

float Game::getTimeOfDay() const {
	return m_dayCycle.getTimeOfDay();
}

void Game::startOnline(const std::string& name, const std::string& host, uint16_t port) {
	m_playerName = name.empty() ? "Player" : name;
	m_connectHost = host;
	m_connectPort = port;
	if (!m_network.connect(host, port)) {
		LOG(Level::ERROR) << "Could not connect to " << host << ":" << port << std::endl;
		addChatLine("Server", "Could not connect to " + host + ":" + std::to_string(port));
		m_shouldReturnToMenu = true;
		return;
	}
	LOG(Level::INFO) << "Connected to " << host << ":" << port << std::endl;
	m_network.sendHello(m_playerName);
}

void Game::stopOnline() {
	m_network.disconnect();
	if (m_mcpNetwork.isConnected()) {
		m_mcpNetwork.disconnect();
		m_mcpBotOnline = false;
		m_mcpBotId = -1;
	}
	m_online = false;
	m_selfId = -1;
	m_remotePlayers.clear();
	m_remoteVisuals.clear();
	m_chatLines.clear();
	m_shouldReturnToMenu = false;
}

void Game::pollNetwork() {
	if (!m_network.isConnected()) {
		// The connection died without a graceful disconnect (server crash, network drop).
		if (m_online) {
			addChatLine("Server", "Connection lost");
			m_shouldReturnToMenu = true;
		}
		return;
	}
	m_network.poll([this](Protocol::MessageType type, sf::Packet& packet) {
		handleNetworkMessage(type, packet);
	});
}

void Game::handleNetworkMessage(Protocol::MessageType type, sf::Packet& packet) {
	switch (type) {
	case Protocol::MSG_WELCOME: {
		uint32_t version, seed, selfId;
		float sx, sy, sz, timeOfDay;
		packet >> version >> seed >> sx >> sy >> sz >> timeOfDay >> selfId;
		if (version != Protocol::PROTOCOL_VERSION) {
			addChatLine("Server", "Protocol mismatch (client " + std::to_string(Protocol::PROTOCOL_VERSION) +
				", server " + std::to_string(version) + ")");
			m_shouldReturnToMenu = true;
			break;
		}
		m_selfId = static_cast<int>(selfId);
		m_online = true;
		m_player.getCamera().setPosition(vec3(sx, sy, sz));
		m_dayCycle.setTimeOfDay(timeOfDay);
		LOG(Level::INFO) << "Welcome: selfId=" << m_selfId << " spawn=" << sx << "," << sy << "," << sz << std::endl;
		addChatLine("Server", "Welcome to VoxLord, " + m_playerName + "!");
		break;
	}
	case Protocol::MSG_PLAYER_STATE: {
		uint32_t id;
		float x, y, z, yaw, pitch;
		uint8_t onGround;
		packet >> id >> x >> y >> z >> yaw >> pitch >> onGround;
		if (id == static_cast<uint32_t>(m_selfId)) {
			// Resync only on real divergence; the server lags local prediction by input lag.
			vec3 serverPos{ x, y, z };
			float drift = glm::distance(m_player.getPosition(), serverPos);
			if (drift > 1.5f) {
				// Ease toward the server position in update() instead of teleporting.
				m_reconcileTarget = serverPos;
				m_hasReconcile = true;
				LOG(Level::INFO) << "net snap drift=" << drift
					<< " local=" << m_player.getPosition().x << ',' << m_player.getPosition().y << ',' << m_player.getPosition().z
					<< " server=" << x << ',' << y << ',' << z << std::endl;
			}
			break;
		}
		RemotePlayer remote;
		auto it = m_remotePlayers.find(static_cast<int>(id));
		if (it == m_remotePlayers.end()) {
			// First sighting: seed at the reported position so it doesn't glide in from the origin.
			remote.name = "Player";
			remote.serverPosition = remote.prevPosition = remote.position = { x, y, z };
			remote.yaw = yaw;
			remote.pitch = pitch;
			remote.onGround = onGround != 0;
			m_remotePlayers.emplace(static_cast<int>(id), remote);
			LOG(Level::INFO) << "Tracking remote player id " << id << " at " << x << ',' << y << ',' << z << std::endl;
			break;
		}
		RemotePlayer& tracked = it->second;
		tracked.prevPosition = tracked.position;
		tracked.serverPosition = { x, y, z };
		tracked.yaw = yaw;
		tracked.pitch = pitch;
		tracked.onGround = onGround != 0;
		tracked.timer = 0.f;
		if (tracked.name.empty())
			tracked.name = "Player";
		break;
	}
	case Protocol::MSG_PLAYER_JOIN: {
		uint32_t id;
		std::string name;
		packet >> id >> name;
		m_remotePlayers[static_cast<int>(id)].name = name;
		addChatLine("Server", name + " joined");
		break;
	}
	case Protocol::MSG_PLAYER_LEAVE: {
		uint32_t id;
		packet >> id;
		auto it = m_remotePlayers.find(static_cast<int>(id));
		if (it != m_remotePlayers.end()) {
			addChatLine("Server", it->second.name + " left");
			m_remotePlayers.erase(it);
		}
		break;
	}
	case Protocol::MSG_BLOCK_EDIT_S: {
		int x, y, z;
		uint8_t blockType;
		packet >> x >> y >> z >> blockType;
		m_chunkMap.setBlock({ x, y, z }, Block{ BlockID::_from_integral(blockType) });
		break;
	}
	case Protocol::MSG_BULK_EDIT_S: {
		int cx, cy, cz;
		uint16_t radius;
		uint8_t blockType;
		packet >> cx >> cy >> cz >> radius >> blockType;
		m_chunkMap.submitBulkEdit([cx, cy, cz, radius, blockType]() {
			return BulkEdit::smoothSphere({ cx, cy, cz }, radius, Block{ BlockID::_from_integral(blockType) });
		});
		break;
	}
	case Protocol::MSG_TIME: {
		float timeOfDay;
		packet >> timeOfDay;
		m_dayCycle.setTimeOfDay(timeOfDay);
		break;
	}
	case Protocol::MSG_CHAT_S: {
		std::string sender, text;
		packet >> sender >> text;
		addChatLine(sender, text);
		break;
	}
	case Protocol::MSG_DISCONNECT: {
		std::string reason;
		packet >> reason;
		addChatLine("Server", reason.empty() ? "Disconnected" : reason);
		m_shouldReturnToMenu = true;
		break;
	}
	default:
		break;
	}
}

void Game::sendInput() {
	if (!m_online)
		return;
	// 30 Hz to match the server tick.
	if (m_lastInputClock.getElapsedTime().asMilliseconds() < 33)
		return;
	m_lastInputClock.restart();
	Protocol::PlayerInput input;
	input.seq = m_netSeq++;
	input.yaw = m_player.getCamera().getYaw();
	input.pitch = m_player.getCamera().getPitch();
	input.moveFlags = m_netMoveFlags;
	input.sprint = m_netSprint;
	m_network.sendInput(input);
}

void Game::addChatLine(const std::string& sender, const std::string& text) {
	LOG(Level::INFO) << "[" << sender << "] " << text << std::endl;
	m_chatLines.push_back({ sender, text });
	// Keep the HUD history short.
	while (m_chatLines.size() > 8)
		m_chatLines.erase(m_chatLines.begin());
}

void Game::buildRemoteVisuals() {
	m_remoteVisuals.clear();
	for (auto& entry : m_remotePlayers) {
		RemotePlayer& remote = entry.second;
		RemotePlayerVisual visual;
		visual.eyePosition = remote.position;
		visual.yaw = remote.yaw;
		visual.color = playerColor(entry.first);
		m_remoteVisuals.push_back(visual);
	}
}

void Game::openChat() {
	m_chatOpen = true;
	// Stop moving while typing.
	m_netMoveFlags = 0;
	m_netSprint = false;
}

void Game::closeChat() {
	m_chatOpen = false;
	m_chatBuffer.clear();
	m_netMoveFlags = 0;
	m_netSprint = false;
}

void Game::appendChatChar(char c) {
	if (c < 32 || c == 127)
		return; // reject control characters (backspace is handled by backspaceChat)
	if (m_chatBuffer.size() >= 256)
		return;
	m_chatBuffer.push_back(c);
}

void Game::backspaceChat() {
	if (!m_chatBuffer.empty())
		m_chatBuffer.pop_back();
}

void Game::sendChatBuffer() {
	std::string text = m_chatBuffer;
	closeChat();
	if (text.empty())
		return;
	// "@bot <order>" is consumed here, never sent as public chat.
	if (handleBotCommand(text))
		return;
	if (m_online)
		m_network.sendChat(text);
}

void Game::startMcp(uint16_t port) {
	if (m_mcpActive)
		return;
	m_mcp = std::make_unique<McpServer>(port);
	m_mcp->start();
	m_mcpActive = true;
	m_mcpMoveFlags = 0;
	m_mcpSprint = false;
	connectMcpBot();
	LOG(Level::INFO) << "MCP player control enabled on port " << port << std::endl;
}

void Game::stopMcp() {
	if (!m_mcpActive)
		return;
	m_mcp->stop();
	m_mcp.reset();
	m_mcpActive = false;
	m_mcpMoveFlags = 0;
	m_mcpSprint = false;
	m_mcpJumpHeld = false;
	// Drop the MCP bot's connection to the world.
	if (m_mcpNetwork.isConnected())
		m_mcpNetwork.disconnect();
	m_mcpBotOnline = false;
	m_mcpBotId = -1;
	LOG(Level::INFO) << "MCP player control disabled" << std::endl;
}

void Game::connectMcpBot() {
	// The bot is a second client on the same server, so it shows up as a normal remote player.
	if (!m_online || m_mcpNetwork.isConnected())
		return;
	if (!m_mcpNetwork.connect(m_connectHost, m_connectPort)) {
		LOG(Level::WARNING) << "MCP bot could not connect to " << m_connectHost << ":" << m_connectPort << std::endl;
		return;
	}
	m_mcpNetwork.sendHello("MCP Player");
}

void Game::pollMcpNetwork() {
	if (!m_mcpNetwork.isConnected())
		return;
	m_mcpNetwork.poll([this](Protocol::MessageType type, sf::Packet& packet) {
		switch (type) {
		case Protocol::MSG_WELCOME: {
			// MSG_WELCOME layout: version(u32), seed(u32), x(y,z float), timeOfDay(float), selfId(u32).
			uint32_t version, seed, selfId;
			float x, y, z, timeOfDay;
			packet >> version >> seed >> x >> y >> z >> timeOfDay >> selfId;
			if (version != Protocol::PROTOCOL_VERSION) {
				m_mcpNetwork.disconnect();
				m_mcpBotOnline = false;
				return;
			}
			m_mcpBotId = static_cast<int>(selfId);
			m_mcpBotPrevPos = m_mcpBotPos = { x, y, z };
			m_mcpBotOnline = true;
			LOG(Level::INFO) << "MCP bot welcomed as id " << m_mcpBotId << " at " << x << ',' << y << ',' << z << std::endl;
			return;
		}
		case Protocol::MSG_PLAYER_STATE: {
			uint32_t id;
			float x, y, z, yaw, pitch;
			uint8_t onGround;
packet >> id >> x >> y >> z >> yaw >> pitch >> onGround;
			// Filter to the bot's own pose.
			if (static_cast<int>(id) != m_mcpBotId)
				return;
			m_mcpBotPrevPos = m_mcpBotPos;
			// Smooth over a couple of frames so aim/observe read a steady pose, not server ticks.
			// Keep the intended yaw/pitch; the server's copy only feeds the remote-player list.
			m_mcpBotPos = { x, y, z };
			m_mcpBotOnGround = onGround != 0;
			return;
		}
		case Protocol::MSG_BLOCK_EDIT_S:
			// The primary connection already applies these; ignore the bot's copy.
			return;
		case Protocol::MSG_CHAT_S:
			// The primary connection already shows these; ignoring avoids duplicate HUD lines.
			return;
		case Protocol::MSG_DISCONNECT:
			m_mcpBotOnline = false;
			m_mcpNetwork.disconnect();
			m_mcpBotId = -1;
			return;
		default:
			return;
		}
	});
	if (!m_mcpNetwork.isConnected() && m_mcpBotOnline) {
		m_mcpBotOnline = false;
		m_mcpBotId = -1;
	}
}

void Game::sendMcpInput() {
	if (!m_mcpBotOnline)
		return;
	// 30 Hz to match the server tick.
	if (m_mcpBotInputClock.getElapsedTime().asMilliseconds() < 33)
		return;
	m_mcpBotInputClock.restart();
	Protocol::PlayerInput input;
	input.seq = m_mcpBotSeq++;
	input.yaw = m_mcpBotYaw;
	input.pitch = m_mcpBotPitch;
	input.moveFlags = m_mcpMoveFlags;
	input.sprint = m_mcpSprint;
	m_mcpNetwork.sendInput(input);
}

std::optional<ivec3> Game::mcpBotTarget() const {
	// Aiming ray from the bot's eye toward where it looks; returns the first solid block within reach.
	vec3 front{
		cos(glm::radians(m_mcpBotYaw)) * cos(glm::radians(m_mcpBotPitch)),
		sin(glm::radians(m_mcpBotPitch)),
		sin(glm::radians(m_mcpBotYaw)) * cos(glm::radians(m_mcpBotPitch))
	};
	LineBlockFinder finder{ m_mcpBotPos + vec3(0.f, PlayerController::PLAYER_HEAD_HEIGHT, 0.f), front };
	while (finder.getDistance() <= GameServer::REACH) {
		ivec3 pos = finder.next();
		Block block = m_chunkMap.getBlock(pos);
		if (ResManager::blockDatas().get(block.id).isObstacle())
			return pos;
	}
	return std::nullopt;
}

std::optional<ivec3> Game::mcpBotPlace() const {
	// The first air block in front of the aimed-at face (the cell the bot would place against).
	if (!mcpBotTarget().has_value())
		return std::nullopt;
	vec3 front{
		cos(glm::radians(m_mcpBotYaw)) * cos(glm::radians(m_mcpBotPitch)),
		sin(glm::radians(m_mcpBotPitch)),
		sin(glm::radians(m_mcpBotYaw)) * cos(glm::radians(m_mcpBotPitch))
	};
	LineBlockFinder finder{ m_mcpBotPos + vec3(0.f, PlayerController::PLAYER_HEAD_HEIGHT, 0.f), front };
	while (finder.getDistance() <= GameServer::REACH) {
		ivec3 pos = finder.next();
		Block block = m_chunkMap.getBlock(pos);
		if (ResManager::blockDatas().get(block.id).isObstacle())
			break; // reached the aimed-at block; the face is the last air cell before it
		return pos;
	}
	return std::nullopt;
}

void Game::drainMcp(sf::Time) {
	// A `jump` is a short button-press: hold the UP intent for ~250 ms, then release it.
	if (m_mcpJumpHeld && m_mcpJumpClock.getElapsedTime().asMilliseconds() > 250) {
		m_mcpJumpHeld = false;
		m_mcpMoveFlags &= static_cast<uint8_t>(~Protocol::FLAG_UP);
	}

	while (std::unique_ptr<McpServer::Command> cmd = m_mcp->poll()) {
		const std::string& method = cmd->method;
		if (method == "initialize") {
			nlohmann::json result{
				{ "protocolVersion", "2025-03-26" },
				{ "capabilities", { { "tools", nlohmann::json::object() } } },
				{ "serverInfo", { { "name", "voxlord-mcp" }, { "version", "1.0.0" } } }
			};
			if (!cmd->id.is_null())
				m_mcp->sendResult(cmd->id, result);
		} else if (method == "ping") {
			if (!cmd->id.is_null())
				m_mcp->sendResult(cmd->id, nlohmann::json::object());
		} else if (method == "tools/list") {
			static const nlohmann::json tools = nlohmann::json::array({
				nlohmann::json{
					{ "name", "observe" },
					{ "description", "Return the player's snapshot: position, look direction, aimed-at block, "
						"nearby players, recent chat, held block." },
					{ "inputSchema", { { "type", "object" }, { "properties", nlohmann::json::object() } } }
				},
				nlohmann::json{
					{ "name", "look" },
					{ "description", "Set the player's absolute look direction in degrees (yaw 0..360, pitch -90..90)." },
					{ "inputSchema", nlohmann::json{
						{ "type", "object" },
						{ "properties", {
							{ "yaw", { { "type", "number" }, { "description", "yaw degrees, 0 = south, 90 = west" } } },
							{ "pitch", { { "type", "number" }, { "description", "pitch degrees, -90 = straight up" } } }
						} },
						{ "required", nlohmann::json::array({ "yaw" }) }
					} }
				},
				nlohmann::json{
					{ "name", "move" },
					{ "description", "Set which movement directions are held (like key presses). Call again to change; "
						"set all false to stop. Example: {\"forward\":true,\"sprint\":true} walks forward running." },
					{ "inputSchema", nlohmann::json{
						{ "type", "object" },
						{ "properties", {
							{ "forward", { { "type", "boolean" } } },
							{ "backward", { { "type", "boolean" } } },
							{ "left", { { "type", "boolean" } } },
							{ "right", { { "type", "boolean" } } },
							{ "up", { { "type", "boolean" } } },
							{ "down", { { "type", "boolean" } } },
							{ "sprint", { { "type", "boolean" } } }
						} }
					} }
				},
				nlohmann::json{
					{ "name", "jump" },
					{ "description", "Perform a short jump. No arguments." },
					{ "inputSchema", { { "type", "object" }, { "properties", nlohmann::json::object() } } }
				},
				nlohmann::json{
					{ "name", "chat" },
					{ "description", "Send a chat message to the server (broadcast to all players)." },
					{ "inputSchema", nlohmann::json{
						{ "type", "object" },
						{ "properties", { { "text", { { "type", "string" } } } } },
						{ "required", nlohmann::json::array({ "text" }) }
					} }
				},
				nlohmann::json{
					{ "name", "break_block" },
					{ "description", "Break (remove) the block currently aimed at. Uses observe()'s target, so look at a "
						"block first. No arguments." },
					{ "inputSchema", { { "type", "object" }, { "properties", nlohmann::json::object() } } }
				},
				nlohmann::json{
					{ "name", "place_block" },
					{ "description", "Place a block at the face in front of the aimed-at block. type accepts an index "
						"(0 AIR .. 25) or a name like \"STONE\", \"DIRT\", \"GRASS\", \"SAND\", \"LOG\"." },
					{ "inputSchema", nlohmann::json{
						{ "type", "object" },
						{ "properties", { { "type", { { "type", "string" }, { "description", "block type name or index" } } } } },
						{ "required", nlohmann::json::array({ "type" }) }
					} }
				}
			});
			if (!cmd->id.is_null())
				m_mcp->sendResult(cmd->id, nlohmann::json{ { "tools", tools } });
		} else if (method == "tools/call") {
			std::string name = cmd->params.value("name", "");
			nlohmann::json args = cmd->params.value("arguments", nlohmann::json::object());
			nlohmann::json result = mcpToolResult(name, args);
			result["_tool"] = name;
			if (!cmd->id.is_null())
				m_mcp->sendResult(cmd->id, result);
		} else {
			if (!cmd->id.is_null())
				m_mcp->sendError(cmd->id, -32601, "Method not found: " + method);
		}
	}
}

// Map a JSON value (int index or block name) to a BlockID. Returns std::nullopt on garbage.
static std::optional<BlockID> mcpBlockId(const nlohmann::json& v) {
	if (v.is_number_integer()) {
		int n = v.get<int>();
		if (n >= 0 && n < static_cast<int>(BlockID::SIZE))
			return BlockID::_from_integral(n);
		return std::nullopt;
	}
	if (v.is_string()) {
		std::string name = v.get<std::string>();
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
		try {
			BlockID id = BlockID::_from_string(name.c_str());
			return id == +BlockID::AIR ? std::nullopt : std::optional<BlockID>{ id };
		} catch (const std::runtime_error&) {
			return std::nullopt;
		}
	}
	return std::nullopt;
}

nlohmann::json Game::mcpToolResult(const std::string& name, const nlohmann::json& args) {
	nlohmann::json result;
	result["isError"] = false;
	nlohmann::json payload;

	if (name == "observe") {
		payload = mcpObserve();
	} else if (name == "look") {
		float yaw = args.value("yaw", m_mcpBotYaw);
		float pitch = args.value("pitch", m_mcpBotPitch);
		m_mcpBotYaw = static_cast<float>(fmod(yaw + 360.f, 360.f));
		m_mcpBotPitch = std::clamp(pitch, -89.9f, 89.9f);
		payload = mcpObserve();
	} else if (name == "move") {
		uint8_t flags = 0;
		if (args.value("forward", false)) flags |= Protocol::FLAG_FORWARD;
		if (args.value("backward", false)) flags |= Protocol::FLAG_BACKWARD;
		if (args.value("left", false)) flags |= Protocol::FLAG_LEFT;
		if (args.value("right", false)) flags |= Protocol::FLAG_RIGHT;
		if (args.value("up", false)) flags |= Protocol::FLAG_UP;
		if (args.value("down", false)) flags |= Protocol::FLAG_DOWN;
		m_mcpMoveFlags = flags;
		m_mcpSprint = args.value("sprint", false);
		payload = mcpObserve();
	} else if (name == "jump") {
		m_mcpMoveFlags |= Protocol::FLAG_UP;
		m_mcpJumpHeld = true;
		m_mcpJumpClock.restart();
		payload = mcpObserve();
	} else if (name == "chat") {
		std::string text = args.value("text", "");
		if (!text.empty()) {
			if (m_mcpBotOnline)
				m_mcpNetwork.sendChat(text);
			else
				addChatLine(m_mcpBotName, text);
		}
		payload["sent"] = !text.empty() && m_mcpBotOnline;
	} else if (name == "break_block") {
		std::optional<ivec3> target = mcpBotTarget();
		if (target.has_value()) {
			ivec3 pos = target.value();
			m_mcpNetwork.sendBlockEdit(pos, +BlockID::AIR);
			payload["ok"] = true;
			payload["block"] = { pos.x, pos.y, pos.z };
		} else {
			payload["ok"] = false;
			payload["reason"] = "no block aimed at; use look to aim then observe to confirm a target";
		}
	} else if (name == "place_block") {
		std::optional<BlockID> id = mcpBlockId(args.value("type", nlohmann::json("")));
		if (!id.has_value()) {
			payload["ok"] = false;
			payload["reason"] = "invalid block type";
		} else {
			std::optional<ivec3> place = mcpBotPlace();
			if (place.has_value()) {
				m_mcpNetwork.sendBlockEdit(place.value(), *id);
				m_mcpHeldBlock = *id;
				payload["ok"] = true;
				payload["block"] = { place.value().x, place.value().y, place.value().z };
			} else {
				payload["ok"] = false;
				payload["reason"] = "nothing to place against (aim at a block face first)";
			}
		}
	} else {
		result["isError"] = true;
		payload["error"] = "Unknown tool: " + name;
	}

	result["content"] = nlohmann::json::array({
		nlohmann::json{ { "type", "text" }, { "text", payload.dump() } }
	});
	return result;
}

nlohmann::json Game::mcpObserve() const {
	nlohmann::json obs;
	vec3 pos = m_mcpBotPos;
	obs["position"] = { pos.x, pos.y, pos.z };
	obs["yaw"] = m_mcpBotYaw;
	obs["pitch"] = m_mcpBotPitch;
	obs["velocity"] = { 0.f, 0.f, 0.f };
	obs["on_ground"] = m_mcpBotOnGround;
	obs["sprinting"] = m_mcpSprint;
	obs["online"] = m_mcpBotOnline;
	obs["self_id"] = m_mcpBotId;
	obs["time_of_day"] = m_dayCycle.getTimeOfDay();

	std::optional<ivec3> botTarget = mcpBotTarget();
	if (botTarget.has_value()) {
		ivec3 t = botTarget.value();
		Block block = m_chunkMap.getBlock(t);
		obs["target"] = {
			{ "block", block.id._to_string() },
			{ "id", static_cast<int>(block.id) },
			{ "position", { t.x, t.y, t.z } },
			{ "distance", glm::length(vec3(t) + vec3(0.5f) - pos) }
		};
	} else {
		obs["target"] = nullptr;
	}
	if (m_mcpHeldBlock != +BlockID::AIR)
		obs["held_block"] = m_mcpHeldBlock._to_string();
	else
		obs["held_block"] = "AIR";

	nlohmann::json players = nlohmann::json::array();
	for (const auto& entry : m_remotePlayers) {
		const RemotePlayer& r = entry.second;
		nlohmann::json p;
		p["id"] = entry.first;
		p["name"] = r.name;
		p["position"] = { r.position.x, r.position.y, r.position.z };
		p["yaw"] = r.yaw;
		p["pitch"] = r.pitch;
		p["on_ground"] = r.onGround;
		players.push_back(std::move(p));
	}
	obs["players"] = players;

	nlohmann::json chat = nlohmann::json::array();
	for (const ChatLine& line : m_chatLines) {
		nlohmann::json l;
		l["sender"] = line.sender;
		l["text"] = line.text;
		chat.push_back(std::move(l));
	}
	obs["chat"] = chat;
	return obs;
}

namespace {
	// Helpers for the in-game bot command grammar.
	std::string toLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}
	std::vector<std::string> splitWords(const std::string& s) {
		std::vector<std::string> words;
		std::stringstream ss(s);
		std::string word;
		while (ss >> word)
			words.push_back(word);
		return words;
	}
	// Cardinal direction words map onto yaw so "go north" works like "go there".
	bool cardinalYaw(const std::string& w, float& yaw) {
		if (w == "north") { yaw = 0.f; return true; }
		if (w == "east") { yaw = 90.f; return true; }
		if (w == "south") { yaw = 180.f; return true; }
		if (w == "west") { yaw = 270.f; return true; }
		if (w == "northeast") { yaw = 45.f; return true; }
		if (w == "northwest") { yaw = 315.f; return true; }
		if (w == "southeast") { yaw = 135.f; return true; }
		if (w == "southwest") { yaw = 225.f; return true; }
		return false;
	}
	int parseCount(const std::vector<std::string>& words) {
		static const std::unordered_map<std::string, int> counts{
			{ "a", 1 }, { "one", 1 }, { "two", 2 }, { "couple", 2 }, { "three", 3 }, { "few", 3 },
			{ "four", 4 }, { "some", 4 }, { "five", 5 }, { "six", 6 }, { "seven", 7 }, { "eight", 8 },
			{ "bunch", 8 }, { "nine", 9 }, { "ten", 10 }, { "many", 12 }, { "lot", 12 }, { "lots", 12 },
			{ "stack", 16 }
		};
		for (const std::string& w : words) {
			if (counts.count(w))
				return counts.at(w);
			if (!w.empty() && std::all_of(w.begin(), w.end(), ::isdigit))
				return std::atoi(w.c_str());
		}
		return 1;
	}
}

bool Game::handleBotCommand(const std::string& text) {
	std::string t = toLower(text);
	t.erase(0, t.find_first_not_of(" \t"));
	t.erase(t.find_last_not_of(" \t") + 1);
	if (t.empty())
		return false;

	// A bare directive only becomes a command when the bot is online, so "mining"
	// chat between two humans is not swallowed by accident.
	static const std::unordered_map<std::string, bool> handles{
		{ "@bot", true }, { "@mcp", true }, { "/bot", true }, { "/mcp", true },
		{ "bot:", true }, { "bot,", true }, { "mcp:", true }, { "mcp,", true },
		{ "!bot", true }, { "!mcp", true }, { "hey bot", true }, { "hey mcp", true }
	};
	bool directed = false;
	for (const auto& [handle, flag] : handles) {
		if (t.rfind(handle, 0) == 0) {
			directed = true;
			t = t.substr(handle.size());
			t.erase(0, t.find_first_not_of(" \t"));
			break;
		}
	}

	static const std::unordered_set<std::string> verbs{
		"go", "walk", "run", "move", "come", "travel", "follow", "stop", "halt", "wait",
		"stay", "stand", "freeze", "mine", "dig", "break", "chop", "harvest", "gather",
		"collect", "place", "put", "build", "lay", "set", "look", "turn", "face", "jump",
		"help", "scout", "return", "comeback", "followme"
	};
	std::vector<std::string> words = splitWords(t);
	if (words.empty())
		return false;
	if (!directed && !verbs.count(words[0]))
		return false; // not a bot command (no @bot handle, no directive verb)
	if (!m_mcpBotOnline)
		return false; // bot not connected; let it go out as normal chat

	// Tally an audience of the local aimed block and player position for spatial words.
	if (words[0] == "help") {
		addChatLine(m_mcpBotName, "Commands: go there / come here / follow me / mine stone | mine that / "
			"place stone (there) / look at me / stop / jump. Example: \"bot mine some stone\".");
		return true;
	}
	if (words[0] == "jump") {
		m_mcpMoveFlags |= Protocol::FLAG_UP;
		m_mcpJumpHeld = true;
		m_mcpJumpClock.restart();
		addChatLine(m_mcpBotName, "Hop!");
		return true;
	}

	// Resolve the two spatial anchors BEFORE building the task, so "there" always means
	// where the local player is currently aiming.
	vec3 there = m_mcpBotPos;      // fallback: if the player isn't aiming at anything, go nowhere useful
	vec3 here = m_player.getPosition();
	if (m_player.getTarget().has_value()) {
		ivec3 aim = m_player.getTarget().value();
		there = vec3(aim) + vec3(0.5f);
	} else {
		vec3 front = m_player.getCamera().getFront();
		there = here + front * 6.f;
	}

	BotTask task;
	if (words[0] == "stop" || words[0] == "halt" || words[0] == "wait" || words[0] == "stay"
		|| words[0] == "stand" || words[0] == "freeze" || words[0] == "return" || words[0] == "comeback") {
		task.kind = BotTaskKind::STOP;
		m_mcpMoveFlags = 0;
		m_mcpSprint = false;
		m_botTaskActive = false;
		m_botTask = BotTask{}; // clear any running task
		addChatLine(m_mcpBotName, "Stopped.");
		return true;
	}

	task.kind = BotTaskKind::GO;
	task.sprint = words[0] == "run";
	bool spatialSeen = false;
	// "follow" wins as a mode: the bot keeps re-aiming at the player every frame.
	if (std::find(words.begin(), words.end(), "follow") != words.end()
		|| words[0] == "followme") {
		task.target = here;
		task.follow = true;
		task.sprint = true;
		spatialSeen = true;
	}
	for (const std::string& w : words) {
		float yaw;
		if (!task.follow && (w == "here" || w == "me" || w == "player")) {
			task.target = here;
			task.follow = false;
			spatialSeen = true;
		} else if (!task.follow && (w == "there" || w == "aim" || w == "that" || w == "target" || w == "point")) {
			task.target = there;
			task.follow = false;
			spatialSeen = true;
		}
		if (cardinalYaw(w, yaw) && (words[0] == "go" || words[0] == "walk" || words[0] == "run" || words[0] == "move")) {
			vec3 ahead{ cos(glm::radians(yaw)), 0.f, sin(glm::radians(yaw)) };
			task.target = m_mcpBotPos + ahead * 4.f;
			task.follow = false;
			spatialSeen = true;
		}
	}
	if (!spatialSeen) {
		// Bare imperative with no spatial word: default to where the player is aiming.
		task.target = there;
		task.follow = false;
	}

	// Mining / placing / looking are one-shot actions with an explicit kind; finish this block
	// with the parsed block type when the user actually ordered an interaction.
	bool isInteraction = false;
	for (const std::string& w : words) {
		if (w == "mine" || w == "dig" || w == "break" || w == "chop" || w == "harvest"
			|| w == "gather" || w == "collect") {
			task.kind = BotTaskKind::MINE;
			isInteraction = true;
			break;
		}
		if (w == "place" || w == "put" || w == "build" || w == "lay" || w == "set") {
			task.kind = BotTaskKind::PLACE;
			isInteraction = true;
			break;
		}
		if (w == "look" || w == "turn" || w == "face") {
			task.kind = BotTaskKind::LOOK;
			isInteraction = true;
			break;
		}
	}
	if (isInteraction) {
		if (task.kind == BotTaskKind::LOOK) {
			// Look at a spatial hint, or wherever the player is aiming by default.
			bool toHere = std::find(words.begin(), words.end(), "here") != words.end()
				|| std::find(words.begin(), words.end(), "me") != words.end();
			task.target = toHere ? here : there;
		} else {
			BlockID wanted = parseBlockName(words.back());
			if (task.kind == BotTaskKind::MINE) {
				task.blockId = (wanted == +BlockID::AIR) ? +BlockID::AIR : wanted;
				if (task.blockId == +BlockID::AIR) {
					// "mine that": mine where the player is aiming.
					if (m_player.getTarget().has_value())
						task.block = m_player.getTarget().value();
					else if (mcpBotTarget().has_value())
						task.block = mcpBotTarget().value();
					else {
						addChatLine(m_mcpBotName, "I can't see anything to mine there.");
						processBotCommand(BotTask{ BotTaskKind::STOP });
						return true;
					}
				}
				m_botMineCount = 0;
				if (wanted != +BlockID::AIR)
					m_botMineGoal = parseCount(words);
				else
					m_botMineGoal = 1;
			} else {
				task.blockId = (wanted == +BlockID::AIR) ? +BlockID::STONE : wanted;
				if (m_player.getPlacePos().has_value())
					task.block = m_player.getPlacePos().value();
				else if (mcpBotPlace().has_value())
					task.block = mcpBotPlace().value();
				else {
					addChatLine(m_mcpBotName, "I don't see a spot to build at.");
					processBotCommand(BotTask{ BotTaskKind::STOP });
					return true;
				}
			}
		}
	}

	processBotCommand(task);
	return true;
}

void Game::processBotCommand(BotTask task) {
	m_botTask = task;
	m_botTaskActive = true;
	m_botLastMine = ivec3{ -1 };
	addChatLine(m_mcpBotName, "On it!");
}

void Game::steerBotLookAt(const vec3& point) {
	vec3 eye = m_mcpBotPos + vec3(0.f, PlayerController::PLAYER_HEAD_HEIGHT, 0.f);
	vec3 d = point - eye;
	float horiz = glm::length(vec2(d.x, d.z));
	m_mcpBotYaw = glm::degrees(std::atan2(d.z, d.x));
	m_mcpBotPitch = glm::degrees(std::atan2(d.y, horiz));
	m_mcpBotYaw = static_cast<float>(fmod(m_mcpBotYaw + 360.f, 360.f));
	m_mcpBotPitch = std::clamp(m_mcpBotPitch, -89.9f, 89.9f);
}

void Game::steerBotToward(const vec3& target, bool sprint, float stopDistance) {
	vec3 d = target - m_mcpBotPos;
	float horiz = glm::length(vec2(d.x, d.z));
	m_mcpBotYaw = glm::degrees(std::atan2(d.z, d.x));
	m_mcpBotYaw = static_cast<float>(fmod(m_mcpBotYaw + 360.f, 360.f));
	if (horiz > stopDistance) {
		m_mcpMoveFlags |= Protocol::FLAG_FORWARD;
		m_mcpSprint = sprint;
	} else {
		m_mcpMoveFlags &= static_cast<uint8_t>(~Protocol::FLAG_FORWARD);
		m_mcpSprint = false;
	}
}

void Game::maybeHopObstacle() {
	if (!m_mcpBotOnGround)
		return;
	std::optional<ivec3> hit = mcpBotTarget();
	if (!hit.has_value())
		return;
	vec3 center = vec3(hit.value()) + vec3(0.5f);
	float dist = glm::length(center - (m_mcpBotPos + vec3(0.f, PlayerController::PLAYER_HEAD_HEIGHT, 0.f)));
	// A wall right in front (not the floor and not a far target) -> hop it.
	if (dist < 2.2f && center.y > m_mcpBotPos.y - 0.5f)
		m_mcpMoveFlags |= Protocol::FLAG_UP;
}

std::optional<ivec3> Game::findNearestBlock(BlockID id, float maxRadius) const {
	ivec3 bot = Converter::globalPosToBlock(m_mcpBotPos);
	std::optional<ivec3> best;
	float bestDist = 1e9f;
	int rad = static_cast<int>(std::ceil(maxRadius));
	for (int y = bot.y - 5; y <= bot.y + 5; ++y) {
		for (int z = bot.z - rad; z <= bot.z + rad; ++z) {
			for (int x = bot.x - rad; x <= bot.x + rad; ++x) {
				ivec3 p{ x, y, z };
				if (m_chunkMap.getBlock(p).id != id)
					continue;
				float dist = glm::length(vec3(p) + vec3(0.5f) - m_mcpBotPos);
				if (dist < bestDist) {
					bestDist = dist;
					best = p;
				}
			}
		}
	}
	return best;
}

BlockID Game::parseBlockName(const std::string& word) const {
	static const std::unordered_map<std::string, BlockID> names{
		{ "stone", +BlockID::STONE }, { "rock", +BlockID::STONE }, { "cobble", +BlockID::STONE },
		{ "dirt", +BlockID::DIRT }, { "soil", +BlockID::DIRT }, { "earth", +BlockID::DIRT }, { "mud", +BlockID::DIRT },
		{ "grass", +BlockID::GRASS }, { "sand", +BlockID::SAND },
		{ "water", +BlockID::WATER }, { "lava", +BlockID::LAVA },
		{ "log", +BlockID::LOG }, { "wood", +BlockID::LOG }, { "tree", +BlockID::LOG }, { "trunk", +BlockID::LOG },
		{ "leaves", +BlockID::LEAVES }, { "leaf", +BlockID::LEAVES },
		{ "darkwood", +BlockID::DARK_LOG }, { "darklog", +BlockID::DARK_LOG },
		{ "darkleaves", +BlockID::DARK_LEAVES },
		{ "snow", +BlockID::SNOW }, { "ice", +BlockID::ICE }, { "cactus", +BlockID::CACTUS },
		{ "light", +BlockID::LIGHT }, { "torch", +BlockID::LIGHT }, { "lamp", +BlockID::LIGHT }
	};
	auto it = names.find(toLower(word));
	if (it != names.end())
		return it->second;
	return +BlockID::AIR;
}

void Game::updateBotTask(sf::Time) {
	if (!m_botTaskActive || !m_mcpBotOnline)
		return;

	switch (m_botTask.kind) {
	case BotTaskKind::STOP: {
		m_mcpMoveFlags = 0;
		m_mcpSprint = false;
		m_botTaskActive = false;
		m_botTask.kind = BotTaskKind::NONE;
		return;
	}
	case BotTaskKind::GO: {
		if (m_botTask.follow)
			m_botTask.target = m_player.getPosition();
		vec3 d = m_botTask.follow ? (m_botTask.target - m_mcpBotPos) : (m_botTask.target - m_mcpBotPos);
		if (glm::length(d) < 1.2f) {
			if (m_botTask.follow) {
				// Follow: hang out near the player without ending the task.
				m_mcpMoveFlags &= static_cast<uint8_t>(~Protocol::FLAG_FORWARD);
				m_mcpSprint = false;
				return;
			}
			m_botTaskActive = false;
			m_botTask.kind = BotTaskKind::NONE;
			addChatLine(m_mcpBotName, "I'm here!");
			return;
		}
		steerBotToward(m_botTask.target, m_botTask.sprint || d.x * d.x + d.z * d.z > 36.f);
		maybeHopObstacle();
		return;
	}
	case BotTaskKind::MINE: {
		if (m_botTask.blockId != +BlockID::AIR) {
			// Mine a count of the requested material: find the nearest remaining block each pass.
			if (m_botLastMine != ivec3{ -1 } && m_botMineCount >= m_botMineGoal) {
				m_botTaskActive = false;
				m_botTask.kind = BotTaskKind::NONE;
				addChatLine(m_mcpBotName, "All done mining.");
				return;
			}
			std::optional<ivec3> next = findNearestBlock(m_botTask.blockId, GameServer::REACH);
			if (!next.has_value()) {
				if (m_botMineCount > 0) {
					m_botTaskActive = false;
					m_botTask.kind = BotTaskKind::NONE;
					addChatLine(m_mcpBotName, "Couldn't find more of that nearby.");
					return;
				}
				m_botTaskActive = false;
				m_botTask.kind = BotTaskKind::NONE;
				addChatLine(m_mcpBotName, "I can't find any of that around here.");
				return;
			}
			// Skip the block we just mined until the broadcast clears it client-side.
			if (next.value() == m_botLastMine)
				return;
			m_botTask.block = next.value();
		} else if (m_botTask.block == ivec3{ 0, 0, 0 }) {
			m_botTaskActive = false;
			m_botTask.kind = BotTaskKind::NONE;
			addChatLine(m_mcpBotName, "Nothing to mine there.");
			return;
		}

		// Walk to within reach, then break the block from the server's perspective.
		vec3 targetCenter = vec3(m_botTask.block) + vec3(0.5f);
		float dist = glm::length(targetCenter - m_mcpBotPos);
		steerBotLookAt(targetCenter);
		if (dist <= GameServer::REACH) {
			m_mcpNetwork.sendBlockEdit(m_botTask.block, +BlockID::AIR);
			if (m_botTask.blockId != +BlockID::AIR) {
				// The server broadcasts the edit to this client too; let it settle before hunting next.
			m_botLastMine = m_botTask.block;
				++m_botMineCount;
				if (m_botMineCount >= m_botMineGoal) {
					m_botTaskActive = false;
					m_botTask.kind = BotTaskKind::NONE;
					addChatLine(m_mcpBotName, "Mined it.");
				}
				return;
			}
			m_botTaskActive = false;
			m_botTask.kind = BotTaskKind::NONE;
			addChatLine(m_mcpBotName, "Mined!");
			return;
		}
		steerBotToward(targetCenter, false, 2.5f);
		maybeHopObstacle();
		return;
	}
	case BotTaskKind::PLACE: {
		vec3 targetCenter = vec3(m_botTask.block) + vec3(0.5f);
		float dist = glm::length(targetCenter - m_mcpBotPos);
		steerBotLookAt(targetCenter);
		if (dist <= GameServer::REACH) {
			m_mcpNetwork.sendBlockEdit(m_botTask.block, m_botTask.blockId);
			m_mcpHeldBlock = m_botTask.blockId;
			m_botTaskActive = false;
			m_botTask.kind = BotTaskKind::NONE;
			addChatLine(m_mcpBotName, "Placed!");
			return;
		}
		steerBotToward(targetCenter, false, 2.5f);
		maybeHopObstacle();
		return;
	}
	case BotTaskKind::LOOK: {
		vec3 target = m_botTask.target;
		steerBotLookAt(target);
		m_botTaskActive = false;
		m_botTask.kind = BotTaskKind::NONE;
		return;
	}
	case BotTaskKind::NONE:
		return;
	}
}

void Game::askOllama(const std::string& text) {
	if (m_ollamaQuestionPending)
		return;
	m_ollamaQuestionPending = true;
	std::thread([this, text]() {
		nlohmann::json req{
			{ "model", "qwen2.5:3b" },
			{ "stream", false },
			{ "format", "json" },
			{ "prompt",
				"You control the helper bot in the voxel game VoxLord. The player typed: \"" + text
				+ "\". Decide the single best action and reply with ONLY a JSON object using this shape: "
				"{ \"action\": \"go\"|\"mine\"|\"place\"|\"look\"|\"stop\"|\"jump\"|\"follow\", "
				"\"target\": \"here\"|\"there\"|\"\", \"block\": \"stone\"|\"dirt\"|\"sand\"|\"log\"|\"leaves\"|\"water\"|"
				"\"lava\"|\"snow\"|\"ice\"|\"cactus\"|\"light\"|\"grass\"|\"\" , \"count\": 1 }. "
				"\"here\" = the player's position, \"there\" = where the player is looking. "
				"If a block type is not mentioned, leave block empty. No prose, no markdown, JSON only." }
		};
		std::string result;
		try {
			sf::Http http("127.0.0.1", 11434);
			sf::Http::Request request("/api/generate", sf::Http::Request::Post, req.dump());
			request.setField("Content-Type", "application/json");
			sf::Http::Response response = http.sendRequest(request, sf::seconds(60.f));
			if (response.getStatus() == sf::Http::Response::Ok) {
				nlohmann::json body = nlohmann::json::parse(response.getBody());
				result = body.value("response", "");
			}
		} catch (const std::exception& e) {
			result = "";
		}
		{
			std::lock_guard<std::mutex> lock(m_ollamaMutex);
			m_ollamaResult = result;
			m_ollamaResultReady = true;
			m_ollamaQuestionPending = false;
		}
	}).detach();
}

void Game::applyOllamaResult() {
	if (!m_ollamaResultReady)
		return;
	std::string result;
	{
		std::lock_guard<std::mutex> lock(m_ollamaMutex);
		result = m_ollamaResult;
		m_ollamaResultReady = false;
	}
	if (result.empty()) {
		addChatLine(m_mcpBotName, "I couldn't understand that (is ollama running?).");
		return;
	}
	try {
		nlohmann::json parsed = nlohmann::json::parse(result);
		std::string action = parsed.value("action", "");
		BotTask task;
		if (action == "go" || action == "walk") {
			task.kind = BotTaskKind::GO;
			vec3 here = m_player.getPosition();
			vec3 there = m_mcpBotPos;
			if (m_player.getTarget().has_value())
				there = vec3(m_player.getTarget().value()) + vec3(0.5f);
			std::string target = parsed.value("target", "");
			task.target = (target == "here" || target == "me") ? here : there;
		} else if (action == "follow") {
			task.kind = BotTaskKind::GO;
			task.target = m_player.getPosition();
			task.follow = true;
			task.sprint = true;
		} else if (action == "mine" || action == "dig" || action == "break") {
			task.kind = BotTaskKind::MINE;
			std::string block = parsed.value("block", "");
			if (!block.empty()) {
				task.blockId = parseBlockName(block);
				if (task.blockId == +BlockID::AIR)
					task.blockId = +BlockID::STONE;
				m_botMineGoal = std::max(1, static_cast<int>(parsed.value("count", 1)));
				m_botMineCount = 0;
			} else if (m_player.getTarget().has_value()) {
				task.block = m_player.getTarget().value();
			} else if (mcpBotTarget().has_value()) {
				task.block = mcpBotTarget().value();
			} else {
				addChatLine(m_mcpBotName, "I can't see what to mine.");
				return;
			}
		} else if (action == "place" || action == "build" || action == "put") {
			task.kind = BotTaskKind::PLACE;
			task.blockId = parseBlockName(parsed.value("block", ""));
			if (task.blockId == +BlockID::AIR)
				task.blockId = +BlockID::STONE;
			if (m_player.getPlacePos().has_value())
				task.block = m_player.getPlacePos().value();
			else if (mcpBotPlace().has_value())
				task.block = mcpBotPlace().value();
			else {
				addChatLine(m_mcpBotName, "I don't see where to build.");
				return;
			}
		} else if (action == "look") {
			task.kind = BotTaskKind::LOOK;
			vec3 here = m_player.getPosition();
			std::string target = parsed.value("target", "");
			if (target == "there" && m_player.getTarget().has_value())
				task.target = vec3(m_player.getTarget().value()) + vec3(0.5f);
			else
				task.target = here;
		} else if (action == "jump") {
			m_mcpMoveFlags |= Protocol::FLAG_UP;
			m_mcpJumpHeld = true;
			m_mcpJumpClock.restart();
			addChatLine(m_mcpBotName, "Hop!");
			return;
		} else if (action == "stop" || action == "halt") {
			task.kind = BotTaskKind::STOP;
			m_mcpMoveFlags = 0;
			m_mcpSprint = false;
		} else {
			addChatLine(m_mcpBotName, "I didn't catch that.");
			return;
		}
		processBotCommand(task);
	} catch (const std::exception& e) {
		addChatLine(m_mcpBotName, "I couldn't understand that.");
	}
}

void Game::endBotTask(const std::string& message, bool ok) {
	m_botTaskActive = false;
	m_botTask.kind = BotTaskKind::NONE;
	addChatLine(m_mcpBotName, ok ? message : "Stopped: " + message);
}


