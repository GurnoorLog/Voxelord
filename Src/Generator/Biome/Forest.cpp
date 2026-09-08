#include "Forest.h"

Forest::Forest() : Biome(Config{ 4, 0.5, 1. / 128, 8, 12, BlockID::GRASS, BlockID::DIRT }) {}

std::vector<StructureInfo> Forest::getStructures() const {
	return {
		{ StructureID::OAK, 0.4f },
		{ StructureID::BIG_OAK, 0.2f },
		{ StructureID::MAPLE, 0.2f },
		{ StructureID::CHERRY, 0.2f }
	};
}

double Forest::biomeValue(double temperature, double altitude) const {
	return medium(temperature) * high(altitude);
}