#include "Mountains.h"

#include "world/WorldConstants.h"

int Mountains::getHeight(ivec2 pos) const {
	return ridgedHeight(pos, warp, perlin, 60., 24, 72);
}

Block Mountains::getBlock(ivec3 pos, int depth) const {
	// Green lower slopes, bare rock higher up
	if (pos.y < Const::SEA_LEVEL + 48)
		return layeredGround(pos, depth, BlockID::GRASS, BlockID::DIRT);
	return { BlockID::STONE };
}

std::vector<StructureInfo> Mountains::getStructures() const {
	return { { StructureID::OAK, 0.05f } };
}

double Mountains::biomeValue(double temperature, double altitude) const {
	return high(temperature) * medium(altitude);
}