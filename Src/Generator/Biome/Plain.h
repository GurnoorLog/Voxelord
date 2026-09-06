#pragma once

#include "generator/Biome/Biome.h"
#include "generator/Noise/OctavePerlin.h"

class Plain : public Biome {
	virtual int getHeight(ivec2 pos) const override;
	virtual Block getBlock(ivec3 pos, int depth) const override;
	virtual std::vector<StructureInfo> getStructures() const override;
	virtual double biomeValue(double temperature, double altitude) const override;

private:
	OctavePerlin perlin{ 2, 0.5, 1. / 128. };
};