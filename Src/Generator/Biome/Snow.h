#pragma once

#include "generator/Biome/Biome.h"

class Snow : public Biome {
public:
	Snow();
	BlockID surfaceFluid() const override { return BlockID::ICE; }
	std::vector<StructureInfo> getStructures() const override;
	double biomeValue(double temperature, double altitude) const override;
};