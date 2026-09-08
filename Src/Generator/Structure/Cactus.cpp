#include "Cactus.h"

Cactus::Cactus() : Structure({ 1, 5, 1 }) {
	ivec2 center = getCenterPos();
	fill({ center.x, 0, center.y }, { center.x, 3, center.y }, +BlockID::CACTUS);
}

bool Cactus::isValidPos(ivec3 centerPos, BiomeID biomeID) const {
	return isGroundedOn(centerPos, biomeID, { +BlockID::SAND });
}