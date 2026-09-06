#include "BlockData.h"

#include "util/Logger.h"
#include "BlockID.h"

BlockData::BlockData() {}

int findID(std::string name, const std::vector<TextureArray*>& texArrays) {
	for (TextureArray* texArray : texArrays) {
		if (texArray->containsTexture(name)) {
			return texArray->getTextureID(name);
		}
	}
	throw "Bad texture name : " + name;
}

namespace {
	// The block properties common to every load path (textures need a GL context, so headless
	// loads skip them). Unknown texture names can't be resolved without texture arrays, so the
	// with-textures path is the only one allowed to throw.
}
void parseCommonFields(BlockData& data, const json& j) {
	data.m_opaque = j.value("opaque", false);
	data.m_obstacle = j.value("obstacle", false);
	data.m_resistance = j.value("resistance", 0);
	data.m_emission = j.value("emission", 0);
	std::string category = j.value("category", "");
	if (category == "default")
		data.m_category = BlockData::DEFAULT;
	else if (category == "water")
		data.m_category = BlockData::WATER;
	else if (category == "lava")
		data.m_category = BlockData::LAVA;
	else if (category == "air")
		data.m_category = BlockData::AIR;
	else if (category == "semi-transparent")
		data.m_category = BlockData::SEMI_TRANSPARENT;
	else if (category == "plant")
		data.m_category = BlockData::PLANT;
}

BlockData::BlockData(const json& j, const std::vector<TextureArray*>& texArrays) {
	parseCommonFields(*this, j);

	json::const_iterator it;
	it = j.find("textures");
	if (it != j.end()) {
		auto texJ = *it;
		it = texJ.find("all");
		if (it != texJ.end()) {
			for (Dir3D::Dir dir : Dir3D::all()) {
				m_textures[dir] = findID(*it, texArrays);
			}
		}
		it = texJ.find("side");
		if (it != texJ.end()) {
			for (Dir3D::Dir dir : Dir3D::all_horizontal()) {
				m_textures[dir] = findID(*it, texArrays);
			}
		}
		it = texJ.find("up");
		if (it != texJ.end())
			m_textures[Dir3D::UP] = findID(*it, texArrays);
		it = texJ.find("front");
		if (it != texJ.end())
			m_textures[Dir3D::FRONT] = findID(*it, texArrays);
		it = texJ.find("right");
		if (it != texJ.end())
			m_textures[Dir3D::RIGHT] = findID(*it, texArrays);
		it = texJ.find("down");
		if (it != texJ.end())
			m_textures[Dir3D::DOWN] = findID(*it, texArrays);
		it = texJ.find("back");
		if (it != texJ.end())
			m_textures[Dir3D::BACK] = findID(*it, texArrays);
		it = texJ.find("left");
		if (it != texJ.end())
			m_textures[Dir3D::LEFT] = findID(*it, texArrays);
	}
}

BlockData::BlockData(const json& j) {
	parseCommonFields(*this, j);
	m_textures.fill(0);
}

bool BlockData::isOpaque() const {
	return m_opaque;
}

bool BlockData::isObstacle() const {
	return m_obstacle;
}

int BlockData::getResistance() const {
	return m_resistance;
}

int BlockData::getEmission() const {
	return m_emission;
}

BlockData::Category BlockData::getCategory() const {
	return m_category;
}

int BlockData::getTexture(Dir3D::Dir dir) const {
	return m_textures[dir];
}