#pragma once

#include "generator/Biome/Biome.h"

class Forest : public Biome {
public:
	Forest();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};