#include <iostream>
#include <glad/glad.h>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics.hpp>

#include "Game/Game.h"
#include "Util/DebugGL.h"
#include "View/Window.h"
#include "View/MainMenu.h"
#include "Util/Logger.h"
#include "Util/FPSCounter.h"
#include "World/WorldConstants.h"
#include "View/WindowTextDrawer.h"
#include "View/CaptureMouse.h"
#include "View/Crosshair.h"
#include "Commands/Commands.h"
#include "Network/Protocol.h"
#include "Server/GameServer.h"

#include <filesystem>
#include <memory>
#include <thread>
#include <cstdlib>

enum class AppState {
	MENU,
	PLAYING
};

int main(int argc, char* argv[]) {
	std::filesystem::create_directories("Data/Logs");
	LOG.setFileOutputLevel(Level::DEBUG);
	LOG.setOutputFile("Data/Logs/global.log");

	// Optional --connect <host> skips the menu and joins a server immediately; --host starts an
	// embedded server and joins it (the host-and-play path) without needing the menu. Both are
	// used for headless testing and quick multiplayer sessions. --mcp <port> exposes an embedded
	// MCP (JSON-RPC over TCP) control server on 127.0.0.1 so an LLM/agent can drive this client
	// as a player; the port defaults to 8765 when the flag is given without a value.
	std::string autoConnect;
	bool autoHost = false;
	bool testMove = false;
	int mcpPort = 0;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--connect" && i + 1 < argc)
			autoConnect = argv[++i];
		if (arg == "--host")
			autoHost = true;
		if (arg == "--test-move")
			testMove = true;
		if (arg == "--mcp") {
			mcpPort = 8765;
			if (i + 1 < argc) {
				int parsed = std::atoi(argv[i + 1]);
				if (parsed > 0 && parsed <= 65535) {
					mcpPort = parsed;
					++i;
				}
			}
		}
	}

	LOG(Level::INFO) << "Application launched" << std::endl;
	Window window;
	MainMenu menu{ &window };
	WindowTextDrawer textDrawer{ &window };
	Crosshair crosshair{ &window };

	AppState state = AppState::MENU;
	std::unique_ptr<Game> game;
	std::unique_ptr<CaptureMouse> captureMouse;

	// Embedded host-and-play server (Create World). Owned by main so its thread can be joined on exit.
	std::unique_ptr<GameServer> hostServer;
	std::thread hostThread;

	sf::Clock clock;
	FPSCounter fpsCounter;
	// Command/hold-state tracker, shared across the whole session so movement key presses and
	// releases accumulate over many frames (the window event pump only fires on edges).
	Commands commands;

	// Tear down the current session and return to the title screen: disconnect the client,
	// stop and join the host server thread, drop the GL world.
	auto leaveToMenu = [&]() {
		if (game) {
			game->stopOnline();
			game.reset();
		}
		captureMouse.reset();
		if (hostServer)
			hostServer->stop();
		if (hostThread.joinable())
			hostThread.join();
		hostServer.reset();
		commands.reset();
		state = AppState::MENU;
		LOG(Level::INFO) << "Returned to menu" << std::endl;
	};

	auto createWorld = [&]() {
		LOG(Level::INFO) << "Creating world" << std::endl;
		// Host a server on this machine and connect to it (host-and-play).
		hostServer = std::make_unique<GameServer>(Protocol::DEFAULT_PORT);
		if (hostServer->start()) {
			hostThread = std::thread([&]() { hostServer->run(); });
			LOG(Level::INFO) << "Embedded server listening on port " << Protocol::DEFAULT_PORT << std::endl;
		} else {
			LOG(Level::WARNING) << "Port " << Protocol::DEFAULT_PORT << " in use; connecting to the existing server instead" << std::endl;
			hostServer.reset();
		}
		game = std::make_unique<Game>(&window);
		game->startOnline("Player", "127.0.0.1", Protocol::DEFAULT_PORT);
		if (mcpPort > 0)
			game->startMcp(static_cast<uint16_t>(mcpPort));
		captureMouse = std::make_unique<CaptureMouse>(&window);
		state = AppState::PLAYING;
	};

	auto joinWorld = [&]() {
		LOG(Level::INFO) << "Joining " << menu.getJoinAddress() << std::endl;
		game = std::make_unique<Game>(&window);
		game->startOnline("Player", menu.getJoinAddress(), Protocol::DEFAULT_PORT);
		if (mcpPort > 0)
			game->startMcp(static_cast<uint16_t>(mcpPort));
		captureMouse = std::make_unique<CaptureMouse>(&window);
		state = AppState::PLAYING;
	};

	if (!autoConnect.empty() || autoHost) {
		if (autoHost) {
			createWorld();
		} else {
			menu.setJoinAddress(autoConnect);
			joinWorld();
		}
		game->setTestMove(testMove);
	}

	while (!window.shouldClose()) {
		sf::Time deltaTime = clock.getElapsedTime();
		clock.restart();
		fpsCounter.update(deltaTime);

		sf::Event event;
		while (window.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				window.toClose();
				break;
			case sf::Event::Resized:
				if (game) {
					game->onChangedSize({ event.size.width, event.size.height });
				}
				break;
			case sf::Event::KeyPressed:
				if (state == AppState::PLAYING) {
					if (game->isChatOpen()) {
						if (event.key.code == sf::Keyboard::Escape)
							game->closeChat();
						else if (event.key.code == sf::Keyboard::Return)
							game->sendChatBuffer();
						else if (event.key.code == sf::Keyboard::BackSpace)
							game->backspaceChat();
					} else {
						commands.onKeyPressed(event.key.code);
						if (event.key.code == sf::Keyboard::Escape) {
							captureMouse->toggle();
						} else if (event.key.code == sf::Keyboard::T) {
							game->openChat();
						}
						// 1-9 select the matching hotbar slot directly.
						if (event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num9)
							game->selectHotbarSlot(event.key.code - sf::Keyboard::Num1);
					}
				} else {
					// The menu consumes Enter/Escape for its join-address mode.
					MenuAction action = menu.handleEvent(event);
					if (action == MenuAction::JOIN)
						joinWorld();
					if (!menu.isJoinMode() && event.key.code == sf::Keyboard::Escape)
						window.toClose();
				}
				break;
			case sf::Event::KeyReleased:
				if (state == AppState::PLAYING)
					commands.onKeyReleased(event.key.code);
				break;
			case sf::Event::TextEntered:
				if (state == AppState::PLAYING) {
					if (game->isChatOpen() && event.text.unicode < 128)
						game->appendChatChar(static_cast<char>(event.text.unicode));
				} else {
					menu.handleEvent(event);
				}
				break;
			case sf::Event::MouseWheelScrolled:
				if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel && game) {
					game->processMouseWheel(deltaTime, event.mouseWheelScroll.delta);
				}
				break;
			case sf::Event::MouseMoved:
			case sf::Event::MouseButtonPressed:
				if (state == AppState::MENU) {
					MenuAction action = menu.handleEvent(event);
					switch (action) {
					case MenuAction::CREATE_WORLD:
						createWorld();
						break;
					case MenuAction::JOIN:
						joinWorld();
						break;
					case MenuAction::QUIT:
						window.toClose();
						break;
					default:
						break;
					}
				}
				break;
			default:
				break;
			}
		}

		if (state == AppState::PLAYING) {
			if (captureMouse->isEnabled() && !game->isChatOpen()) {
				commands.pollHeld();
				game->processMouseMove(deltaTime);
				game->processKeyboard(deltaTime, commands);
				game->processMouseClick(deltaTime, commands);
			}
			game->update(deltaTime);
			captureMouse->update();

			game->render();

			window.pushGLStates();
			crosshair.draw();
			textDrawer.drawAll(fpsCounter.get(), *game);
			textDrawer.drawChat(*game);
			window.popGLStates();

			// Server disconnect (or a failed connection): back to the title screen.
			if (game->shouldReturnToMenu())
				leaveToMenu();
		} else {
			glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			window.pushGLStates();
			menu.draw();
			window.popGLStates();
		}

		window.display();
	}

	leaveToMenu();
	LOG(Level::INFO) << "Application terminated" << std::endl;
	return 0;
}