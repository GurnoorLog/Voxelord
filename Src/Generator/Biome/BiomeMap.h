#pragma once

#include "generator/Biome/Biome.h"
#include "generator/Biome/BiomeID.h"
#include "generator/Structure/Structure.h"
#include "generator/Structure/StructureID.h"
#include "generator/Noise/OctavePerlin.h"
#include "maths/GlmCommon.h"

#include <memory>
#include <array>

// Everything the generator needs about a terrain column, computed in one pass.
struct ColumnInfo {
	BiomeID biome;
	int height;
	double ruggedness;
};

class BiomeMap {
public:
	BiomeMap();
	BiomeID getBiomeID(ivec2 pos) const;
	const Biome& getBiome(ivec2 pos) const;
	const Biome& getBiome(BiomeID biomeID) const;
	int getHeight(ivec2 pos) const;
	ColumnInfo getColumn(ivec2 pos) const;
	std::string getBiomeName(ivec2 pos) const;
	const Structure& getStructure(StructureID structureID) const;

private:
	std::array<std::unique_ptr<Biome>, static_cast<int>(BiomeID::SIZE)> m_biomes;
	std::array<std::string, static_cast<int>(BiomeID::SIZE)> m_biome_names;
	std::array<std::unique_ptr<Structure>, static_cast<int>(StructureID::SIZE)> m_structures;

	void addBiome(std::unique_ptr<Biome> biome, BiomeID biomeID, std::string name);
	void addStructure(std::unique_ptr<Structure> structure, StructureID structureID);
	double getTemperature(ivec2 pos) const;
	double getAltitude(ivec2 pos) const;

	// Base hills, domain-warped so coastlines bend and no two regions mirror each other.
	OctavePerlin m_continentalNoise{ 5, 0.5, 1. / 2048. };
	// Warps the continental samples by itself so hill crests become curved rather than axial.
	OctavePerlin m_continentalWarp{ 2, 0.5, 1. / 1400. };
	// Broad ridged spine signal; the mountain belts ride on top of the base elevation.
	OctavePerlin m_mountainNoise{ 5, 0.55, 1. / 1100. };
	// Slow, huge-scale warmth bands that set the overall climate gradient across the map.
	OctavePerlin m_hotspotNoise{ 3, 0.5, 1. / 3072. };
	// Belt that localizes where mountain ranges sit, so they're long and connected, not speckled.
	OctavePerlin m_mountainBeltNoise{ 2, 0.5, 1. / 2200. };
};

