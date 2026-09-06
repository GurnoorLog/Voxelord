#include "server/ServerWorld.h"

#include "maths/Converter.h"
#include "world/WorldConstants.h"
#include "physics/PlayerController.h"

#include <algorithm>

void ServerWorld::generateAround(ivec2 centerChunk, int radiusChunks, int budget) {
	// Center chunk first, so the player never stands on ungenerated ground while rings fill in.
	int generated = 0;
	if (budget <= 0 || generated < budget) {
		ensureChunk(centerChunk);
		++generated;
	}
	for (int r = 1; r <= radiusChunks; ++r) {
		for (int i = -r; i <= r; ++i) {
			for (ivec2 c : { ivec2{ i, -r }, ivec2{ i, r }, ivec2{ -r, i }, ivec2{ r, i } }) {
				ensureChunk(centerChunk + c);
				++generated;
				if (budget > 0 && generated >= budget)
					return;
			}
		}
	}
}

Chunk& ServerWorld::ensureChunk(ivec2 pos) {
	auto it = m_chunks.find(pos);
	if (it != m_chunks.end())
		return *it->second;

	auto chunk = std::make_shared<Chunk>(nullptr, pos);
	m_chunks.emplace(pos, chunk);
	m_generator.loadChunk(*chunk);
	return *chunk;
}

const Chunk* ServerWorld::findChunk(ivec2 pos) const {
	auto it = m_chunks.find(pos);
	return it == m_chunks.end() ? nullptr : it->second.get();
}

Block ServerWorld::getBlock(ivec3 globalPos) const {
	ivec2 chunkPos = Converter::globalToChunk(globalPos);
	const Chunk* chunk = findChunk(chunkPos);
	if (chunk == nullptr)
		return { BlockID::AIR };
	ivec2 inner = Converter::globalToInnerChunk2D(globalPos);
	return chunk->getBlock({ inner.x, globalPos.y, inner.y });
}

void ServerWorld::setBlock(ivec3 globalPos, Block block) {
	ivec2 chunkPos = Converter::globalToChunk(globalPos);
	Chunk& chunk = ensureChunk(chunkPos);
	ivec2 inner = Converter::globalToInnerChunk2D(globalPos);
	chunk.setBlock({ inner.x, globalPos.y, inner.y }, block);
}

std::optional<int> ServerWorld::surfaceHeight(ivec2 xz) const {
	ivec2 chunkPos = Converter::globalToChunk(ivec3{ xz.x, 0, xz.y });
	if (findChunk(chunkPos) == nullptr)
		return std::nullopt;
	for (int y = 255; y >= 0; --y) {
		Block block = getBlock({ xz.x, y, xz.y });
		if (m_blockDatas.get(block.id).isObstacle())
			return y + 1;
	}
	return std::nullopt;
}

vec3 ServerWorld::findSpawn() {
	const int maxRadius = 64;
	for (int r = 0; r <= maxRadius; ++r) {
		// Ring: all columns on the square of Chebyshev radius r.
		auto tryColumn = [&](int x, int z) -> std::optional<vec3> {
			generateAround(Converter::globalToChunk(ivec3{ x, 0, z }), 1);
			auto h = surfaceHeight({ x, z });
			if (h.has_value() && h.value() >= Const::SEA_LEVEL + 1) {
				// Feet rest on the surface at h so the eye sits head-height above it.
				Block above = getBlock({ x, h.value(), z });
				Block above2 = getBlock({ x, h.value() + 1, z });
				if (!m_blockDatas.get(above.id).isObstacle() &&
					!m_blockDatas.get(above2.id).isObstacle())
					return vec3{ float(x) + 0.5f, float(h.value()) + PlayerController::PLAYER_HEAD_HEIGHT, float(z) + 0.5f };
			}
			return std::nullopt;
		};
		for (int i = -r; i <= r; ++i) {
			if (auto s = tryColumn(i, -r)) return *s;
			if (auto s = tryColumn(i, r)) return *s;
			if (auto s = tryColumn(-r, i)) return *s;
			if (auto s = tryColumn(r, i)) return *s;
		}
	}
	return { 0.5f, 100.f, 0.5f };
}

int ServerWorld::generatedChunks() const {
	return static_cast<int>(m_chunks.size());
}