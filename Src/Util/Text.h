#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include "util/Logger.h"

// Loads the game's UI font. Returns true on success, logs an error and returns false otherwise.
inline bool loadGameFont(sf::Font& font) {
	if (!font.loadFromFile("assets/Fonts/Minecraft.ttf")) {
		LOG(Level::ERROR) << "Failed to load font" << std::endl;
		return false;
	}
	return true;
}

// Anchors a text's local-bound center onto the given point (scale-aware SFML 2.6 text).
inline void centerTextOn(sf::Text& text, sf::Vector2f anchor) {
	const sf::FloatRect bounds = text.getLocalBounds();
	text.setOrigin({ bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f });
	text.setPosition(anchor);
}