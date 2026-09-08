#pragma once

#include <SFML/Graphics/RenderTexture.hpp>

#include "maths/GlmCommon.h"
#include "util/Logger.h"

// Creates a GL 4.3 render texture sized to the window, logging an error on failure.
inline bool createRenderTexture(sf::RenderTexture& renderTexture, ivec2 windowSize, const char* label) {
	sf::ContextSettings settings;
	settings.majorVersion = 4;
	settings.minorVersion = 3;
	settings.depthBits = 24;
	settings.stencilBits = 8;
	settings.antialiasingLevel = 0;
	if (!renderTexture.create(windowSize.x, windowSize.y, settings)) {
		LOG(Level::ERROR) << "Could not create " << label << std::endl;
		return false;
	}
	return true;
}