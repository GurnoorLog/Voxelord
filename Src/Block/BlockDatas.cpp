#include "BlockDatas.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
	void loadAll(BlockDatas& datas, const std::vector<TextureArray*>& texArrays, bool resolveTextures) {
		std::string blocksPath = "Data/Blocks";
		for (const fs::directory_entry& entry : fs::directory_iterator(blocksPath)) {
			std::ifstream ifs(entry.path());
			json j;
			ifs >> j;
			std::string filename = entry.path().stem().string();
			BlockID id = BlockID::_from_string_nocase(filename.c_str());
			datas.get(id) = resolveTextures ? BlockData(j, texArrays) : BlockData(j);
		}
	}
}

BlockDatas::BlockDatas(const std::vector<TextureArray*>& texArrays) {
	loadAll(*this, texArrays, true);
}

BlockDatas::BlockDatas(bool withoutTextures) {
	loadAll(*this, {}, !withoutTextures);
}

const BlockData& BlockDatas::get(BlockID id) const {
	return blockDatas[id];
}

BlockData & BlockDatas::get(BlockID id) {
	return blockDatas[id];
}
