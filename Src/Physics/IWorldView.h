#pragma once

#include "Maths/GlmCommon.h"
#include "Block/Block.h"
#include "Block/BlockData.h"

// Everything physics needs from the world: a block lookup by global position and the block
// properties (obstacle, fluid category, ...) that decide collisions. Implemented by the client's
// ChunkMap (through ChunkWorldView) and the server's ServerWorld, so the shared PlayerController
// runs identical physics on both sides without sharing the client's texture-loaded block data.
class IWorldView {
public:
	virtual ~IWorldView() = default;
	virtual Block getBlock(ivec3 globalPos) const = 0;
	virtual const BlockData& blockData(BlockID id) const = 0;
};