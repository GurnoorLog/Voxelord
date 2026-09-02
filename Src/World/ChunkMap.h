#pragma once

#include "World/Chunk.h"
#include "World/WorldConstants.h"
#include "World/BlockEdit.h"
#include "View/Frustum.h"
#include "Maths/GlmCommon.h"

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <utility>
#include <mutex>
#include <atomic>
#include <functional>

// Main thread renders and applies single-block edits. One orchestrator keeps m_toSelect fresh and
// unloads far chunks. A pool of workers pulls from the work pools and generate/mesh/bulk-edit.
// m_chunksMutex guards m_chunks and every work pool. Mesh builds read from a snapshot taken under
// the lock; a chunk is claimed for loading via atomic CAS so two workers never load the same one.
// Touched chunks are kept alive with shared_ptr across unlocked work so the orchestrator can erase
// them without freeing them under a worker.
class ChunkMap {
public:
	static const int VIEW_DISTANCE, SIDE, LOADING_WORKERS_COUNT;

	// Produces block changes lazily on a worker thread so the heavy work never stutters the frame.
	using EditGenerator = std::function<std::vector<BlockEdit>()>;

	ChunkMap(ivec2 center = ivec2{ 0, 0 });
	~ChunkMap();

	// ---- Main (game-loop) thread ----
	void update();
	void render(const Frustum& frustum, const std::vector<const Renderer*>& renderers);
	void setBlock(ivec3 globalPos, Block block);
	void applyEdits(const std::vector<BlockEdit>& edits);
	// Returns true if the edit was queued, false if it was dropped because one is already in flight.
	bool submitBulkEdit(EditGenerator generator);
	Block getBlock(ivec3 globalPos) const;
	void setCenter(ivec2 center);
	ivec2 getCenter();
	void stop();

	int size();
	int chunksAtLeastInState(Chunk::State state);
	int chunksInState(Chunk::State state);
	int getRenderedChunks();

	// ---- Worker threads ----
	void refreshSelection(const Frustum& frustum); // orchestrator: rebuild the sorted view list
	void processNextTask();                         // worker: take and run one task
	void unloadFarChunks();                         // orchestrator

	// No internal locking. The const overloads are the loading thread's lock-free neighbor
	// reads (safe because only the loading thread changes the map structure); the non-const
	// overloads are used by the main thread while it already holds m_chunksMutex.
	Chunk& getChunk(ivec2 pos);
	const Chunk& getChunk(ivec2 pos) const;
	Section& getSection(ivec3 pos);
	const Section& getSection(ivec3 pos) const;
	void onChangeChunkState(Chunk::State previous, Chunk::State next);

private:
	std::unordered_map<ivec2, std::shared_ptr<Chunk>, Comp_ivec2, Comp_ivec2> m_chunks;
	std::queue<ivec2> m_chunksToUploadMesh;
	// A post-edit section rebuild skips its own upload for these, since the pending full upload
	// already carries it.
	std::unordered_set<ivec2, Comp_ivec2, Comp_ivec2> m_chunksPendingUpload;
	std::queue<ivec3> m_sectionsToReUploadMesh;

	// Workers pull from the first non-empty pool in this order: load blocks, load mesh, select.
	// The m_in* sets keep the queues duplicate-free.
	std::queue<ivec2> m_toLoadBlocks;
	std::unordered_set<ivec2, Comp_ivec2, Comp_ivec2> m_inLoadBlocks;
	std::queue<ivec2> m_toLoadMeshes;
	std::unordered_set<ivec2, Comp_ivec2, Comp_ivec2> m_inLoadMeshes;
	std::vector<ivec2> m_toSelect;
	size_t m_selectCursor = 0;

	// At most one bulk edit in flight at a time.
	std::queue<EditGenerator> m_bulkEdits;
	bool m_bulkEditRunning = false;

	ivec2 m_center;
	mutable std::mutex m_chunksMutex; // guards m_chunks, the work pools and the upload queues
	std::mutex m_lightMutex; // guards light spill across chunks; never held alongside m_chunksMutex
	bool m_mustStop = false;

	int m_renderedChunks = 0; // main thread only
	// Atomic because onChangeChunkState() is called from several threads, some unlocked.
	std::array<std::atomic<int>, Chunk::STATE_SIZE> m_countChunks{};

	// Iterator to the chunk owning globalPos if it has its blocks loaded, else end().
	// Caller must already hold m_chunksMutex.
	std::unordered_map<ivec2, std::shared_ptr<Chunk>, Comp_ivec2, Comp_ivec2>::const_iterator
		findLoadedChunk(ivec3 globalPos) const;
	bool isInLoadDistance(ivec2 pos) const;
	// Worker task handlers
	void doSelectChunk(ivec2 pos);
	void doLoadBlocks(ivec2 pos);
	void doLoadMesh(ivec2 pos);
	void doBulkEdit(EditGenerator generator);
	void remeshSections(const std::vector<ivec3>& sections);
	bool canChunkBeEdited(ivec2 pos) const;
	// Caller must already hold m_chunksMutex
	void enqueueBlocksIfNeeded(ivec2 pos);
	void enqueueMeshIfReady(ivec2 pos);
	// No lock needed
	bool isChunkInFrustum(Chunk* chunk, const Frustum& frustum);
	bool isChunkInFrustumApprox(ivec2 chunkPos, const Frustum& frustum) const;
};
