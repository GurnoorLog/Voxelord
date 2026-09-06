#pragma once

#include <optional>
#include <vector>

#include <SFML/Window.hpp>

#include <View/Camera.h>
#include <Physics/PlayerController.h>
#include <World/ChunkWorldView.h>
#include <Block/Block.h>
#include <Block/BlockID.h>
#include <Commands/Commands.h>

class Game;

class Player {
public:
	Player(Game* game);

	void processMouseClick(sf::Time dt, Commands& commands);
	void processMouseMove(sf::Time dt);
	void processMouseWheel(sf::Time dt, GLfloat delta);
	void update(sf::Time dt);
	void teleport();
	void placeBlockBelow();
	void move(PlayerController::Direction direction, sf::Time dt);
	void setSprinting(bool sprinting);
	// Places block at the aimed face; false when nothing is targetable or it's blocked.
	bool placeBlock(Block block);
	vec3 getPosition() const;
	vec3 getVelocity() const;
	bool isOnGround() const;
	void toggleFlying();
	void setFlying(bool flying);
	bool isFlying() const;

	Game& getGame();
	const Game& getGame() const;
	Camera& getCamera();
	const Camera& getCamera() const;
	std::optional<ivec3> getTarget() const;
	std::optional<Block> getPickedBlock() const;
	// The air block just past the aimed face, where a place action would land.
	std::optional<ivec3> getPlacePos() const;
	void cycleBlock(int dir);
	void selectBlock(int slot);
	int getHotbarIndex() const;
	const std::vector<BlockID>& hotbar() const;
	bool isInWater() const;
	bool intersectsBlock(ivec3 blockPos) const;

private:
	static const vec3 INITIAL_POSITION;
	static const float DEFAULT_TARGET_DISTANCE;
	static const sf::Time REPEAT_DELAY;

	bool tryRepeat(sf::Time& accumulator, sf::Time dt, bool active);
	void refreshController();

	Game* game;
	bool m_flying = true;
	Camera m_camera;
	ChunkWorldView m_chunkWorldView;
	PlayerController m_controller;
	std::optional<ivec3> targetPos;
	std::optional<ivec3> placePos;
	std::optional<Block> pickedBlock{ BlockID::WATER };
	// Mirrored to pickedBlock so the HUD badge shows the same selection.
	int m_hotbarIndex = 0;

	sf::Time breakAccumulator = sf::seconds(0.f);
	sf::Time placeAccumulator = sf::seconds(0.f);
};

