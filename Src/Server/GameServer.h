#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <SFML/Network.hpp>

#include "network/Protocol.h"
#include "server/ServerWorld.h"
#include "physics/PlayerController.h"

// One connected player's server-side state: the authoritative position driven by the shared
// PlayerController, plus the latest input snapshot from the client.
struct PlayerEntity {
	int id = 0;
	std::string name;
	PlayerController controller;
	vec3 position{ 0.f, static_cast<float>(Const::SPAWN_Y), 0.f };
	float yaw{ -90.f };
	float pitch{ 0.f };
	bool onGround = false;
	Protocol::PlayerInput latestInput;
};

// The authoritative game server: deterministic world, fixed-tick physics, TCP transport.
// One codebase serves both the dedicated `voxlord-server` binary and the embedded host's
// server thread. Not thread-safe; run() owns all of it on a single thread.
class GameServer {
public:
	static constexpr float TICK_RATE = 30.f;
	static constexpr float TICK_TIME = 1.f / TICK_RATE;
	static constexpr float REACH = 8.f;
	static constexpr int EDIT_COOLDOWN_MS = 150;
	static constexpr int WORLD_RADIUS_CHUNKS = 8;

	explicit GameServer(uint16_t port = Protocol::DEFAULT_PORT);
	~GameServer();

	// Binds the listener. Returns false if the port is already taken.
	bool start();
	// Asks run() to exit and disconnects everyone.
	void stop();
	// Blocking server loop: accept, receive, fixed-tick, broadcast. Dedicated binary or host thread.
	void run();

	uint16_t port() const { return m_port; }

private:
	struct Client {
		sf::TcpSocket socket;
		std::unique_ptr<PlayerEntity> player;
		sf::Clock lastEditClock;
		bool welcomeSent = false;
		bool dead = false;
	};

	void acceptNewClients();
	void receiveFrom(Client& client);
	void handlePacket(Client& client, sf::Packet& packet);
	void handleBlockEdit(Client& client, int x, int y, int z, uint8_t blockType);
	void handleBulkEdit(Client& client, int cx, int cy, int cz, uint16_t radius, uint8_t blockType);
	void pruneDeadClients();
	void tick();
	void applyInput(PlayerEntity& player, float dt);
	void broadcast(sf::Packet& packet, const Client* except = nullptr);
	void broadcastPlayerStates();
	void sendWelcome(Client& client);
	void disconnectAll(const std::string& reason);

	uint16_t m_port;
	sf::TcpListener m_listener;
	bool m_started = false;
	std::atomic<bool> m_stop{ false };
	std::vector<std::unique_ptr<Client>> m_clients;
	ServerWorld m_world;
	float m_timeOfDay = 0.25f;
	uint32_t m_nextPlayerId = 1;
	sf::Clock m_tickClock;
	float m_accumulator = 0.f;
	int m_tickCounter = 0;
};