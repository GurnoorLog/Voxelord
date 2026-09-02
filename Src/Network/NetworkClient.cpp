#include "Network/NetworkClient.h"

#include <stdexcept>

bool NetworkClient::connect(const std::string& host, uint16_t port) {
	std::string address = host;
	std::size_t colon = address.rfind(':');
	if (colon != std::string::npos) {
		try {
			port = static_cast<uint16_t>(std::stoi(address.substr(colon + 1)));
		} catch (const std::exception&) {
			// Non-numeric suffix: leave the default port.
		}
		address = address.substr(0, colon);
	}

	m_socket.setBlocking(true);
	sf::Socket::Status status = m_socket.connect(sf::IpAddress(address), port, sf::seconds(5.f));
	if (status != sf::Socket::Done) {
		m_connected = false;
		return false;
	}
	m_socket.setBlocking(false);
	m_connected = true;
	return true;
}

void NetworkClient::disconnect() {
	if (m_connected)
		m_socket.disconnect();
	m_connected = false;
}

void NetworkClient::sendHello(const std::string& name) {
	sf::Packet p = Protocol::makePacket(Protocol::MSG_HELLO);
	p << name;
	m_socket.send(p);
}

void NetworkClient::sendInput(const Protocol::PlayerInput& input) {
	sf::Packet p = Protocol::inputPacket(input);
	m_socket.send(p);
}

void NetworkClient::sendBlockEdit(ivec3 pos, BlockID id) {
	sf::Packet p = Protocol::blockEditPacket(pos.x, pos.y, pos.z, static_cast<uint8_t>(id._to_integral()));
	m_socket.send(p);
}

void NetworkClient::sendBulkEdit(ivec3 center, uint16_t radius, BlockID id) {
	sf::Packet p = Protocol::bulkEditPacket(center.x, center.y, center.z, radius, static_cast<uint8_t>(id._to_integral()));
	m_socket.send(p);
}

void NetworkClient::sendChat(const std::string& text) {
	sf::Packet p = Protocol::chatPacket(text);
	m_socket.send(p);
}

void NetworkClient::poll(const std::function<void(Protocol::MessageType, sf::Packet&)>& onMessage) {
	if (!m_connected)
		return;
	sf::Packet packet;
	while (true) {
		sf::Socket::Status status = m_socket.receive(packet);
		if (status == sf::Socket::Done) {
			Protocol::MessageType type;
			if (Protocol::readType(packet, type))
				onMessage(type, packet);
			packet.clear();
		} else if (status == sf::Socket::NotReady || status == sf::Socket::Partial) {
			// NotReady: nothing buffered. Partial: the socket still holds an unfinished packet
			// (SFML 2.6 buffers it internally). Neither means the connection is gone.
			break;
		} else {
			// Socket closed or an I/O error: the connection is gone.
			m_connected = false;
			break;
		}
	}
}