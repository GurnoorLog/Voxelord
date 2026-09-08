#pragma once

#include "generator/Biome/Biome.h"

class Plain : public Biome {
public:
	Plain();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};