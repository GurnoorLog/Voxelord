#pragma once

#include <physics/IWorldView.h>
#include <world/ChunkMap.h>
#include "resources/ResManager.h"

// IWorldView adapter over the client's ChunkMap, so PlayerController can run prediction
// against the render world on the game thread.
class ChunkWorldView : public IWorldView {
public:
	ChunkWorldView() : m_chunkMap{ nullptr } {}
	explicit ChunkWorldView(const ChunkMap* chunkMap) : m_chunkMap{ chunkMap } {}

	void setChunkMap(const ChunkMap* chunkMap) { m_chunkMap = chunkMap; }

	Block getBlock(ivec3 globalPos) const override {
		return m_chunkMap->getBlock(globalPos);
	}

	const BlockData& blockData(BlockID id) const override {
		return ResManager::blockDatas().get(id);
	}

private:
	const ChunkMap* m_chunkMap;
};