#pragma once

#include "maths/GlmCommon.h"
#include "block/Block.h"
#include "generator/Biome/BiomeID.h"
#include "generator/Noise/OctavePerlin.h"
#include "generator/Structure/StructureID.h"

#include <vector>

struct StructureInfo {
	StructureID id;
	float freq;
};

class Biome {
public:
	// Parameters for the two most common terrain shapes. The default height is
	// SEA_LEVEL + base + shapeNoise * amplitude with 'surface' on top and 'subsurface' below;
	// biomes with their own profile (mountains, extremes) override getHeight/getBlock instead.
	struct Config {
		int octaves{ 3 };
		double persistence{ 0.5 };
		double frequency{ 1. / 128 };
		int base{ 0 };
		double amplitude{ 1. };
		BlockID surface{ BlockID::GRASS };
		BlockID subsurface{ BlockID::DIRT };
	};

	explicit Biome(const Config& config);
	Biome() : Biome(Config{}) {}
	virtual ~Biome() = default;

	// Large-scale surface height of a column.
	virtual int getHeight(ivec2 pos) const;

	// Block in solid ground. 'depth' is solid blocks above this one (0 = surface), for layering
	// grass/dirt/stone even on overhangs.
	virtual Block getBlock(ivec3 pos, int depth) const;

	// How much the 3D noise distorts the surface, in blocks. 0 keeps the biome flat (a plain),
	// higher values carve cliffs and overhangs (mountains).
	virtual double ruggedness() const { return 0.; }

	// Block used for the topmost liquid layer below sea level (water by default, ice for snow).
	virtual BlockID surfaceFluid() const { return BlockID::WATER; }

	virtual std::vector<StructureInfo> getStructures() const = 0;
	virtual double biomeValue(double temperature, double altitude) const = 0;

protected:
	// Domain-warped ridged terrain shared by the mountain biomes: the lookup is nudged by low
	// frequency noise so ridges meander instead of tracking the grid.
	int ridgedHeight(ivec2 pos, const OctavePerlin& warp, const OctavePerlin& ridge,
		double warpScale, int base, double amplitude) const;

	static double threshold, transition;

	// Common ground layering: a beach of sand around the waterline, otherwise 'surface' on top,
	// a few blocks of 'subsurface' under it, then stone.
	static Block layeredGround(ivec3 pos, int depth, BlockID surface, BlockID subsurface);

	static double low(double value);
	static double medium(double value);
	static double high(double value);
	static double step(double value, double threshold);
	static double smooth(double x);

private:
	OctavePerlin m_heightNoise;
	int m_heightBase;
	double m_heightAmplitude;
	BlockID m_surface;
	BlockID m_subsurface;
};