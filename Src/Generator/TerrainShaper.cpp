#include "TerrainShaper.h"

#include "generator/Biome/BiomeMap.h"
#include "maths/Converter.h"
#include "maths/MiscMath.h"
#include "util/DynamicArray3D.h"
#include "world/Chunk.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
	enum class ColumnBlock { AIR, CARVED, SOLID };

	// The ruggedness term lets density fold back over itself into overhangs.
	template<typename SampleNoise, typename IsCarved, typename Visit>
	void walkColumn(int top, int height, double ruggedness, ivec2 xz,
			SampleNoise sampleNoise, IsCarved isCarved, Visit visit) {
		int depth = 0;
		for (int y = top; y >= 0; --y) {
			ivec3 pos{ xz.x, y, xz.y };
			double density = (height - y) + sampleNoise(pos) * ruggedness;
			if (density > 0.) {
				ColumnBlock kind = isCarved(pos, depth) ? ColumnBlock::CARVED : ColumnBlock::SOLID;
				if (!visit(pos, depth, kind))
					return;
				++depth;
			} else {
				depth = 0;
				if (!visit(pos, depth, ColumnBlock::AIR))
					return;
			}
		}
	}
}

TerrainShaper::TerrainShaper(const BiomeMap& biomeMap) : m_biomeMap{ biomeMap } {}

void TerrainShaper::fillBlocks(Chunk& chunk, const ChunkGenerationInfo& info) const {
	ivec3 origin = Converter::chunkToGlobal(chunk.getPosition());

	// Tallest column plus how high an overhang can reach.
	int topY = Const::SEA_LEVEL;
	for (int x = 0; x < Const::SECTION_SIDE; ++x)
		for (int z = 0; z < Const::SECTION_SIDE; ++z) {
			ivec2 c{ x, z };
			topY = std::max(topY, info.height(c) + static_cast<int>(ceil(info.ruggedness(c))));
		}
	topY += 1;

	// Coarse lattice, trilinearly interpolated per block to keep it cheap.
	const ivec3 L{ DENSITY_LATTICE_XZ, DENSITY_LATTICE_Y, DENSITY_LATTICE_XZ };
	int nx = Const::SECTION_SIDE / L.x + 1, nz = Const::SECTION_SIDE / L.z + 1, ny = topY / L.y + 2;
	DynamicArray3D<double> grid({ nx, ny, nz });
	auto at = [&](int i, int j, int k) -> double& { return grid.at({ i, j, k }); };
	for (int i = 0; i < nx; ++i)
		for (int k = 0; k < nz; ++k)
			for (int j = 0; j < ny; ++j) {
				dvec3 p{ origin.x + i * L.x, j * L.y * DENSITY_VERTICAL_SCALE, origin.z + k * L.z };
				at(i, j, k) = m_terrainNoise.getNoise(p);
			}

	CaveCarver::Grid caveGrid = m_caveCarver.sample(origin, topY);

	auto sampleNoise = [&](ivec3 pos) {
		math::LatticeCoord lc = math::latticeCoord(pos, L);
		return math::trilerp(grid, lc.cell, lc.frac);
	};

	ivec2 origin2D = Converter::to2D(origin);
	for (int x = 0; x < Const::SECTION_SIDE; ++x)
		for (int z = 0; z < Const::SECTION_SIDE; ++z) {
			ivec2 c{ x, z };
			const Biome& biome = m_biomeMap.getBiome(info.biome(c));
			int height = info.height(c);
			double rugged = info.ruggedness(c);
			CaveCarver::Column caveCol = m_caveCarver.column(origin2D + c, height);

			// Air cell just above the surface, where a plant goes.
			ivec3 lastAirPos{ 0, -1, 0 };
			// Topmost solid block; the lake pass uses it to flatten land into water.
			int surfaceY = -1;
			walkColumn(topY, height, rugged, c, sampleNoise,
				[&](ivec3 local, int depth) { return m_caveCarver.isCarved(caveGrid, caveCol, local, depth); },
				[&](ivec3 local, int depth, ColumnBlock kind) {
					// Caves stay air, but a lava pit pools near the bottom.
					switch (kind) {
					case ColumnBlock::CARVED:
						if (local.y < caveCol.lavaLevel)
							chunk.setBlock(local, { BlockID::LAVA });
						break;
					case ColumnBlock::SOLID: {
						Block surface = biome.getBlock(origin + local, depth);
						chunk.setBlock(local, surface);
						surfaceY = std::max(surfaceY, local.y);
						if (depth == 0 && surface.id == +BlockID::GRASS && lastAirPos.y >= Const::SEA_LEVEL)
							placePlant(chunk, lastAirPos);
						break;
					}
					case ColumnBlock::AIR:
						lastAirPos = local;
						if (local.y < Const::SEA_LEVEL) {
							BlockID fluid = BlockID::WATER;
							if (local.y == Const::SEA_LEVEL - 1)
								fluid = biome.surfaceFluid();
							chunk.setBlock(local, { fluid });
						}
						break;
					}
					return true;
				});

			if (lakeAt(origin2D + c, height)) {
				constexpr int BED_DEPTH = 3;
				int bed = LAKE_LEVEL - BED_DEPTH;
				for (int y = topY; y > bed; --y) {
					if (y <= LAKE_LEVEL)
						chunk.setBlock({ x, y, z }, { BlockID::WATER });
					else
						chunk.setBlock({ x, y, z }, { BlockID::AIR });
				}
				chunk.setBlock({ x, bed, z }, { BlockID::SAND });
			}
		}

	// A flat 3-block cloud sheet with holes where the noise dips.
	constexpr int CLOUD_Y = 106;
	for (int x = 0; x < Const::SECTION_SIDE; ++x)
		for (int z = 0; z < Const::SECTION_SIDE; ++z) {
			dvec2 p{ (origin.x + x) * 0.008, (origin.z + z) * 0.008 };
			if (m_cloudNoise.getNoise(p) <= 0.1)
				continue;
			for (int y = CLOUD_Y; y < CLOUD_Y + 3; ++y)
				chunk.setBlock({ x, y, z }, { BlockID::CLOUD });
		}
}

bool TerrainShaper::lakeAt(ivec2 xz, int columnHeight) const {
	// Don't gouge a pond into a mountain face.
	if (columnHeight > LAKE_LEVEL + 2)
		return false;
	double core = m_lakeNoise.getNoise(static_cast<dvec2>(xz));
	double edge = m_lakeNoise.getNoise(static_cast<dvec2>(xz * 3));
	// Blended so pond edges are softer than one noise patch.
	return core * 0.8 + edge * 0.2 > 0.45;
}

void TerrainShaper::placePlant(Chunk& chunk, ivec3 pos) const {
	// ~45% of columns grow vegetation, 20% of those a flower.
	if (math::fnvHash01({ pos.x, pos.z }) >= 0.45)
		return;
	static const std::array<BlockID, 6> FLOWERS{
		BlockID::YELLOW_FLOWER, BlockID::RED_FLOWER, BlockID::PURPLE_FLOWER,
		BlockID::SUNFLOWER, BlockID::WHITE_FLOWER, BlockID::BLUE_FLOWER
	};
	BlockID plant = BlockID::TALL_GRASS;
	if (math::fnvHash01({ pos.x, pos.z, 1 }) < 0.2)
		plant = FLOWERS[static_cast<size_t>(math::fnvHash01({ pos.x, pos.z, 2 }) * FLOWERS.size()) % FLOWERS.size()];
	chunk.setBlock(pos, { plant });
}

std::optional<int> TerrainShaper::surfaceHeight(ivec2 xz, const ColumnInfo& col) const {
	CaveCarver::Column caveCol = m_caveCarver.column(xz, col.height);
	// Start above the highest ruggedness could reach.
	int top = col.height + static_cast<int>(std::ceil(std::abs(col.ruggedness))) + 2;
	std::optional<int> result;
	walkColumn(top, col.height, col.ruggedness, xz,
		[&](ivec3 pos) {
			return m_terrainNoise.getNoise(dvec3{ double(pos.x), pos.y * DENSITY_VERTICAL_SCALE, double(pos.z) });
		},
		[&](ivec3 pos, int depth) { return m_caveCarver.isCarvedPointwise(caveCol, pos, depth); },
		[&](ivec3 pos, int, ColumnBlock kind) {
			if (kind == ColumnBlock::SOLID) {
				result = pos.y + 1;
				return false;
			}
			return true;
		});
	return result;
}
