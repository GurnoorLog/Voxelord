#include "Snow.h"

Snow::Snow() : Biome(Config{ 4, 0.5, 1. / 128, 8, 8, BlockID::SNOW, BlockID::DIRT }) {}

std::vector<StructureInfo> Snow::getStructures() const {
	return { { StructureID::FIR, 0.03f } };
}

double Snow::biomeValue(double temperature, double altitude) const {
	return low(temperature) * high(altitude);
}