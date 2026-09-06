#pragma once

#include <cstdint>
#include <string>

#include <SFML/Network.hpp>

// Wire protocol between a VoxLord client and server (TCP, length-prefixed sf::Packet).
// Bump PROTOCOL_VERSION whenever the message layout changes, so a mismatch is a clean disconnect,
// not silent desync.

namespace Protocol {
	constexpr uint32_t PROTOCOL_VERSION = 2;
	constexpr uint16_t DEFAULT_PORT = 25565;
	// Port the embedded MCP server listens on (settings panel AI player).
	constexpr uint16_t MCP_PORT = 8765;
	// Cap so one bulk edit can't stall the tick.
	constexpr uint16_t MAX_BULK_EDIT_RADIUS = 25;

	enum MessageType : uint8_t {
		// Client -> Server
		MSG_HELLO = 1,        // { name: string }
		MSG_INPUT,            // { seq: u32, yaw: f32, pitch: f32, moveFlags: u8, sprint: u8 }
		MSG_BLOCK_EDIT,       // { x: i32, y: i32, z: i32, blockType: u8 }      (single block)
		MSG_BULK_EDIT,        // { cx: i32, cy: i32, cz: i32, radius: u16, blockType: u8 } (sphere)
		MSG_CHAT,             // { text: string }
		// Server -> Client
		MSG_WELCOME = 16,     // { version: u32, seed: u32, spawnX/Y/Z: f32, timeOfDay: f32, selfId: u32 }
		MSG_PLAYER_STATE,     // { id: u32, x/y/z: f32, yaw: f32, pitch: f32, onGround: u8 }
		MSG_PLAYER_JOIN,      // { id: u32, name: string }
		MSG_PLAYER_LEAVE,     // { id: u32 }
		MSG_BLOCK_EDIT_S,     // { x: i32, y: i32, z: i32, blockType: u8 }
		MSG_BULK_EDIT_S,      // { cx: i32, cy: i32, cz: i32, radius: u16, blockType: u8 }
		MSG_TIME,             // { timeOfDay: f32 }
		MSG_CHAT_S,           // { sender: string, text: string }
		MSG_DISCONNECT,       // { reason: string }
		MSG_COUNT
	};

	// Bitmask for MSG_INPUT.moveFlags.
	enum MoveFlag : uint8_t {
		FLAG_FORWARD = 1 << 0,
		FLAG_BACKWARD = 1 << 1,
		FLAG_LEFT = 1 << 2,
		FLAG_RIGHT = 1 << 3,
		FLAG_UP = 1 << 4,
		FLAG_DOWN = 1 << 5,
		FLAG_SPRINT = 1 << 6,
		FLAG_FLYING = 1 << 7
	};

	// Client input sent every frame, consumed by the server's fixed tick.
	struct PlayerInput {
		uint32_t seq = 0;
		float yaw = -90.f;
		float pitch = 0.f;
		uint8_t moveFlags = 0;
		bool sprint = false;
	};

	// Build a length-prefixed packet for each message kind.
	inline sf::Packet makePacket(MessageType type) {
		sf::Packet packet;
		packet << static_cast<uint8_t>(type);
		return packet;
	}

	inline bool readType(sf::Packet& packet, MessageType& type) {
		uint8_t t;
		if (!(packet >> t))
			return false;
		type = static_cast<MessageType>(t);
		return true;
	}

	inline sf::Packet inputPacket(const PlayerInput& in) {
		sf::Packet p = makePacket(MSG_INPUT);
		p << in.seq << in.yaw << in.pitch << in.moveFlags << static_cast<uint8_t>(in.sprint ? 1 : 0);
		return p;
	}

	inline PlayerInput readInput(sf::Packet& p) {
		PlayerInput in;
		uint8_t sprint;
		p >> in.seq >> in.yaw >> in.pitch >> in.moveFlags >> sprint;
		in.sprint = sprint != 0;
		return in;
	}

	inline sf::Packet blockEditPacket(int x, int y, int z, uint8_t blockType) {
		sf::Packet p = makePacket(MSG_BLOCK_EDIT);
		p << static_cast<int32_t>(x) << static_cast<int32_t>(y) << static_cast<int32_t>(z) << blockType;
		return p;
	}

	inline sf::Packet bulkEditPacket(int cx, int cy, int cz, uint16_t radius, uint8_t blockType) {
		sf::Packet p = makePacket(MSG_BULK_EDIT);
		p << static_cast<int32_t>(cx) << static_cast<int32_t>(cy) << static_cast<int32_t>(cz) << radius << blockType;
		return p;
	}

	inline sf::Packet chatPacket(const std::string& text) {
		sf::Packet p = makePacket(MSG_CHAT);
		p << text;
		return p;
	}
} // namespace Protocol