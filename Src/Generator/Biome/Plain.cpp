#include "Plain.h"

Plain::Plain() : Biome(Config{ 2, 0.5, 1. / 128, 4, 4, BlockID::GRASS, BlockID::DIRT }) {}

std::vector<StructureInfo> Plain::getStructures() const {
	return { { StructureID::OAK, 0.001f }, { StructureID::BIG_OAK, 0.004f },
			{ StructureID::CHERRY, 0.006f } };
}

double Plain::biomeValue(double temperature, double altitude) const {
	return medium(temperature) * medium(altitude);
}