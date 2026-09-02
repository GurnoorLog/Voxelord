#pragma once

#include "Generator/Biome/BiomeID.h"
#include "Generator/Structure/StructureGrid.h"
#include "Generator/Structure/StructureID.h"

#include <memory>
#include <optional>
#include <vector>

class BiomeMap;
class Chunk;
class TerrainShaper;
struct ColumnInfo;

// Places structures into chunks via overlaid grids, one per cell. Cell resolving is a pure function of
// position (memoized), so chunks agree regardless of visit order.
class StructureGenerator {
public:
	StructureGenerator(const BiomeMap& biomeMap, const TerrainShaper& terrain);

	// Draws every structure whose cell intersects the chunk.
	void place(Chunk& chunk) const;

	// Drops cached structure cells far from the player (margin added on top of viewDistanceChunks).
	void unloadFar(ivec2 centerChunk, int viewDistanceChunks) const;

private:
	std::shared_ptr<const Placement> resolveCell(int gridIndex, ivec2 cell) const;
	std::shared_ptr<const Placement> computeCell(int gridIndex, ivec2 cell) const;
	// True if the candidate overlaps a higher priority grid; lower ones always yield, so the result
	// stays order free.
	bool collidesWithHigher(int gridIndex, const Placement& candidate) const;
	std::optional<StructureID> pickStructure(BiomeID biome, StructureTier tier, double cellArea, double roll) const;
	void drawPlacement(Chunk& chunk, const Placement& placement) const;
	static StructureTier tierOf(ivec3 size);

	const BiomeMap& m_biomeMap;
	const TerrainShaper& m_terrain;

	// Ordered by collision priority: earlier grids win.
	std::vector<std::unique_ptr<StructureGrid>> m_grids;
};
