#include "Structure.h"

#include "maths/Converter.h"
#include "maths/Dir2D.h"
#include "maths/Dir3D.h"
#include "generator/Biome/Biome.h"
#include "generator/WorldGenerator.h"
#include "world/WorldConstants.h"

Structure::Structure(ivec3 size) : m_blocks{ size } {}

ivec2 Structure::getSupportPos(ivec2 globalPos) const {
	return getCenterPos(globalPos);
}

bool Structure::isGroundedOn(ivec3 centerPos, BiomeID biomeID,
		std::initializer_list<BlockID> ground) const {
	const Biome& biome = g_worldGenerator.biomeMap().getBiome(biomeID);
	BlockID below = biome.getBlock(centerPos + Dir3D::to_ivec3(Dir3D::DOWN), 0).id;
	if (centerPos.y < Const::SEA_LEVEL)
		return false;
	for (BlockID candidate : ground)
		if (below == +candidate)
			return true;
	return false;
}

DynamicArray3D<Block> Structure::build(uint32_t) const {
	return m_blocks;
}

ivec3 Structure::size() const {
	return m_blocks.size();
}

ivec2 Structure::getCenterPos(ivec2 globalPos) const {
	return globalPos + Converter::to2D(size()) / 2;
}

void Structure::add(ivec3 pos, Block block) {
	m_blocks.at(pos) = block;
}

void Structure::addSymetrically(ivec3 pos, Block block) {
	ivec2 center = getCenterPos();
	ivec2 relToCenter = Converter::to2D(pos) - center;
	for (int i = 0; i < 4; ++i) {
		ivec2 rotatedPos = relToCenter + center;
		m_blocks.at({ rotatedPos.x, pos.y, rotatedPos.y }) = block;
		relToCenter = { relToCenter.y, -relToCenter.x };
	}
}

void Structure::fill(ivec3 low, ivec3 high, Block block) {
	for (int y = low.y; y <= high.y; ++y)
		for (int x = low.x; x <= high.x; ++x)
			for (int z = low.z; z <= high.z; ++z) {
				add({ x, y, z }, block);
			}
}

void Structure::fillSymetrically(ivec3 low, ivec3 high, Block block) {
	for (int y = low.y; y <= high.y; ++y)
		for (int x = low.x; x <= high.x; ++x)
			for (int z = low.z; z <= high.z; ++z) {
				addSymetrically({ x, y, z }, block);
			}
}