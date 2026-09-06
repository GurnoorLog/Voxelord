#pragma once

#include <glad/glad.h>
#include <maths/GlmCommon.h>

#include <array>
#include <vector>
#include <optional>
#include <mutex>
#include <memory>
#include <utility>

#include "util/Array3D.h"
#include "maths/Dir2D.h"
#include "maths/Dir3D.h"
#include "block/Block.h"
#include "Mesh.h"
#include "renderer/Renderer.h"
#include "WorldConstants.h"

class Chunk;

// 3x3 block of chunks around a section's own chunk, snapshotted so sections never read the chunk map.
// Indexed by horizontal offset in [-1, 1] per axis, center holds the own chunk; the diagonals feed AO/smooth light.
struct NeighborChunks {
	std::array<const Chunk*, 9> chunks{};
	static int index(int dx, int dz) { return (dx + 1) + (dz + 1) * 3; }
	const Chunk* at(int dx, int dz) const { return chunks[index(dx, dz)]; }
	const Chunk*& at(int dx, int dz) { return chunks[index(dx, dz)]; }
};

class Section {
public:
	Section(const Chunk* chunk = nullptr, ivec3 position = ivec3{ 0, 0, 0 });
	// Lives in place in Chunk's section map and owns a mutex, so neither copy nor move is valid.
	Section(const Section& other) = delete;
	Section& operator=(const Section& other) = delete;

	void loadMesh(const NeighborChunks& neighbors);
	void uploadMesh();
	void releaseMesh();
	void render(const Renderer& renderer) const;
	// Each renderer grabs the active mesh it owns.
	const DefaultMesh& getDefaultMesh() const;
	const WaterMesh& getWaterMesh() const;
	const LavaMesh& getLavaMesh() const;

	ivec3 getPosition() const;
	void setBlock(ivec3 pos, Block block);
	Block getBlock(ivec3 pos) const;
	Block& getBlock(ivec3 pos);

private:
	using BlockArray = Array3D<Block, Const::SECTION_SIDE, Const::SECTION_HEIGHT, Const::SECTION_SIDE>;
	const Chunk* p_chunk{ nullptr };
	ivec3 m_position;

	std::unique_ptr<BlockArray> m_blocks;
	DefaultMesh activeDefaultMesh;
	DefaultMesh nextDefaultMesh;
	WaterMesh activeWaterMesh;
	WaterMesh nextWaterMesh;
	LavaMesh activeLavaMesh;
	LavaMesh nextLavaMesh;
	// Filled on a worker thread by loadMesh(); uploaded to the GPU on the main thread by uploadMesh().
	std::vector<DefaultMesh::Vertex> m_nextDefaultVertices;
	std::vector<WaterMesh::Vertex> m_nextWaterVertices;
	std::vector<LavaMesh::Vertex> m_nextLavaVertices;
	// True while the vectors above hold a fresh build awaiting upload. uploadMesh() clears them, so
	// without this flag a second upload with no rebuild would push an empty mesh. Guarded by m_meshMutex.
	bool m_meshReady = false;
	// Serializes builds/uploads of the "next" buffers above.
	mutable std::mutex m_meshMutex;

	bool isInSection(ivec3 globalPos) const;
	const Section* findNeighboringSection(Dir3D::Dir dir, const NeighborChunks& neighbors) const;
	std::tuple<std::vector<DefaultMesh::Vertex>, std::vector<WaterMesh::Vertex>, std::vector<LavaMesh::Vertex>> findVisibleFaces(const NeighborChunks& neighbors) const;
	// Appends the crossed billboard quads of every plant block in the section.
	void addPlantFaces(std::vector<DefaultMesh::Vertex>& defaultVertices) const;
	void addVisibleFacesOnLastAxis(
		std::vector<DefaultMesh::Vertex>& defaultVertices, std::vector<WaterMesh::Vertex>& waterVertices,
		std::vector<LavaMesh::Vertex>& lavaVertices,
		Dir3D::Dir dir, ivec3 localPos, int indexOfLastAxis, int sizeOfLastAxis,
		const Section* neighboringSection, const NeighborChunks& neighbors
	) const;
	void addFace(std::vector<DefaultMesh::Vertex>& defaultVertices, std::vector<WaterMesh::Vertex>& waterVertices,
		std::vector<LavaMesh::Vertex>& lavaVertices,
		Dir3D::Dir dir, ivec3 localPos, int length, Block block, int indexOfLastAxis,
		const std::array<GLfloat, 4>& ao, const std::array<vec2, 4>& light) const;
	void addDefaultFace(std::vector<DefaultMesh::Vertex>& defaultVertices, Dir3D::Dir dir, Block block,
		int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos, const std::array<GLfloat, 4>& ao,
		const std::array<vec2, 4>& light) const;
	// Per-corner light for a unit-block face, computed during meshing: AO brightness darkened by the
	// solid cells diagonally around the corner, plus (sky, block) smooth light averaging the open cells.
	std::pair<std::array<GLfloat, 4>, std::array<vec2, 4>> computeFaceLighting(
		const NeighborChunks& neighbors, Dir3D::Dir dir, ivec3 localPos) const;
	// Block in this section's local coords, resolving into neighboring sections.
	Block getNeigboringBlock(const NeighborChunks& neighbors, ivec3 localPos) const;
	void addWaterFace(std::vector<WaterMesh::Vertex>& waterVertices, Dir3D::Dir dir,
		int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos) const;
	// Lava is opaque, so it gets one back-face-culled face like a default block, but position+normal
	// only: the molten look is fully procedural in the shader.
	void addLavaFace(std::vector<LavaMesh::Vertex>& lavaVertices, Dir3D::Dir dir,
		int indexOfLastAxis, int length, ivec3 firstBlockGlobalPos) const;
};