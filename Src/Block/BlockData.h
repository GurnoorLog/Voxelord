#pragma once

#include "maths/Dir3D.h"
#include "json/json.hpp"

#include "resources/TextureArray.h"

#include <array>
#include <vector>

using json = nlohmann::json;

class BlockData {
public:
	enum Category {
		DEFAULT, SEMI_TRANSPARENT, WATER, LAVA, AIR, PLANT
	};

	BlockData();
	BlockData(const json& j, const std::vector<TextureArray*>& texArrays);
	// Physics-only load: parses every field except texture indices, so headless processes
	// (server) can use BlockData without a GL context or texture arrays.
	BlockData(const json& j);

	bool isOpaque() const;
	bool isObstacle() const;
	int getResistance() const;
	int getEmission() const;
	Category getCategory() const;
	int getTexture(Dir3D::Dir dir) const;

private:
	bool m_opaque = false;
	bool m_obstacle = false;
	int m_resistance = 0;
	int m_emission = 0;
	Category m_category = DEFAULT;
	std::array<int, Dir3D::SIZE> m_textures;

	friend void parseCommonFields(BlockData& data, const json& j);
};
