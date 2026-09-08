#include "ExtremeMountains.h"

#include "generator/Noise/PerlinNoise.h"
#include "world/WorldConstants.h"
#include "maths/Converter.h"

int ExtremeMountains::getHeight(ivec2 pos) const {
	return ridgedHeight(pos, warp, perlin, 80., 48, 200);
}

Block ExtremeMountains::getBlock(ivec3 pos, int depth) const {
	// Snow caps the surface above a noisy snow line, rock everywhere below
	double snowLine = 170 + snowPerlin.getNoise(Converter::to2D(pos)) * 30;
	if (pos.y > snowLine && depth <= 3)
		return { BlockID::SNOW };
	return { BlockID::STONE };
}

std::vector<StructureInfo> ExtremeMountains::getStructures() const {
	return { { StructureID::OAK, 0.005f } };
}

double ExtremeMountains::biomeValue(double temperature, double altitude) const {
	return high(temperature) * high(altitude);
}