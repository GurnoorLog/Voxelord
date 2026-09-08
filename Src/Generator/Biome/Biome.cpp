#include "Biome.h"

#include "world/WorldConstants.h"
#include "maths/Converter.h"
#include "maths/MiscMath.h"

double Biome::threshold = 0.14, Biome::transition = 0.15;

Biome::Biome(const Config& config)
	: m_heightNoise{ config.octaves, config.persistence, config.frequency },
	  m_heightBase{ config.base }, m_heightAmplitude{ config.amplitude },
	  m_surface{ config.surface }, m_subsurface{ config.subsurface } {}

int Biome::getHeight(ivec2 pos) const {
	double noise = m_heightNoise.getNoise(static_cast<dvec2>(pos));
	return Const::SEA_LEVEL + m_heightBase + static_cast<int>(noise * m_heightAmplitude);
}

Block Biome::getBlock(ivec3 pos, int depth) const {
	return layeredGround(pos, depth, m_surface, m_subsurface);
}

int Biome::ridgedHeight(ivec2 pos, const OctavePerlin& warp, const OctavePerlin& ridge,
		double warpScale, int base, double amplitude) const {
	dvec2 p = static_cast<dvec2>(pos);
	// Warp the lookup so ridges meander instead of looking grid-aligned.
	dvec2 offset{ warp.getNoise(p), warp.getNoise(p + 137.5) };
	double value = ridge.getRidgedNoise(p + offset * warpScale);
	return Const::SEA_LEVEL + base + static_cast<int>(value * amplitude);
}

Block Biome::layeredGround(ivec3 pos, int depth, BlockID surface, BlockID subsurface) {
	if (pos.y <= Const::SEA_LEVEL && depth <= 2)
		return { BlockID::SAND };
	if (depth == 0)
		return { surface };
	if (depth <= 3)
		return { subsurface };
	return { BlockID::STONE };
}

double Biome::low(double value) {
	// 1 -> 0 over [-threshold, -threshold + transition]
	return step(value, -threshold);
}

double Biome::medium(double value) {
	// 0 -> 1 over [-threshold - transition, -threshold] then 1 -> 0 over [threshold, threshold + transition]
	return step(-value, threshold) * step(value, threshold);
}

double Biome::high(double value) {
	// 0 -> 1 over [threshold - transition, threshold]
	return step(-value, -threshold);
}

double Biome::step(double value, double threshold) {
	double diff = threshold + transition - value;
	if (diff > transition) return 1.;
	// threshold <= value < threshold + transition
	if (diff > 0) return math::smoothstep(diff / transition);
	return 0.;
}
