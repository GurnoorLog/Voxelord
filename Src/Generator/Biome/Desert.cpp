#include "Desert.h"

Desert::Desert() : Biome(Config{ 3, 0.5, 1. / 128, 6, 8, BlockID::SAND, BlockID::SAND }) {}

std::vector<StructureInfo> Desert::getStructures() const {
	return { { StructureID::DEAD_TREE, 0.001f } /*, { StructureID::CACTUS, 0.005f }*/ };
}

double Desert::biomeValue(double temperature, double altitude) const {
	return high(temperature) * low(altitude);
}