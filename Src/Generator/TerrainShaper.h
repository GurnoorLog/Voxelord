#pragma once

#include "generator/CaveCarver.h"
#include "generator/Noise/OctavePerlin.h"
#include "world/WorldConstants.h"

#include <optional>

class BiomeMap;
class Chunk;
class ChunkGenerationInfo;
struct ColumnInfo;

class TerrainShaper {
public:
	explicit TerrainShaper(const BiomeMap& biomeMap);

	void fillBlocks(Chunk& chunk, const ChunkGenerationInfo& info) const;

	// First air block above the terrain, so structures sit on the ground.
	std::optional<int> surfaceHeight(ivec2 xz, const ColumnInfo& col) const;

private:
	void placePlant(Chunk& chunk, ivec3 pos) const;
	// Finer vertical lattice so overhangs aren't smoothed away.
	static constexpr int DENSITY_LATTICE_XZ = 4;
	static constexpr int DENSITY_LATTICE_Y = 2;
	// <1 stretches features, >1 folds more.
	static constexpr double DENSITY_VERTICAL_SCALE = 1.4;

	const BiomeMap& m_biomeMap;
	const CaveCarver m_caveCarver;
	// Folds the surface into overhangs, scaled by ruggedness.
	OctavePerlin m_terrainNoise{ 2, 0.4, 1. / 72. };
	// Drifting cloud masses (~1 pattern per 125 blocks).
	OctavePerlin m_cloudNoise{ 8, 0.5, 1. };
	// Scatters lakes (~1 per 56 blocks); the lake pass floors land into a basin.
	OctavePerlin m_lakeNoise{ 4, 0.5, 1. / 56. };
	bool lakeAt(ivec2 xz, int columnHeight) const;
	// A few blocks above SEA_LEVEL so ponds sit above the open sea.
	static constexpr int LAKE_LEVEL = Const::SEA_LEVEL + 3;
};
