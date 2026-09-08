#include "Swamp.h"

Swamp::Swamp() : Biome(Config{ 4, 0.5, 1. / 128, 4, 32, BlockID::GRASS, BlockID::DIRT }) {}

std::vector<StructureInfo> Swamp::getStructures() const {
	return { { StructureID::OAK, 0.1f }, { StructureID::WILLOW, 0.06f } };
}

double Swamp::biomeValue(double temperature, double altitude) const {
	return low(temperature) * medium(altitude);
}