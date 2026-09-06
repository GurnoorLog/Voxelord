#include "Player.h"

#include "Maths/Converter.h"

#include <Game/Game.h>
#include <Maths/LineBlockFinder.h>
#include <World/ChunkWorldView.h>

const vec3 Player::INITIAL_POSITION{ vec3{ 0.f, 80.f, 0.f } };
const float Player::DEFAULT_TARGET_DISTANCE{ static_cast<float>(ChunkMap::VIEW_DISTANCE * Const::SECTION_SIDE) };
const sf::Time Player::REPEAT_DELAY{ sf::seconds(0.2f) };

namespace {
	// Grass first so a natural block is held at spawn.
	const std::vector<BlockID>& g_hotbar() {
		static const std::vector<BlockID> slots{
			BlockID::GRASS, BlockID::STONE, BlockID::DIRT, BlockID::SAND, BlockID::LOG,
			BlockID::LEAVES, BlockID::SNOW, BlockID::ICE, BlockID::WATER, BlockID::LAVA
		};
		return slots;
	}
}

Player::Player(Game* game)
	: game{ game }, m_flying{ true }, m_camera{ INITIAL_POSITION }, m_controller{} {
	pickedBlock = g_hotbar().front();
}

void Player::refreshController() {
	m_chunkWorldView.setChunkMap(&game->getChunkMap());
	m_controller.setWorldView(&m_chunkWorldView);
	m_controller.setPosition(m_camera.getPosition());
	m_controller.setYawPitch(m_camera.getYaw(), m_camera.getPitch());
	m_controller.setFlying(m_flying);
}

void Player::processMouseClick(sf::Time dt, Commands& commands) {
	refreshController();

	if (commands.isActive(Command::PICK) && targetPos.has_value())
		pickedBlock = game->getChunkMap().getBlock(targetPos.value());

	bool breaking = commands.isActive(Command::BREAK) && targetPos.has_value();
	if (tryRepeat(breakAccumulator, dt, breaking))
		game->setBlockNetwork(targetPos.value(), +BlockID::AIR);

	bool placing = commands.isActive(Command::PLACE) && placePos.has_value() &&
		pickedBlock.has_value() && !intersectsBlock(placePos.value());
	if (tryRepeat(placeAccumulator, dt, placing))
		game->setBlockNetwork(placePos.value(), pickedBlock.value());
}

// Fires right away on the first active frame, then once per REPEAT_DELAY while held.
bool Player::tryRepeat(sf::Time& accumulator, sf::Time dt, bool active) {
	if (!active) {
		// Stay primed so the next click acts immediately.
		accumulator = REPEAT_DELAY;
		return false;
	}
	accumulator += dt;
	if (accumulator < REPEAT_DELAY)
		return false;
	accumulator = sf::seconds(0.f);
	return true;
}

void Player::processMouseMove(sf::Time) {
	sf::Vector2i mousePosTemp{ sf::Mouse::getPosition(game->getWindow()) };
	ivec2 mousePosition{ mousePosTemp.x, mousePosTemp.y };
	ivec2 windowCenter{ game->getWindow().getCenter() };
	ivec2 mouseOffset{ mousePosition - windowCenter };
	m_camera.processMouse(mouseOffset);
}

void Player::processMouseWheel(sf::Time, GLfloat delta) {
	m_camera.processMouseScroll(delta);
}

void Player::update(sf::Time dt) {
	refreshController();
	m_controller.update(dt.asSeconds());
	m_camera.move(m_controller.getMoveAndReset(dt.asSeconds()));
	m_camera.update({ game->getWindow().size() });

	LineBlockFinder lineBlockFinder{ m_camera.getPosition(), m_camera.getFront() };
	placePos = std::nullopt;
	targetPos = std::nullopt;
	float targetDistance = DEFAULT_TARGET_DISTANCE;
	while (lineBlockFinder.getDistance() <= targetDistance) {
		ivec3 iterPos = lineBlockFinder.next();
		Block block = game->getChunkMap().getBlock(iterPos);
		if (ResManager::blockDatas().get(block.id).isObstacle()) {
			targetPos = iterPos;
			break;
		}
		placePos = iterPos;
	}
	if (targetPos == std::nullopt)
		placePos = std::nullopt;
}

void Player::teleport() {
	if (targetPos.has_value()) {
		vec3 pos = targetPos.value();
		pos += vec3(0.5, 1 + PlayerController::PLAYER_HEAD_HEIGHT, 0.5);
		m_camera.setPosition(pos);
		m_controller.setPosition(pos);
	}
}

void Player::placeBlockBelow() {
	if (pickedBlock.has_value()) {
		vec3 pos = getPosition();
		pos.y -= 1.f + PlayerController::PLAYER_HEAD_HEIGHT;
		game->setBlockNetwork(Converter::globalPosToBlock(pos), pickedBlock.value());
	}
}

void Player::move(PlayerController::Direction direction, sf::Time dt) {
	refreshController();
	m_controller.move(direction, dt.asSeconds());
}

void Player::setSprinting(bool sprinting) {
	m_controller.setSprinting(sprinting);
}

bool Player::placeBlock(Block block) {
	if (!placePos.has_value())
		return false;
	if (intersectsBlock(placePos.value()))
		return false;
	game->setBlockNetwork(placePos.value(), block);
	return true;
}

vec3 Player::getPosition() const {
	return m_camera.getPosition();
}

vec3 Player::getVelocity() const {
	return m_controller.getVelocity();
}

bool Player::isOnGround() const {
	return m_controller.isOnGround();
}

void Player::toggleFlying() {
	m_flying = !m_flying;
	m_controller.setFlying(m_flying);
}

void Player::setFlying(bool flying) {
	if (m_flying == flying)
		return;
	m_flying = flying;
	m_controller.setFlying(m_flying);
}

bool Player::isFlying() const {
	return m_flying;
}

Game& Player::getGame() {
	return *game;
}

const Game& Player::getGame() const {
	return *game;
}

Camera& Player::getCamera() {
	return m_camera;
}

const Camera& Player::getCamera() const {
	return m_camera;
}

std::optional<ivec3> Player::getTarget() const {
	return targetPos;
}

std::optional<Block> Player::getPickedBlock() const {
	return pickedBlock;
}

void Player::cycleBlock(int dir) {
	selectBlock(m_hotbarIndex + dir);
}

void Player::selectBlock(int slot) {
	const auto& slots = g_hotbar();
	m_hotbarIndex = (slot % static_cast<int>(slots.size()) + static_cast<int>(slots.size())) % static_cast<int>(slots.size());
	pickedBlock = slots[static_cast<size_t>(m_hotbarIndex)];
}

int Player::getHotbarIndex() const {
	return m_hotbarIndex;
}

const std::vector<BlockID>& Player::hotbar() const {
	return g_hotbar();
}

std::optional<ivec3> Player::getPlacePos() const {
	return placePos;
}

bool Player::isInWater() const {
	return m_controller.isInWater();
}

bool Player::intersectsBlock(ivec3 blockPos) const {
	return m_controller.intersectsBlock(blockPos);
}
