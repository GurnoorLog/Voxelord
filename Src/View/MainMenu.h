#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

#include "View/Window.h"

enum class MenuAction {
	NONE,
	CREATE_WORLD,
	JOIN,
	QUIT
};

// The title screen shown before entering a world: a VoxLord title and a stack of menu buttons.
// Pure SFML 2D UI, drawn between GL clears; the GL game world only exists once PLAYING begins.
// Clicking Join enters an address-entry mode (host[:port]); Enter confirms, Escape cancels.
class MainMenu {
public:
	explicit MainMenu(Window* window);

	// Feeds mouse move/click events; returns the action of a clicked button (NONE otherwise).
	MenuAction handleEvent(const sf::Event& event);
	// Recomputes the layout from the current window size and draws the screen.
	void draw();

	std::string getJoinAddress() const { return m_address; }
	bool isJoinMode() const { return m_joinMode; }
	void setJoinAddress(const std::string& address) { m_address = address; }

private:
	struct Button {
		std::string label;
		MenuAction action;
		bool enabled{ true };
		sf::FloatRect bounds;
	};

	Window* p_window;
	sf::Font m_font;
	sf::Text m_title;
	std::vector<Button> m_buttons;
	int m_hovered{ -1 };

	bool m_joinMode = false;
	bool m_addressActive = false;
	std::string m_address = "127.0.0.1";
	sf::FloatRect m_addressBounds;

	sf::Vector2f buttonSize() const;
	void layout();
	int buttonAt(sf::Vector2f point) const;
};