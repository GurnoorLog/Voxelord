#include "physics/PlayerController.h"

#include <cmath>

#include "maths/AABB.h"

const float PlayerController::WALK_HORIZONTAL_SPEED{ 5.f };
const float PlayerController::FLY_HORIZONTAL_SPEED{ 15.f };
const float PlayerController::FLY_VERTICAL_SPEED{ 15.f };
const float PlayerController::WALK_SPRINT_MULTIPLIER{ 1.85f };
const float PlayerController::FLY_SPRINT_MULTIPLIER{ 20.f };
const float PlayerController::PLAYER_WIDTH{ 0.6f };
const float PlayerController::PLAYER_HEIGHT{ 1.8f };
const float PlayerController::PLAYER_HEAD_HEIGHT{ 1.65f };

const float PlayerController::JUMP_SPEED{ 10.f };
const float PlayerController::GRAVITY{ 32.f };
const float PlayerController::PUSH_OUT_SPEED{ 8.f }; // Speed the player is ejected when stuck in a block

const vec3 PlayerController::WORLDUP{ vec3{ 0.f, 1.f, 0.f } };

PlayerController::PlayerController() {}

void PlayerController::setWorldView(const IWorldView* worldView) {
	m_worldView = worldView;
}

void PlayerController::setPosition(vec3 position) {
	m_position = position;
}

vec3 PlayerController::getPosition() const {
	return m_position;
}

void PlayerController::setYaw(float yaw) {
	m_yaw = yaw;
}

void PlayerController::setPitch(float pitch) {
	m_pitch = pitch;
}

void PlayerController::setYawPitch(float yaw, float pitch) {
	m_yaw = yaw;
	m_pitch = pitch;
}

float PlayerController::getYaw() const {
	return m_yaw;
}

float PlayerController::getPitch() const {
	return m_pitch;
}

void PlayerController::setFlying(bool flying) {
	m_flying = flying;
}

void PlayerController::setSprinting(bool sprinting) {
	m_sprinting = sprinting;
}

vec3 toHorizontal(vec3 vec) {
	return normalize(vec3{ vec.x, 0.f, vec.z });
}

vec3 PlayerController::getFront() const {
	// Same math as Camera so prediction uses the same move axes.
	vec3 front;
	front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	front.y = sin(glm::radians(m_pitch));
	front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	return normalize(front);
}

vec3 PlayerController::getRight() const {
	return normalize(cross(getFront(), WORLDUP));
}

void PlayerController::move(Direction direction, float deltaTime) {
	vec3 front = getFront();
	vec3 right = getRight();
	switch (direction) {
	case FORWARD:
		m_horizontalDir += toHorizontal(front);
		break;
	case BACKWARD:
		m_horizontalDir -= toHorizontal(front);
		break;
	case RIGHT:
		m_horizontalDir += toHorizontal(right);
		break;
	case LEFT:
		m_horizontalDir -= toHorizontal(right);
		break;
	case UP:
		if (!m_flying) {
			if (m_onTheGround) {
				if (m_fluid != Fluid::NONE)
					m_verticalSpeed = fluidPhysics(m_fluid).jumpSpeed;
				else
					m_verticalSpeed = JUMP_SPEED;
			} else {
				if (m_fluid != Fluid::NONE)
					m_verticalSpeed += fluidPhysics(m_fluid).swimUpAcceleration * deltaTime;
			}
		} else {
			m_verticalDir += WORLDUP;
		}
		break;
	case DOWN:
		m_verticalDir -= WORLDUP;
		break;
	}
}

void PlayerController::update(float deltaTime) {
	if (!m_flying) {
		if (m_fluid != Fluid::NONE) {
			FluidPhysics phys = fluidPhysics(m_fluid);
			m_verticalSpeed -= phys.gravity * deltaTime;
			m_verticalSpeed *= std::pow(phys.verticalDrag, deltaTime);
		} else {
			m_verticalSpeed -= GRAVITY * deltaTime;
		}
	}
}

vec3 PlayerController::getVelocityAndReset() {
	float horizontal_speed = m_flying ? FLY_HORIZONTAL_SPEED : WALK_HORIZONTAL_SPEED;
	float sprint_multiplier = m_flying ? FLY_SPRINT_MULTIPLIER : WALK_SPRINT_MULTIPLIER;
	vec3 horizontal_move{};
	if (m_horizontalDir != vec3())
		horizontal_move = normalize(m_horizontalDir) * horizontal_speed;
	if (m_sprinting)
		horizontal_move *= sprint_multiplier;
	if (!m_flying && m_fluid != Fluid::NONE)
		horizontal_move *= fluidPhysics(m_fluid).horizontalMultiplier;
	vec3 vertical_move;
	if (!m_flying) {
		vertical_move = WORLDUP * m_verticalSpeed;
	} else {
		m_verticalSpeed = 0;
		vertical_move = m_verticalDir * FLY_VERTICAL_SPEED;
		if (m_sprinting)
			vertical_move *= sprint_multiplier;
	}
	m_horizontalDir = m_verticalDir = vec3();
	m_sprinting = false;
	vec3 velocity = horizontal_move + vertical_move;
	m_lastVelocity = velocity;
	return velocity;
}

vec3 PlayerController::getMoveAndReset(float deltaTime) {
	return getMoveWithCollisionsAndReset(deltaTime);
}

Box PlayerController::makeHitbox() const {
	vec3 hitbox_size{ PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH };
	vec3 hitbox_pos = m_position;
	hitbox_pos.x -= hitbox_size.x / 2;
	hitbox_pos.z -= hitbox_size.z / 2;
	hitbox_pos.y -= PLAYER_HEAD_HEIGHT;
	return { hitbox_pos, hitbox_size };
}

vec3 PlayerController::getMoveWithCollisionsAndReset(float deltaTime) {
	vec3 velocity = getVelocityAndReset();

	Box hitbox = makeHitbox();
	std::vector<ivec3> blocks = getBroadphaseBlocks(hitbox, velocity * deltaTime, [this](Block block) {
		return m_worldView->blockData(block.id).isObstacle();
	});
	std::vector<Box> bs;
	for (ivec3 block : blocks) {
		bs.push_back({ block, {1, 1, 1} });
	}
	vec3 shift = repeatedSweptAABB(hitbox, velocity, bs, deltaTime);

	if (!m_flying) {
		// Compare against requested displacement, not |shift.y| ~ 0, which is more robust.
		bool verticalBlocked = velocity.y != 0.f && std::abs(shift.y - velocity.y * deltaTime) > 1e-6f;
		// Grounded only when blocked while moving down.
		m_onTheGround = velocity.y < 0.f && verticalBlocked;
		if (verticalBlocked)
			m_verticalSpeed = 0.f;
	}
	m_fluid = currentFluid();

	// Push out of any block the player ends up embedded in.
	shift += getUnstuckShift(deltaTime);

	return shift;
}

vec3 PlayerController::getUnstuckShift(float deltaTime) const {
	Box hitbox = makeHitbox();
	std::vector<ivec3> blocks = getBroadphaseBlocks(hitbox, vec3(), [this](Block block) {
		return m_worldView->blockData(block.id).isObstacle();
	});

	// Only treat a block as penetrating past this depth, so merely touching a floor or wall
	// (the usual case) never produces a push
	const float epsilon = 1e-4f;

	// Push out of each block along its shallowest axis so a corner escapes diagonally
	// instead of oscillating between two perpendicular walls.
	vec3 push{};
	for (ivec3 bp : blocks) {
		vec3 overlap, dir;
		bool penetrating = true;
		for (int i = 0; i < 3; ++i) {
			float hmin = hitbox.pos[i], hmax = hmin + hitbox.size[i];
			float bmin = float(bp[i]), bmax = bmin + 1.f;
			float penPos = bmax - hmin; // depth to exit towards +i
			float penNeg = hmax - bmin; // depth to exit towards -i
			overlap[i] = std::min(penPos, penNeg);
			dir[i] = penPos < penNeg ? 1.f : -1.f;
			if (overlap[i] <= epsilon)
				penetrating = false;
		}
		if (!penetrating)
			continue;

		int axis = 0;
		for (int i = 1; i < 3; ++i)
			if (overlap[i] < overlap[axis])
				axis = i;

		// Blocks sharing an axis don't stack: keep the deepest push needed on each axis
		float p = dir[axis] * overlap[axis];
		if (std::abs(p) > std::abs(push[axis]))
			push[axis] = p;
	}

	float dist = length(push);
	if (dist <= epsilon)
		return vec3();

	// Eject smoothly, capped per frame so we get pushed out rather than teleported
	return push * (std::min(dist, PUSH_OUT_SPEED * deltaTime) / dist);
}

PlayerController::Fluid PlayerController::currentFluid() const {
	auto overlaps = [this](BlockData::Category category) {
		return !getBroadphaseBlocks(makeHitbox(), vec3(), [this, category](Block block) {
			return m_worldView->blockData(block.id).getCategory() == category;
		}).empty();
	};
	if (overlaps(BlockData::Category::LAVA))
		return Fluid::LAVA;
	if (overlaps(BlockData::Category::WATER))
		return Fluid::WATER;
	return Fluid::NONE;
}

bool PlayerController::isOnGround() const {
	return m_onTheGround;
}

bool PlayerController::intersectsBlock(ivec3 blockPos) const {
	return aabb_check(makeHitbox(), { blockPos, {1, 1, 1} });
}

std::vector<ivec3> PlayerController::getBroadphaseBlocks(const Box& hitbox, vec3 shift,
		std::function<bool(Block)> filter) const {

	std::vector<ivec3> blocks;
	Box b; ivec3 dirs;
	std::tie(b, dirs) = getBroadphaseBox(hitbox, shift);
	ivec3 pos;
	const IWorldView& worldView = *m_worldView;

	std::function<void(int)> rec = [&rec, &blocks, &b, &dirs, &pos, &worldView, &filter](int axis) {
		if (axis == 3) {
			Block block = worldView.getBlock(pos);
			if (filter(block))
				blocks.push_back(pos);
			return;
		}
		// Start on the side at the opposite of the hitbox
		float left = b.pos[axis], right = b.pos[axis] + b.size[axis];
		int& coord = pos[axis];
		if (dirs[axis] == 1) {
			for (coord = int(floor(right)); coord > left - 1; --coord) {
				rec(axis + 1);
			}
		} else {
			for (coord = int(floor(left)); coord < right; ++coord) {
				rec(axis + 1);
			}
		}
	};
	rec(0);
	return blocks;
}