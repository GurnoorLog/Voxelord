#pragma once

#include "BlockData.h"
#include "BlockID.h"

#include <array>

class BlockDatas {
public:
	BlockDatas() = default;
	BlockDatas(const std::vector<TextureArray*>& texArrays);
	// Physics-only load: parses every block JSON without resolving textures, for headless servers.
	explicit BlockDatas(bool withoutTextures);

	const BlockData& get(BlockID id) const;
	BlockData& get(BlockID id);

private:
	std::array<BlockData, BlockID::SIZE> blockDatas;
};

