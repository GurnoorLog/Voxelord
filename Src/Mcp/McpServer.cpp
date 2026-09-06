#include "mcp/McpServer.h"

#include <cstring>
#include <sstream>

#include "util/Logger.h"

namespace {
	// Newline-delimited JSON: each object is one complete line. SFML sockets give us a byte
	// stream, so we assemble lines ourselves.
	constexpr char kLineEnd = '\n';

	std::string takeNextLine(std::string& buffer) {
		std::size_t pos = buffer.find(kLineEnd);
		if (pos == std::string::npos)
			return {};
		std::string line = buffer.substr(0, pos);
		buffer.erase(0, pos + 1);
		return line;
	}
}

McpServer::McpServer(uint16_t port)
	: m_port{ port } {}

McpServer::~McpServer() {
	stop();
}

void McpServer::start() {
	m_listener.listen(m_port, sf::IpAddress::LocalHost);
	m_listener.setBlocking(false);
	m_stop = false;
	m_thread = std::thread(&McpServer::run, this);
	LOG(Level::INFO) << "MCP server listening on 127.0.0.1:" << m_port << std::endl;
}

void McpServer::stop() {
	m_stop = true;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_client)
			m_client->disconnect();
	}
	m_listener.close();
	if (m_thread.joinable())
		m_thread.join();
}

bool McpServer::hasClient() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_client && m_client->getRemoteAddress() != sf::IpAddress::None;
}

std::unique_ptr<McpServer::Command> McpServer::poll() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_commands.empty())
		return nullptr;
	std::unique_ptr<Command> cmd = std::move(m_commands.front());
	m_commands.pop_front();
	return cmd;
}

void McpServer::sendResult(const nlohmann::json& id, const nlohmann::json& result) {
	nlohmann::json msg;
	msg["jsonrpc"] = "2.0";
	msg["id"] = id;
	msg["result"] = result;
	writeToClient(msg.dump());
}

void McpServer::sendError(const nlohmann::json& id, int code, const std::string& message) {
	nlohmann::json msg;
	msg["jsonrpc"] = "2.0";
	msg["id"] = id;
	msg["error"] = { { "code", code }, { "message", message } };
	writeToClient(msg.dump());
}

void McpServer::run() {
	std::string lineBuffer; // assembled across receive calls; private to this thread

	while (!m_stop) {
		if (!m_client) {
			auto socket = std::make_unique<sf::TcpSocket>();
			sf::Socket::Status status = m_listener.accept(*socket);
			if (status == sf::Socket::Done) {
				socket->setBlocking(false);
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					m_client = std::move(socket);
				}
				LOG(Level::INFO) << "MCP client connected" << std::endl;
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
		}

		// Read the buffered burst and split into whole lines (executed outside the lock so
		// handleLine/enqueue never deadlock against the game thread's poll/respond).
		std::vector<std::string> lines;
		bool clientGone = false;
		while (!m_stop && !clientGone) {
			char buffer[4096];
			std::size_t received = 0;
			sf::Socket::Status status;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				status = m_client->receive(buffer, sizeof(buffer), received);
			}
			if (status == sf::Socket::Done && received > 0) {
				lineBuffer.append(buffer, received);
				std::string line;
				while (!(line = takeNextLine(lineBuffer)).empty())
					lines.push_back(line);
				continue;
			}
			if (status == sf::Socket::NotReady)
				break; // nothing more buffered
			clientGone = true; // Disconnected / Error / Partial-no-more-data
		}

		for (const std::string& line : lines)
			handleLine(line);

		if (clientGone) {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_client->disconnect();
				m_client.reset();
			}
			LOG(Level::INFO) << "MCP client disconnected" << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void McpServer::handleLine(const std::string& line) {
	nlohmann::json msg;
	try {
		msg = nlohmann::json::parse(line);
	} catch (const nlohmann::json::parse_error&) {
		LOG(Level::WARNING) << "MCP: dropped unparseable line" << std::endl;
		return;
	}

	auto cmd = std::make_unique<Command>();
	if (msg.contains("id"))
		// Echo the id's exact wire value (string or number) in the response.
		cmd->id = msg["id"];
	else
		cmd->isNotification = true;
	cmd->method = msg.value("method", "");
	if (msg.contains("params"))
		cmd->params = msg["params"];
	enqueue(std::move(cmd));
}

void McpServer::enqueue(std::unique_ptr<Command> command) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_commands.push_back(std::move(command));
}

void McpServer::writeToClient(const std::string& jsonLine) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_client)
		return;
	std::string out = jsonLine + kLineEnd;
	std::size_t sent = 0;
	sf::Socket::Status status = m_client->send(out.c_str(), out.size(), sent);
	if (status == sf::Socket::Error || status == sf::Socket::Disconnected)
		m_client->disconnect();
}