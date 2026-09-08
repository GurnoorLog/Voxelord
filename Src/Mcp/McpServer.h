#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <SFML/Network.hpp>

#include "json/json.hpp"

// A minimal Model Context Protocol (JSON-RPC 2.0) server embedded in the game client.
//
// It listens on 127.0.0.1:<port>, accepts a single MCP client (an LLM agent, a test harness, or a
// stdio adapter for standard MCP hosts), and speaks newline-delimited JSON-RPC over the socket.
// Requests ("id" present) are pushed to an outgoing command queue that the game thread drains each
// frame; tool execution happens on the game thread, where the live player/network state lives. The
// response is sent back through sendResult()/sendError().
//
// Not thread-safe for responses from two threads at once: the game thread must call the send*
// helpers, and the worker thread must not touch the socket once the server starts handing commands
// to the game. All cross-thread state is guarded by m_mutex.

class McpServer {
public:
	// A decoded inbound request awaiting execution on the game thread.
	struct Command {
		nlohmann::json id;                          // request id (string/number/null); null for notifications
		std::string method;                         // "initialize", "notifications/initialized",
													// "ping", "tools/list", "tools/call", ...
		nlohmann::json params;                      // for tools/call: { name, arguments }
		bool isNotification = false;
	};

	explicit McpServer(uint16_t port);
	~McpServer();

	McpServer(const McpServer&) = delete;
	McpServer& operator=(const McpServer&) = delete;

	// Spawns the accept/serve thread. Does not block.
	void start();
	// Stops the thread and closes the listener/socket. Safe to call more than once.
	void stop();

	// Main-thread API -------------------------------------------------------
	// Pulls the next pending request (nullptr when idle). Call repeatedly until it returns nullptr.
	std::unique_ptr<Command> poll();
	// Responds to a request id with a JSON result object (wrapped as MCP content text).
	void sendResult(const nlohmann::json& id, const nlohmann::json& result);
	// Responds to a request id with a JSON-RPC error.
	void sendError(const nlohmann::json& id, int code, const std::string& message);

private:
	uint16_t m_port;
	std::thread m_thread;
	std::atomic<bool> m_stop{ false };

	// m_mutex guards the command queue and the client socket, touched from the game
	// thread (poll/respond) and the worker thread.
	std::mutex m_mutex;
	std::deque<std::unique_ptr<Command>> m_commands;

	sf::TcpListener m_listener;
	std::unique_ptr<sf::TcpSocket> m_client; // one MCP client at a time

	void run();
	void handleLine(const std::string& line);
	void enqueue(std::unique_ptr<Command> command);
	void writeToClient(const std::string& jsonLine);
};