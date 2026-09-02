#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <SFML/Network.hpp>

#include "Block/BlockID.h"
#include "Maths/GlmCommon.h"
#include "Network/Protocol.h"

// TCP connection to a GameServer. Non-blocking after connect(); the game thread drains received
// messages with poll() and sends the small, frame-rate message set directly.
class NetworkClient {
public:
	// Blocking connect with a 5s timeout; socket turns non-blocking on success.
	bool connect(const std::string& host, uint16_t port = Protocol::DEFAULT_PORT);
	void disconnect();

	bool isConnected() const { return m_connected; }

	void sendHello(const std::string& name);
	void sendInput(const Protocol::PlayerInput& input);
	void sendBlockEdit(ivec3 pos, BlockID id);
	void sendBulkEdit(ivec3 center, uint16_t radius, BlockID id);
	void sendChat(const std::string& text);

	// Drain everything buffered, calling onMessage(type, packet) for each.
	void poll(const std::function<void(Protocol::MessageType, sf::Packet&)>& onMessage);

private:
	sf::TcpSocket m_socket;
	bool m_connected = false;
};