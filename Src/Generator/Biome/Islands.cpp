#include "Islands.h"

Islands::Islands() : Biome(Config{ 3, 0.5, 1. / 64, -1, 32, BlockID::GRASS, BlockID::DIRT }) {}

std::vector<StructureInfo> Islands::getStructures() const {
	return { { StructureID::PALM, 0.06f } };
}

double Islands::biomeValue(double temperature, double altitude) const {
	return medium(temperature) * low(altitude);
}