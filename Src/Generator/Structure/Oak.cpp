#include "Oak.h"

Oak::Oak() : Structure({ 5, 7, 5 }) {
	ivec2 center = getCenterPos();
	fill({ center.x - 2, 3, center.y - 2 }, { center.x + 2, 4, center.y + 2 }, +BlockID::LEAVES);
	fill({ center.x - 1, 5, center.y - 1 }, { center.x + 1, 6, center.y + 1 }, +BlockID::LEAVES);
	fill({ center.x, 0, center.y }, { center.x, 4, center.y }, +BlockID::LOG);
}

bool Oak::isValidPos(ivec3 centerPos, BiomeID biomeID) const {
	return isGroundedOn(centerPos, biomeID, { +BlockID::GRASS, +BlockID::DIRT });
}