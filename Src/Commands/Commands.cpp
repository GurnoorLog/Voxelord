#include "Commands.h"

Commands::Commands() {
}

void Commands::pollHeld() {
	using namespace sf;
	// Match to what's held now so a click isn't stuck on.
	if (Mouse::isButtonPressed(Mouse::Button::Left)) activate(Command::BREAK);
	else deactivate(Command::BREAK);
	if (Mouse::isButtonPressed(Mouse::Button::Right)) activate(Command::PLACE);
	else deactivate(Command::PLACE);
	if (Mouse::isButtonPressed(Mouse::Button::Middle)) activate(Command::PICK);
	else deactivate(Command::PICK);
}

void Commands::onKeyPressed(sf::Keyboard::Key key) {
	using namespace sf;
	// Held state comes from events so it tracks press/release, not polling.
	switch (key) {
	case Keyboard::W: activate(Command::FORWARD); break;
	case Keyboard::S: activate(Command::BACKWARD); break;
	case Keyboard::A: activate(Command::LEFT); break;
	case Keyboard::D: activate(Command::RIGHT); break;
	case Keyboard::Space: activate(Command::UP); break;
	case Keyboard::LShift: activate(Command::DOWN); break;
	case Keyboard::LControl: activate(Command::SPRINT); break;
	default: activate(findKey(key)); break;
	}
}

void Commands::onKeyReleased(sf::Keyboard::Key key) {
	using namespace sf;
	switch (key) {
	case Keyboard::W: deactivate(Command::FORWARD); break;
	case Keyboard::S: deactivate(Command::BACKWARD); break;
	case Keyboard::A: deactivate(Command::LEFT); break;
	case Keyboard::D: deactivate(Command::RIGHT); break;
	case Keyboard::Space: deactivate(Command::UP); break;
	case Keyboard::LShift: deactivate(Command::DOWN); break;
	case Keyboard::LControl: deactivate(Command::SPRINT); break;
	case Keyboard::E: deactivate(Command::EXPLOSION); deactivate(Command::HUGE_EXPLOSION); break;
	case Keyboard::B: deactivate(Command::BRUSH); deactivate(Command::HUGE_BRUSH); break;
	case Keyboard::P: deactivate(Command::PLACE_BELOW); break;
	case Keyboard::T: deactivate(Command::TELEPORT); break;
	case Keyboard::G: deactivate(Command::TOGGLE_FLY); break;
	case Keyboard::Tab: deactivate(Command::NEXT_BLOCK); deactivate(Command::PREV_BLOCK); break;
	default: break;
	}
}

Command Commands::findKey(sf::Keyboard::Key key) {
	using namespace sf;
	switch (key) {
	case Keyboard::E:
		return Keyboard::isKeyPressed(Keyboard::LAlt) ? Command::HUGE_EXPLOSION : Command::EXPLOSION;
	case Keyboard::B:
		return Keyboard::isKeyPressed(Keyboard::LAlt) ? Command::HUGE_BRUSH : Command::BRUSH;
	case Keyboard::P: return Command::PLACE_BELOW;
	case Keyboard::T: return Command::TELEPORT;
	case Keyboard::G: return Command::TOGGLE_FLY;
	case Keyboard::Tab:
		return Keyboard::isKeyPressed(Keyboard::LShift) ? Command::PREV_BLOCK : Command::NEXT_BLOCK;
	default: return Command::UNKNOWN;
	}
}

void Commands::activate(Command command) {
	if (command != Command::UNKNOWN)
		m_commands.insert(command);
}

void Commands::deactivate(Command command) {
	if (command != Command::UNKNOWN)
		m_commands.erase(command);
}

void Commands::reset() {
	m_commands.clear();
}

bool Commands::isActive(Command command) const {
	return m_commands.find(command) != m_commands.end();
}
