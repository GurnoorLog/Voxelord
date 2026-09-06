#pragma once

#include <SFML/Window.hpp>

#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <atomic>
#include <utility>
#include <vector>
#include <unordered_map>
#include <string>

#include "ResManager/ResManager.h"
#include "World/Chunk.h"
#include "World/ChunkMap.h"
#include "View/Window.h"
#include "View/PickedBlockDrawer.h"
#include "View/BlockContourDrawer.h"
#include "View/ExplosionDrawer.h"
#include "View/RemotePlayerDrawer.h"
#include "View/SettingsPanel.h"
#include "Renderer/DefaultRenderer.h"
#include "Renderer/WaterRenderer.h"
#include "Renderer/LavaRenderer.h"
#include "Renderer/PostProcessingRenderer.h"
#include "Renderer/SkyRenderer.h"
#include "Commands/Commands.h"
#include "Game/Player.h"
#include "Game/DayCycle.h"
#include "Network/NetworkClient.h"
#include "Network/Protocol.h"
#include "Mcp/McpServer.h"

class Window;

// A line of chat shown in the HUD, echoed from the server.
struct ChatLine {
	std::string sender;
	std::string text;
};

// A remote player interpolated toward its latest server position.
struct RemotePlayer {
	std::string name;
	vec3 serverPosition{ 0.f };
	vec3 position{ 0.f };
	vec3 prevPosition{ 0.f };
	float yaw = 0.f;
	float pitch = 0.f;
	bool onGround = false;
	float timer = 0.f; // time since the last server update, for interpolation
};

class Game {
public:
	Game(Window* const window);
	~Game();

	void processKeyboard(sf::Time dt, Commands& commands);
	void processMouseClick(sf::Time dt, Commands& commands);
	void processMouseMove(sf::Time dt);
	void processMouseWheel(sf::Time dt, GLfloat delta);
	// Rotates the hotbar block selection by dir (+1 next, -1 previous).
	void cycleHotbarBlock(int dir);
	// Selects the hotbar slot directly (0-based; number keys minus 1).
	void selectHotbarSlot(int slot);
	void update(sf::Time dt);
	void render();
	void runOrchestratorLoop();
	void runWorkerLoop();
	void onChangedSize(ivec2 size);

	Player& getPlayer();
	ChunkMap& getChunkMap();
	const ChunkMap& getChunkMap() const;
	Window& getWindow();
	float getTimeOfDay() const;

	// Multiplayer.
	void startOnline(const std::string& name, const std::string& host, uint16_t port);
	void stopOnline();
	// --test-move: stream FLAG_FORWARD while online.
	void setTestMove(bool on) { m_testMove = on; }
	bool isOnline() const { return m_online; }
	bool shouldReturnToMenu() const { return m_shouldReturnToMenu; }
	// Local edit that is also relayed online.
	void setBlockNetwork(ivec3 pos, Block block);

	// Chat.
	bool isChatOpen() const { return m_chatOpen; }
	const std::string& getChatBuffer() const { return m_chatBuffer; }
	const std::vector<ChatLine>& getChatLines() const { return m_chatLines; }
	void openChat();
	void closeChat();
	void appendChatChar(char c);
	void backspaceChat();
	void sendChatBuffer();

	// MCP (LLM player): embeds a JSON-RPC server so an agent can drive this client as a player.
	void startMcp(uint16_t port);
	void stopMcp();
	// In-game "@bot ..." orders; true if the message was a bot command, not regular chat.
	bool handleBotCommand(const std::string& text);
	bool isBotOnline() const { return m_mcpBotOnline; }
	const std::string& getBotName() const { return m_mcpBotName; }

	// Settings overlay (top-right corner gear).
	SettingsPanel& getSettingsPanel() { return m_settingsPanel; }
	const SettingsPanel& getSettingsPanel() const { return m_settingsPanel; }
	void toggleFlyFromSettings();
	void toggleBotFromSettings();

private:
	void clearRenderTarget();
	void buildHeldBlockMesh(Block block);
	// explosive: also play the visual effect if the edit is accepted.
	void submitSphereEdit(int radius, Block block, bool explosive = false);

	void pollNetwork();
	void handleNetworkMessage(Protocol::MessageType type, sf::Packet& packet);
	void sendInput();
	void addChatLine(const std::string& sender, const std::string& text);
	void buildRemoteVisuals();

	// MCP.
	void drainMcp(sf::Time dt);
	nlohmann::json mcpToolResult(const std::string& name, const nlohmann::json& args);
	nlohmann::json mcpObserve() const;
	// MCP bot: a second server connection driven by the tools, not the local body.
	void connectMcpBot();
	void pollMcpNetwork();
	void sendMcpInput();
	std::optional<ivec3> mcpBotTarget() const;
	std::optional<ivec3> mcpBotPlace() const;

	// In-game bot control.
	enum class BotTaskKind { NONE, GO, MINE, PLACE, LOOK, STOP };
	struct BotTask {
		BotTaskKind kind = BotTaskKind::NONE;
		ivec3 block{ 0, 0, 0 };     // MINE / PLACE: target coordinate
		BlockID blockId = +BlockID::AIR; // MINE / PLACE: block to mine / place
		vec3 target{ 0.f };         // GO: where to walk
		bool sprint = false;
		bool follow = false;        // GO: keep re-aiming at the player
	};
	void processBotCommand(BotTask task);
	void steerBotToward(const vec3& target, bool sprint, float stopDistance = 1.2f);
	void steerBotLookAt(const vec3& point);
	void maybeHopObstacle(); // jump if a wall is in the way
	std::optional<ivec3> findNearestBlock(BlockID id, float maxRadius) const;
	BlockID parseBlockName(const std::string& word) const;
	void updateBotTask(sf::Time dt); // one step per frame
	// Grammar first; fall back to a local ollama model.
	void askOllama(const std::string& text);
	void applyOllamaResult();
	void endBotTask(const std::string& message, bool ok);

	Player m_player;

	Window* const p_window{ nullptr };
	ChunkMap m_chunkMap;

	DayCycle m_dayCycle;

	DefaultRenderer m_defaultRenderer;
	WaterRenderer m_waterRenderer;
	LavaRenderer m_lavaRenderer;
	PostProcessingRenderer m_postProcessingRenderer;
	SkyRenderer m_skyRenderer;

	PickedBlockDrawer m_pickedBlockDrawer;
	BlockContourDrawer m_blockContourDrawer;
	ExplosionDrawer m_explosionDrawer;
	RemotePlayerDrawer m_remotePlayerDrawer;

	std::thread m_orchestratorThread;
	std::vector<std::thread> m_workerThreads;
	std::atomic<bool> m_stopGeneratingThread{ false };

	// Multiplayer state (defaults describe an offline single-player session).
	NetworkClient m_network;
	bool m_online = false;
	bool m_testMove = false;
	int m_selfId = -1;
	std::string m_playerName = "Player";
	uint32_t m_netSeq = 0;
	uint8_t m_netMoveFlags = 0;
	uint64_t m_netDiagCounter = 0;
	sf::Clock m_lastInputClock;
	bool m_netSprint = false;
	// Ease toward the server position when it diverges, instead of teleporting.
	vec3 m_reconcileTarget{ 0.f };
	bool m_hasReconcile = false;
	float m_reconcileSpeed = 30.f;

	// Embedded MCP player.
	std::unique_ptr<McpServer> m_mcp;
	bool m_mcpActive = false;
	// Move flags/sprint injected for the bot connection.
	uint8_t m_mcpMoveFlags = 0;
	bool m_mcpSprint = false;
	sf::Clock m_mcpJumpClock; // how long the hop input is held
	bool m_mcpJumpHeld = false;

	// The MCP bot: a second server connection driven by the tools.
	NetworkClient m_mcpNetwork;
	bool m_mcpBotOnline = false;
	int m_mcpBotId = -1;
	std::string m_mcpBotName = "MCP Player";
	vec3 m_mcpBotPos{ 0.f };
	vec3 m_mcpBotPrevPos{ 0.f };
	float m_mcpBotYaw = -90.f;
	float m_mcpBotPitch = 0.f;
	bool m_mcpBotOnGround = false;
	uint32_t m_mcpBotSeq = 0;
	sf::Clock m_mcpBotInputClock;
	bool m_mcpBotIdMapped = false; // connect the bot once when online
	BlockID m_mcpHeldBlock = +BlockID::AIR;
	// Current autonomous bot task.
	BotTask m_botTask;
	bool m_botTaskActive = false;
	ivec3 m_botLastMine{ -1 }; // last MINE block, so we don't re-mine it
	int m_botMineCount = 0; // how many mined so far this task
	int m_botMineGoal = 1;
	// Async ollama fallback, applied on the game thread when ready.
	std::atomic<bool> m_ollamaQuestionPending{ false };
	std::mutex m_ollamaMutex;
	std::string m_ollamaText;
	bool m_ollamaResultReady = false;
	std::string m_ollamaResult;
	// Mirror of the primary connection's host/port, so the bot joins the same world.
	std::string m_connectHost;
	uint16_t m_connectPort = Protocol::DEFAULT_PORT;
	std::unordered_map<int, RemotePlayer> m_remotePlayers;
	std::vector<RemotePlayerVisual> m_remoteVisuals;
	std::vector<ChatLine> m_chatLines;
	std::string m_chatBuffer;
	bool m_chatOpen = false;
	bool m_shouldReturnToMenu = false;
	SettingsPanel m_settingsPanel;
};