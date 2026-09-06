#pragma once

#include "maths/GlmCommon.h"
#include "block/Block.h"

// A single block change: the block to write and where (global coordinates).
// Bulk operations produce a vector of these; ChunkMap::applyEdits consumes them.
struct BlockEdit {
	ivec3 pos;
	Block block;
};
