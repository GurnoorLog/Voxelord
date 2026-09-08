#pragma once

#include <optional>
#include <vector>

#include "block/Block.h"
#include "world/Mesh.h"

class DefaultRenderer;
class Window;

class PickedBlockDrawer {
public:
    void render(std::optional<Block> picked, Window* p_window, DefaultRenderer& renderer);

private:
    DefaultMesh buildHeldBlockMesh(Block block);

	DefaultMesh m_heldBlockMesh;
	std::optional<BlockID> m_heldBlockId;
};
