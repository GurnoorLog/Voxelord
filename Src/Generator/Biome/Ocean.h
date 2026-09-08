#pragma once

#include "generator/Biome/Biome.h"

class Ocean : public Biome {
public:
	Ocean();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};