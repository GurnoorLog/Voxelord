#pragma once

#include <set>

#include <SFML/Window.hpp>

enum class Command {
	BREAK,
	PLACE,
	PICK,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN,
	SPRINT,
	EXPLOSION,
	HUGE_EXPLOSION,
	BRUSH,
	HUGE_BRUSH,
	PLACE_BELOW,
	TELEPORT,
	TOGGLE_FLY,
	NEXT_BLOCK,
	PREV_BLOCK,
	UNKNOWN,
	SIZE
};

class Commands {
public:
	Commands();
	// Move/action keys tracked via events.
	void onKeyPressed(sf::Keyboard::Key key);
	void onKeyReleased(sf::Keyboard::Key key);
	bool isActive(Command command) const;
	// Click actions (mouse) poll what's held instead.
	void pollHeld();
	// Clear held/momentary state when leaving to the menu.
	void reset();

private:
	void activate(Command command);
	void deactivate(Command command);
	static Command findKey(sf::Keyboard::Key key);

	std::set<Command> m_commands;
};
