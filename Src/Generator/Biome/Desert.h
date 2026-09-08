#pragma once

#include "generator/Biome/Biome.h"

class Desert : public Biome {
public:
	Desert();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};