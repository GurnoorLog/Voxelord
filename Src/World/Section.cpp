#include "Section.h"

#include "Generator/Noise/OctavePerlin.h"
#include "Maths/Converter.h"
#include "Chunk.h"
#include "Util/DebugGL.h"
#include "ResManager/ResManager.h"
#include "Util/Logger.h"
#include "CubeData.h"

#include <iostream>

Section::Section(const Chunk* chunk, ivec3 position)
	: p_chunk{ chunk }, m_position{ position },
	m_blocks{ std::make_unique<BlockArray>() }
{ }

int dirToBin(ivec3 dir) {
	dir += 1;
	return dir.x + (dir.y << 2) + (dir.z << 4);
}

const Section* Section::findNeighboringSection(Dir3D::Dir dir, const NeighborChunks& neighbors) const {
	const ivec3 v = Dir3D::to_ivec3(dir);
	const Chunk* neighborChunk = neighbors.at(v.x, v.z);
	if (neighborChunk == nullptr)
		return nullptr;
	return neighborChunk->tryGetSection(m_position.y + v.y);
}

std::tuple<std::vector<DefaultMesh::Vertex>, std::vector<WaterMesh::Vertex>, std::vector<LavaMesh::Vertex> > Section::findVisibleFaces(const NeighborChunks& neighbors) const {
	std::vector<DefaultMesh::Vertex> defaultVertices;
	std::vector<WaterMesh::Vertex> waterVertices;
	std::vector<LavaMesh::Vertex> lavaVertices;
	
	// Axes are indexed y = 0, x = 1, z = 2.
	const std::array<ivec3, 3> AXIS_ORDER{ {
		{ 1, 0, 2 },
		{ 0, 1, 2 },
		{ 2, 1, 0 }
	} };
	const ivec3 SECTION_BOUNDS{ Const::SECTION_SIDE, Const::SECTION_HEIGHT, Const::SECTION_SIDE };

	for (Dir3D::Dir dir : Dir3D::all()) {
		const int axis = dir % 3;

		// Done once per direction because quite costly
		const Section* neighboringSection = findNeighboringSection(dir, neighbors);

		// Axis iteration order; order[0] first, order[2] last
		const ivec3 order = AXIS_ORDER[axis];

		ivec3 orderedBounds;
		for (int i = 0; i < 3; ++i)
			orderedBounds[i] = SECTION_BOUNDS[order[i]];

		ivec3 localPos;
		for (localPos[order[0]] = 0; localPos[order[0]] < orderedBounds[0]; ++localPos[order[0]]) {
			for (localPos[order[1]] = 0; localPos[order[1]] < orderedBounds[1]; ++localPos[order[1]]) {
				addVisibleFacesOnLastAxis(defaultVertices, waterVertices, lavaVertices, dir, localPos, order[2],
						orderedBounds[2], neighboringSection, neighbors);
			}
		}
	}
	addPlantFaces(defaultVertices);
	return { defaultVertices, waterVertices, lavaVertices };
}

void Section::addPlantFaces(std::vector<DefaultMesh::Vertex>& defaultVertices) const {
	// Plants are two crossed planes, each a full-block quad facing outward.
	static const std::array<std::pair<ivec3, std::array<vec3, 4>>, 4> PLANT_QUADS{ {
		{ Dir3D::to_ivec3(Dir3D::LEFT),  { { { 0.5f, 0.f, 0.f }, { 0.5f, 1.f, 0.f }, { 0.5f, 1.f, 1.f }, { 0.5f, 0.f, 1.f } } } },
		{ Dir3D::to_ivec3(Dir3D::RIGHT), { { { 0.5f, 0.f, 1.f }, { 0.5f, 1.f, 1.f }, { 0.5f, 1.f, 0.f }, { 0.5f, 0.f, 0.f } } } },
		{ Dir3D::to_ivec3(Dir3D::BACK),   { { { 1.f, 0.f, 0.5f }, { 1.f, 1.f, 0.5f }, { 0.f, 1.f, 0.5f }, { 0.f, 0.f, 0.5f } } } },
		{ Dir3D::to_ivec3(Dir3D::FRONT),  { { { 0.f, 0.f, 0.5f }, { 0.f, 1.f, 0.5f }, { 1.f, 1.f, 0.5f }, { 1.f, 0.f, 0.5f } } } }
	} };

	const ivec3 SECTION_BOUNDS{ Const::SECTION_SIDE, Const::SECTION_HEIGHT, Const::SECTION_SIDE };
	ivec3 localPos;
	for (localPos.y = 0; localPos.y < SECTION_BOUNDS.y; ++localPos.y) {
		for (localPos.x = 0; localPos.x < SECTION_BOUNDS.x; ++localPos.x) {
			for (localPos.z = 0; localPos.z < SECTION_BOUNDS.z; ++localPos.z) {
				const Block block = getBlock(localPos);
				if (ResManager::blockDatas().get(block.id).getCategory() != BlockData::PLANT)
					continue;

				const ivec3 globalPos = Converter::sectionToGlobal(m_position) + localPos;
				const GLuint texID = ResManager::blockDatas().get(block.id).getTexture(Dir3D::FRONT);
				// A plant is a single-cell cross, so it reads its own air cell's light.
				const vec2 light{ block.light.sky() / 15.f, block.light.block() / 15.f };

				for (const auto& plantQuad : PLANT_QUADS) {
					const ivec3& normal = plantQuad.first;
					const std::array<vec3, 4>& quad = plantQuad.second;
					for (int vtx = 0; vtx < 4; ++vtx) {
						defaultVertices.push_back({ quad[vtx] + vec3(globalPos), CubeData::faceCoords[vtx], normal, texID, 1.f, light });
					}
				}
			}
		}
	}
}

void Section::addVisibleFacesOnLastAxis(
	std::vector<DefaultMesh::Vertex>& defaultVertices, std::vector<WaterMesh::Vertex>& waterVertices,
	std::vector<LavaMesh::Vertex>& lavaVertices,
	Dir3D::Dir dir, ivec3 localPos, int indexOfLastAxis, int sizeOfLastAxis,
	const Section* neighboringSection, const NeighborChunks& neighbors) const {

	const ivec3 dirVec = Dir3D::to_ivec3(dir);
	const std::array<GLfloat, 4> noAO{ 1.f, 1.f, 1.f, 1.f };
	const std::array<vec2, 4> fullLight{ { { 1.f, 0.f }, { 1.f, 0.f }, { 1.f, 0.f }, { 1.f, 0.f } } };
	Block lastBlock{ BlockID::AIR };
	std::array<GLfloat, 4> lastAO{ noAO };
	std::array<vec2, 4> lastLight{ fullLight };
	int firstBlockIndex = -1;

	for (localPos[indexOfLastAxis] = 0; localPos[indexOfLastAxis] < sizeOfLastAxis; ++localPos[indexOfLastAxis]) {
		Block currBlock{ BlockID::AIR };
		std::array<GLfloat, 4> currAO{ noAO };
		std::array<vec2, 4> currLight{ fullLight };

		const Block block = this->getBlock(localPos);
		if (block.id != +BlockID::AIR) {
			const ivec3 localNeighPos{ localPos + dirVec };
			Block neighBlock{ BlockID::AIR };
			// The neighbor is in this section or the adjacent one; a missing section is air, so edge faces stay visible.
			if (isInSection(localNeighPos)) {
				neighBlock = this->getBlock(localNeighPos);
			}
			else if (neighboringSection != nullptr) {
				neighBlock = neighboringSection->getBlock(Converter::globalToInnerSection(localNeighPos));
			}
			// Face is visible only if the block ahead is transparent and not the same block
			if (!ResManager::blockDatas().get(neighBlock.id).isOpaque() && neighBlock.id != block.id) {
				currBlock = block;
				// AO and smooth light share the corner neighborhood, so computed together.
				std::tie(currAO, currLight) = computeFaceLighting(neighbors, dir, localPos);
			}
		}
		// End the face when the next block, AO or light differs.
		if (currBlock.id != lastBlock.id || currAO != lastAO || currLight != lastLight) {
			addFace(defaultVertices, waterVertices, lavaVertices, dir, localPos, firstBlockIndex, lastBlock, indexOfLastAxis, lastAO, lastLight);
			firstBlockIndex = localPos[indexOfLastAxis];
			lastBlock = currBlock;
			lastAO = currAO;
			lastLight = currLight;
		}
	}
	addFace(defaultVertices, waterVertices, lavaVertices, dir, localPos, firstBlockIndex, lastBlock, indexOfLastAxis, lastAO, lastLight);
}

void Section::addFace(std::vector<DefaultMesh::Vertex>& defaultVertices, std::vector<WaterMesh::Vertex>& waterVertices,
	std::vector<LavaMesh::Vertex>& lavaVertices,
	Dir3D::Dir dir, ivec3 localPos, int firstBlockIndex, Block block, int indexOfLastAxis,
	const std::array<GLfloat, 4>& ao, const std::array<vec2, 4>& light) const {
	BlockData::Category category = ResManager::blockDatas().get(block.id).getCategory();
	if (category != BlockData::AIR) {
		ivec3 negativeLastAxis{ 0, 0, 0 };
		negativeLastAxis[indexOfLastAxis] = -1;
		int length = localPos[indexOfLastAxis] - firstBlockIndex;
		ivec3 firstBlockGlobalPos{ Converter::sectionToGlobal(m_position) + localPos + negativeLastAxis * length };

		if (category == BlockData::DEFAULT || category == BlockData::SEMI_TRANSPARENT) {
			addDefaultFace(defaultVertices, dir, block, indexOfLastAxis, length, firstBlockGlobalPos, ao, light);
		}
		else if (category == BlockData::WATER) {
			addWaterFace(waterVertices, dir, indexOfLastAxis, length, firstBlockGlobalPos);
		}
		else if (category == BlockData::LAVA) {
			addLavaFace(lavaVertices, dir, indexOfLastAxis, length, firstBlockGlobalPos);
		}
	}
}

void Section::addDefaultFace(std::vector<DefaultMesh::Vertex>& defaultVertices, Dir3D::Dir dir, Block block,
	int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos, const std::array<GLfloat, 4>& ao, const std::array<vec2, 4>& light) const {

	GLuint texID = ResManager::blockDatas().get(block.id).getTexture(dir);
	for (int vtx = 0; vtx < 4; ++vtx) {
		vec3 currVtx = CubeData::dirToFace[dir][vtx];
		currVtx[indexOfLastAxis] *= length;

		vec2 tex = CubeData::faceCoords[vtx];
		if (indexOfLastAxis == 1) {
			tex.y *= length;
		} else {
			tex.x *= length;
		}
		defaultVertices.push_back({ currVtx + vec3(firstBlockGlobalPos), tex, Dir3D::to_ivec3(dir), texID, ao[vtx], light[vtx] });
	}
}

void Section::addWaterFace(std::vector<WaterMesh::Vertex>& waterVertices, Dir3D::Dir dir,
	int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos) const {

	auto addWaterFaceInDir = [&indexOfLastAxis, &length, &waterVertices](Dir3D::Dir dir, ivec3 firstBlockGlobalPos) {
		for (int vtx = 0; vtx < 4; ++vtx) {
			vec3 currVtx = CubeData::dirToFace[dir][vtx];
			currVtx[indexOfLastAxis] *= length;
			waterVertices.push_back({ currVtx + vec3(firstBlockGlobalPos), Dir3D::to_ivec3(dir) });
		}
	};
	addWaterFaceInDir(dir, firstBlockGlobalPos);
	addWaterFaceInDir(Dir3D::opp(dir), firstBlockGlobalPos + Dir3D::to_ivec3(dir));
}

void Section::addLavaFace(std::vector<LavaMesh::Vertex>& lavaVertices, Dir3D::Dir dir,
	int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos) const {

	for (int vtx = 0; vtx < 4; ++vtx) {
		vec3 currVtx = CubeData::dirToFace[dir][vtx];
		currVtx[indexOfLastAxis] *= length;
		lavaVertices.push_back({ currVtx + vec3(firstBlockGlobalPos), Dir3D::to_ivec3(dir) });
	}
}

std::pair<std::array<GLfloat, 4>, std::array<vec2, 4>> Section::computeFaceLighting(
		const NeighborChunks& neighbors, Dir3D::Dir dir, ivec3 localPos) const {
	const ivec3 n = Dir3D::to_ivec3(dir);
	// The axis the face points along, plus the two axes spanning the face plane.
	const int normalAxis = (n.x != 0) ? 0 : (n.y != 0 ? 1 : 2);
	const int u = (normalAxis + 1) % 3;
	const int v = (normalAxis + 2) % 3;
	const ivec3 base = localPos + n;

	// Brightness per occlusion level: 0 = corner tucked between two blocks, 3 = open.
	static const std::array<GLfloat, 4> levelBrightness{ 0.5f, 0.7f, 0.85f, 1.f };

	auto sample = [&](ivec3 pos) -> std::pair<vec2, bool> {
		const Block b = getNeigboringBlock(neighbors, pos);
		const bool opaque = ResManager::blockDatas().get(b.id).isOpaque();
		return { vec2{ b.light.sky() / 15.f, b.light.block() / 15.f }, opaque };
	};

	const vec2 baseLight = sample(base).first;

	std::array<GLfloat, 4> ao;
	std::array<vec2, 4> light;
	for (int vtx = 0; vtx < 4; ++vtx) {
		const vec3 corner = CubeData::dirToFace[dir][vtx];
		ivec3 du{ 0, 0, 0 }; du[u] = (corner[u] == 1) ? 1 : -1;
		ivec3 dv{ 0, 0, 0 }; dv[v] = (corner[v] == 1) ? 1 : -1;

		const auto [side1Light, side1Opaque] = sample(base + du);
		const auto [side2Light, side2Opaque] = sample(base + dv);
		const auto [cornLight, cornOpaque] = sample(base + du + dv);

		// Two side blocks fully close the corner; otherwise darken by how many cells are solid.
		const int level = (side1Opaque && side2Opaque)
				? 0 : (3 - (side1Opaque + side2Opaque + cornOpaque));
		ao[vtx] = levelBrightness[level];

		// Smooth light: average the open cells around the corner. The diagonal counts only if a side is open.
		vec2 sum = baseLight;
		int count = 1;
		if (!side1Opaque) { sum += side1Light; ++count; }
		if (!side2Opaque) { sum += side2Light; ++count; }
		if (!(side1Opaque && side2Opaque) && !cornOpaque) { sum += cornLight; ++count; }
		light[vtx] = sum / float(count);
	}
	return { ao, light };
}

Block Section::getNeigboringBlock(const NeighborChunks& neighbors, ivec3 localPos) const {
	if (isInSection(localPos))
		return getBlock(localPos);

	auto axisCrossing = [](int coord) {
		return coord < 0 ? -1 : (coord >= Const::SECTION_SIDE ? 1 : 0);
	};

	// Resolve the horizontal crossing through the 3x3 grid; an unsnapshotted neighbor reads as air.
	const Chunk* chunk = neighbors.at(axisCrossing(localPos.x), axisCrossing(localPos.z));
	if (chunk == nullptr)
		return { BlockID::AIR };
	localPos.x = (localPos.x + Const::SECTION_SIDE) % Const::SECTION_SIDE;
	localPos.z = (localPos.z + Const::SECTION_SIDE) % Const::SECTION_SIDE;

	// Vertical crossing into another stacked section of the resolved chunk.
	const int globalY = m_position.y * Const::SECTION_HEIGHT + localPos.y;
	return chunk->getBlock({ localPos.x, globalY, localPos.z });
}

std::vector<GLuint> getIndices(int size) {
	std::vector<GLuint> indices;
	for (int faceIndex = 0; faceIndex < size; ++faceIndex)
		for (GLuint rectIndex : CubeData::faceElementIndices)
			indices.push_back(4 * faceIndex + rectIndex);
	return indices;
}

void Section::loadMesh(const NeighborChunks& neighbors) {
	// CPU only; runs on a worker thread, so no OpenGL here.
	auto [defaultVertices, waterVertices, lavaVertices] = findVisibleFaces(neighbors);
	std::lock_guard<std::mutex> lock(m_meshMutex);
	m_nextDefaultVertices = std::move(defaultVertices);
	m_nextWaterVertices = std::move(waterVertices);
	m_nextLavaVertices = std::move(lavaVertices);
	m_meshReady = true;
}

void Section::uploadMesh() {
	// All OpenGL for the mesh happens here, on the main thread
	std::lock_guard<std::mutex> lock(m_meshMutex);
	if (!m_meshReady)
		return;
	nextDefaultMesh.loadBuffers(m_nextDefaultVertices, getIndices((int)m_nextDefaultVertices.size() / 4));
	nextWaterMesh.loadBuffers(m_nextWaterVertices, getIndices((int)m_nextWaterVertices.size() / 4));
	nextLavaMesh.loadBuffers(m_nextLavaVertices, getIndices((int)m_nextLavaVertices.size() / 4));
	nextDefaultMesh.loadVAOs();
	nextWaterMesh.loadVAOs();
	nextLavaMesh.loadVAOs();

	// Assigning frees the previous mesh's GPU resources: clear() runs on the old value first.
	activeDefaultMesh = std::move(nextDefaultMesh);
	activeWaterMesh = std::move(nextWaterMesh);
	activeLavaMesh = std::move(nextLavaMesh);

	m_nextDefaultVertices = {};
	m_nextWaterVertices = {};
	m_nextLavaVertices = {};
	m_meshReady = false;

	Debug::glCheckError();
}

void Section::releaseMesh() {
	activeDefaultMesh.clear();
	activeWaterMesh.clear();
	activeLavaMesh.clear();
}

void Section::render(const Renderer& renderer) const {
	renderer.render(*this);
}

const DefaultMesh& Section::getDefaultMesh() const {
	return activeDefaultMesh;
}

const WaterMesh& Section::getWaterMesh() const {
	return activeWaterMesh;
}

const LavaMesh& Section::getLavaMesh() const {
	return activeLavaMesh;
}

ivec3 Section::getPosition() const {
	return m_position;
}

void Section::setBlock(ivec3 pos, Block block) {
	m_blocks->at(pos) = block;
}

Block Section::getBlock(ivec3 pos) const {
	return m_blocks->at(pos);
}

Block& Section::getBlock(ivec3 pos) {
	return m_blocks->at(pos);
}

bool Section::isInSection(ivec3 globalPos) const {
	return 0 <= globalPos.x && globalPos.x < Const::SECTION_SIDE && 
			0 <= globalPos.y && globalPos.y < Const::SECTION_HEIGHT && 
			0 <= globalPos.z && globalPos.z < Const::SECTION_SIDE;
}