#include "DeadTree.h"

DeadTree::DeadTree() : Structure({ 7, 6, 7 }) {
	ivec2 center = getCenterPos();
	fill({ center.x, 0, center.y }, { center.x, 5, center.y }, +BlockID::LOG);
	add({ center.x + 1, 3, center.y }, +BlockID::LOG);
	add({ center.x + 2, 4, center.y }, +BlockID::LOG);
	add({ center.x, 2, center.y - 1 }, +BlockID::LOG);
	add({ center.x - 1, 3, center.y + 1 }, +BlockID::LOG);
	add({ center.x - 1, 3, center.y + 2 }, +BlockID::LOG);
	add({ center.x - 1, 4, center.y + 3 }, +BlockID::LOG);
	add({ center.x - 1, 3, center.y - 2 }, +BlockID::LOG);
	add({ center.x - 1, 4, center.y - 3 }, +BlockID::LOG);
}

bool DeadTree::isValidPos(ivec3 centerPos, BiomeID biomeID) const {
	return isGroundedOn(centerPos, biomeID, { +BlockID::SAND });
}