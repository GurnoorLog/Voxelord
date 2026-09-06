#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

#include "View/Window.h"

class Game;

// The gear in the top-right corner and the settings overlay it opens.
//
// While the panel is open the main loop frees the mouse and routes clicks and
// Escape to this panel instead of the world. It is a thin shell over the
// existing Game plumbing: Fly toggles Player::setFlying, the AI player row
// calls Game::startMcp/stopMcp to bring the "MCP Player" bot online.
class SettingsPanel {
public:
	explicit SettingsPanel(Window* window);

	// Screen area of the open panel, used by the main loop for input routing.
	sf::FloatRect panelBounds() const;
	bool isOpen() const { return m_open; }
	void open() { m_open = true; }
	void close() { m_open = false; }
	// Hit-test the corner gear (top-right, updated to the window size).
	bool isOverButton(sf::Vector2f point) const;
	// Updates hover state from a mouse position; call on each MouseMoved.
	void hover(sf::Vector2f point);
	// A click while the panel is open: toggles the row under the cursor, or
	// closes the panel when the click lands outside it. Returns true when the
	// click was consumed by the panel or its rows.
	bool handleClick(sf::Vector2f point, Game& game);
	// Draws the gear button, then the panel when it is open.
	void draw(Game& game);

private:
	struct Row {
		sf::FloatRect bounds;
		std::string label;
		bool on = false;
	};

	// Rows for the two toggles, positioned inside the current panel rect.
	void layoutRows(std::vector<Row>& rows) const;

	Window* p_window;
	sf::Font m_font;
	bool m_open = false;
	bool m_hoverGear = false;
	int m_hoverRow = -1;

	sf::FloatRect gearBounds() const;
	sf::FloatRect panelRect() const;
};