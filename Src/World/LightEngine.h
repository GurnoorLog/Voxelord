#pragma once

#include "maths/GlmCommon.h"
#include "maths/Dir2D.h"
#include "block/Block.h"

#include <array>
#include <vector>
#include <utility>

class Chunk;

namespace LightEngine {
	// Sky + block light for a freshly generated chunk, no neighbor light yet. Cheap and
	// self-contained; cross-chunk light gets added later by spillBorderLight.
	void computeChunkLight(Chunk& chunk);

	// Pushes a chunk's border light into its neighbors and pulls theirs back in before meshing.
	// Only ever brightens a cell, so order doesn't matter. Changed neighbor sections go to
	// dirtySections for the caller to remesh.
	//
	// Caller must hold the light mutex: writes several chunks at once.
	void spillBorderLight(Chunk& chunk, const std::array<Chunk*, Dir2D::SIZE>& neighbors,
		std::vector<ivec3>& dirtySections);

	// One block change: where, and the block before/after. oldBlock keeps the old light so we can
	// take it back out; newBlock is the new opacity/glow. Caller writes newBlock with light byte zeroed.
	struct LightEdit {
		ivec3 pos;
		Block oldBlock;
		Block newBlock;
	};

	// Fixes light after a batch of edits: takes back the old light, then re-spreads from what's left.
	// chunks is the set the update may touch (edited chunks + neighbors); light beyond it is dropped.
	// Sections are pinned up front, so no locks. Changed sections go to dirtySections to remesh.
	//
	// Caller must hold the light mutex, like spillBorderLight.
	void updateEditLight(const std::vector<std::pair<ivec2, Chunk*>>& chunks,
		const std::vector<LightEdit>& edits, std::vector<ivec3>& dirtySections);
}
