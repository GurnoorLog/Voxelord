#pragma once

#include "generator/Biome/Biome.h"

class Swamp : public Biome {
public:
	Swamp();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};