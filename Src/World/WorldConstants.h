#pragma once

namespace Const {
	const int SECTION_SIDE_POWER = 5;
	const int SECTION_HEIGHT_POWER = 5;
	const int SECTION_SIDE = 1 << SECTION_SIDE_POWER;
	const int SECTION_HEIGHT = 1 << SECTION_HEIGHT_POWER;
	const int INIT_CHUNK_NB_SECTIONS = 4;
	const int INIT_CHUNK_HEIGHT = INIT_CHUNK_NB_SECTIONS * SECTION_HEIGHT;

	const int SEA_LEVEL = 64;
	const int LAVA_MIN = 6;
	const int LAVA_MAX = 22;

	// Default spawn height when no terrain has been scanned yet (client and server agree).
	const int SPAWN_Y = 80;
	// A full daylight cycle in real seconds (server advances its clock with the same value as the
	// client so both reach the same time of day).
	const float DAY_LENGTH_SECONDS = 120.f;
}