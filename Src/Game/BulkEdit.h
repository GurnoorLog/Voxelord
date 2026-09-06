#pragma once

#include "maths/GlmCommon.h"
#include "block/Block.h"
#include "world/BlockEdit.h"

#include <vector>

namespace BulkEdit {
	std::vector<BlockEdit> smoothSphere(ivec3 center, int radius, Block block);
}
