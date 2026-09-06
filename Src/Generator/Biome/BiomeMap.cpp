#include "BiomeMap.h"

#include "generator/Noise/PerlinNoise.h"
#include "maths/MiscMath.h"

#include <cmath>

#include "ExtremeMountains.h"
#include "Mountains.h"
#include "Desert.h"
#include "Snow.h"
#include "Forest.h"
#include "Plain.h"
#include "Ocean.h"
#include "Islands.h"
#include "Swamp.h"

#include "generator/Structure/Oak.h"
#include "generator/Structure/BigOak.h"
#include "generator/Structure/Willow.h"
#include "generator/Structure/Maple.h"
#include "generator/Structure/Cherry.h"
#include "generator/Structure/Fir.h"
#include "generator/Structure/Palm.h"
#include "generator/Structure/DeadTree.h"
#include "generator/Structure/Cactus.h"
#include "util/Logger.h"

BiomeMap::BiomeMap() {
	addBiome(std::make_unique<ExtremeMountains>(), BiomeID::EXTREME_MOUNTAINS, "Extreme mountains");
	addBiome(std::make_unique<Mountains>(), BiomeID::MOUNTAINS, "Mountains");
	addBiome(std::make_unique<Desert>(), BiomeID::DESERT, "Desert");
	addBiome(std::make_unique<Snow>(), BiomeID::SNOW, "Snow");
	addBiome(std::make_unique<Forest>(), BiomeID::FOREST, "Forest");
	addBiome(std::make_unique<Plain>(), BiomeID::PLAIN, "Plain");
	addBiome(std::make_unique<Ocean>(), BiomeID::OCEAN, "Ocean");
	addBiome(std::make_unique<Islands>(), BiomeID::ISLANDS, "Islands");
	addBiome(std::make_unique<Swamp>(), BiomeID::SWAMP, "Swamp");

	addStructure(std::make_unique<Oak>(), StructureID::OAK);
	addStructure(std::make_unique<BigOak>(), StructureID::BIG_OAK);
	addStructure(std::make_unique<Willow>(), StructureID::WILLOW);
	addStructure(std::make_unique<Maple>(), StructureID::MAPLE);
	addStructure(std::make_unique<Cherry>(), StructureID::CHERRY);
	addStructure(std::make_unique<Fir>(), StructureID::FIR);
	addStructure(std::make_unique<Palm>(), StructureID::PALM);
	addStructure(std::make_unique<DeadTree>(), StructureID::DEAD_TREE);
	addStructure(std::make_unique<Cactus>(), StructureID::CACTUS);
}

BiomeID BiomeMap::getBiomeID(ivec2 pos) const {
	return getColumn(pos).biome;
}

const Biome& BiomeMap::getBiome(ivec2 pos) const {
	return getBiome(getBiomeID(pos));
}

const Biome& BiomeMap::getBiome(BiomeID biomeID) const {
	return *m_biomes[static_cast<int>(biomeID)];
}

int BiomeMap::getHeight(ivec2 pos) const {
	return getColumn(pos).height;
}

ColumnInfo BiomeMap::getColumn(ivec2 pos) const {
	double temperature = getTemperature(pos), altitude = getAltitude(pos);

	// One pass over biomes: pick the dominant one and gather weights to blend with.
	std::array<double, static_cast<int>(BiomeID::SIZE)> weights;
	double sumValues = 0., maxValue = 0.;
	BiomeID dominant = BiomeID::SIZE;
	for (int i = 0; i < static_cast<int>(BiomeID::SIZE); ++i) {
		double value = m_biomes[i]->biomeValue(temperature, altitude);
		weights[i] = value;
		sumValues += value;
		if (value > maxValue) {
			maxValue = value;
			dominant = static_cast<BiomeID>(i);
		}
	}
	if (dominant == BiomeID::SIZE)
		throw "Biome not found";

	// Blend height and ruggedness by each biome's share of the total weight.
	double height = 0., ruggedness = 0.;
	for (int i = 0; i < static_cast<int>(BiomeID::SIZE); ++i) {
		if (weights[i] == 0.)
			continue;
		double share = weights[i] / sumValues;
		height += share * m_biomes[i]->getHeight(pos);
		ruggedness += share * m_biomes[i]->ruggedness();
	}
	return { dominant, static_cast<int>(height), ruggedness };
}

std::string BiomeMap::getBiomeName(ivec2 pos) const {
	return m_biome_names[static_cast<int>(getBiomeID(pos))];
}

const Structure& BiomeMap::getStructure(StructureID structureID) const {
	return *m_structures[static_cast<int>(structureID)];
}

void BiomeMap::addBiome(std::unique_ptr<Biome> biome, BiomeID biomeID, std::string name) {
	m_biomes[static_cast<int>(biomeID)] = std::move(biome);
	m_biome_names[static_cast<int>(biomeID)] = name;
}

void BiomeMap::addStructure(std::unique_ptr<Structure> structure, StructureID structureID) {
	m_structures[static_cast<int>(structureID)] = std::move(structure);
}

double BiomeMap::getTemperature(ivec2 pos) const {
	// Hotspot noise drives warmth, lapsed with altitude so mountains are colder.
	double base = m_hotspotNoise.getNoise(static_cast<dvec2>(pos + 37));
	double alt = getAltitude(pos);
	return 0.55 * base - 0.45 * std::clamp(alt, -1., 1.);
}

double BiomeMap::getAltitude(ivec2 pos) const {
	// Domain-warp the base so hills fold into curved masses instead of axial humps.
	dvec2 sample = static_cast<dvec2>(pos) + m_continentalWarp.getNoise(static_cast<dvec2>(pos)) * 320.;
	double base = m_continentalNoise.getNoise(sample);

	// A wide ridged band lifts coherent ranges; the belt mask keeps them as connected spines.
	dvec2 beltSample = static_cast<dvec2>(pos) * 0.0021;
	double belt = 0.5 + 0.5 * m_mountainBeltNoise.getNoise(beltSample);
	double ridge = m_mountainNoise.getRidgedNoise(sample + dvec2(911.0, 547.0));
	base += ridge * 0.9 * belt * 0.9;

	// Quantize the low band so shorelines shelf into gentle beach plateaus.
	if (base > -0.25 && base < 0.45) {
		double shelf = std::floor(base * 9.0) / 9.0;
		double blend = math::smoothstep((base + 0.25) / 0.7) * (1. - math::smoothstep((base - 0.45) / 0.7));
		base = base * (1. - blend) + shelf * blend;
	}

	// Normalize so the biome threshold curves saturate cleanly.
	return std::clamp(base, -1., 1.);
}
