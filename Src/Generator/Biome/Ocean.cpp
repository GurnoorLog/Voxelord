#include "Ocean.h"

Ocean::Ocean() : Biome(Config{ 4, 0.5, 1. / 128, -40, 20, BlockID::SAND, BlockID::SAND }) {}

std::vector<StructureInfo> Ocean::getStructures() const {
	return { };
}

double Ocean::biomeValue(double temperature, double altitude) const {
	return low(temperature) * low(altitude);
}