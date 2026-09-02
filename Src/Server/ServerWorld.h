#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "Maths/GlmCommon.h"
#include "World/Chunk.h"
#include "Physics/IWorldView.h"
#include "Generator/WorldGenerator.h"
#include "Block/BlockDatas.h"

// The server's authoritative world store: a plain chunk map generated deterministically from the
// same generator the client uses. Implements IWorldView so the shared PlayerController drives server
// physics. Not thread-safe: only the server touches it, on one thread.
class ServerWorld : public IWorldView {
public:
	// Ensure every chunk within radiusChunks of centerChunk (Chebyshev), center first, then stop after
	// budget new chunks so a hot loop can grow the world a little per call. budget <= 0 = unlimited.
	void generateAround(ivec2 centerChunk, int radiusChunks, int budget = -1);

	Block getBlock(ivec3 globalPos) const override;
	void setBlock(ivec3 globalPos, Block block);
	const BlockData& blockData(BlockID id) const override {
		return m_blockDatas.get(id);
	}

	// Highest obstacle block's top in the column, or nullopt if none.
	std::optional<int> surfaceHeight(ivec2 xz) const;
	// A spot on land above sea level. May generate chunks while scanning, so not const.
	vec3 findSpawn();

	int generatedChunks() const;

private:
	WorldGenerator m_generator;
	// Physics-only block properties (no textures), loaded at construction.
	BlockDatas m_blockDatas{ true };
	std::unordered_map<ivec2, std::shared_ptr<Chunk>, Comp_ivec2, Comp_ivec2> m_chunks;

	Chunk& ensureChunk(ivec2 pos);
	const Chunk* findChunk(ivec2 pos) const;
};