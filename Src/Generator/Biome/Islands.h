#pragma once

#include "generator/Biome/Biome.h"
#include "generator/Noise/OctavePerlin.h"

class Islands : public Biome {
	virtual int getHeight(ivec2 pos) const override;
	virtual Block getBlock(ivec3 pos, int depth) const override;
	virtual std::vector<StructureInfo> getStructures() const override;
	virtual double biomeValue(double temperature, double altitude) const override;

private:
	OctavePerlin perlin{ 3, 0.5, 1. / 64. };
};