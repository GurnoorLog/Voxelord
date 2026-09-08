#pragma once

#include "generator/Biome/Biome.h"

class Islands : public Biome {
public:
	Islands();
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};