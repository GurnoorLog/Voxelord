#pragma once

#include "GlmCommon.h"
#include "Converter.h"
#include "util/DynamicArray3D.h"

#include <algorithm>

namespace math
{
	inline int manhattan(ivec2 pos1, ivec2 pos2) {
		return std::abs(pos2.x - pos1.x) + std::abs(pos2.y - pos1.y);
	}

	inline int euclidianPow2(ivec2 pos1, ivec2 pos2) {
		int dx = pos1.x - pos2.x;
		int dy = pos1.y - pos2.y;
		return dx * dx + dy * dy;
	}

	inline double lerp(double t, double a, double b) {
		return a + t * (b - a);
	}

	// Perlin's quintic smoothstep.
	inline double smoothstep(double x) {
		x = std::clamp(x, 0., 1.);
		return x * x * x * (x * (x * 6. - 15.) + 10.);
	}

	// Upsample 3D noise: trilerp between the 8 lattice nodes around a cell.
	inline double trilerp(const DynamicArray3D<double>& f, ivec3 cell, dvec3 frac) {
		int i = cell.x, j = cell.y, k = cell.z;
		double x00 = lerp(frac.x, f.at({ i, j, k }),         f.at({ i + 1, j, k }));
		double x10 = lerp(frac.x, f.at({ i, j + 1, k }),     f.at({ i + 1, j + 1, k }));
		double x01 = lerp(frac.x, f.at({ i, j, k + 1 }),     f.at({ i + 1, j, k + 1 }));
		double x11 = lerp(frac.x, f.at({ i, j + 1, k + 1 }), f.at({ i + 1, j + 1, k + 1 }));
		double y0 = lerp(frac.y, x00, x10), y1 = lerp(frac.y, x01, x11);
		return lerp(frac.z, y0, y1);
	}

	// World pos resolved onto the coarse noise lattice: lower-corner node + fractional offset.
	struct LatticeCoord { ivec3 cell; dvec3 frac; };
	inline LatticeCoord latticeCoord(ivec3 pos, ivec3 spacing) {
		return { floorDiv(pos, spacing), dvec3(posMod(pos, spacing)) / dvec3(spacing) };
	}

	inline uint32_t fnvHash(const std::vector<int>& values) {
		uint32_t h = 2166136261u;
		for (int v : values) {
			h = (h ^ static_cast<uint32_t>(v)) * 16777619u;
		}
		return h;
	}

	inline double fnvHash01(const std::vector<int>& values) {
		return double(fnvHash(values)) / std::numeric_limits<uint32_t>::max();
	}
}
