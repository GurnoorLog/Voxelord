#pragma once

#include <functional>

#include <maths/GlmCommon.h>
#include <maths/SweptAABB.h>
#include <block/Block.h>
#include <physics/IWorldView.h>



// Shared physics capsule, used by both the client and the server so they move identically.
class PlayerController {
public:
	enum Direction {
		FORWARD,
		BACKWARD,
		RIGHT,
		LEFT,
		UP,
		DOWN
	};

	static const float WALK_HORIZONTAL_SPEED;
	static const float FLY_HORIZONTAL_SPEED, FLY_VERTICAL_SPEED;
	static const float WALK_SPRINT_MULTIPLIER, FLY_SPRINT_MULTIPLIER;
	static const float PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_HEAD_HEIGHT;

	static const float JUMP_SPEED;
	static const float GRAVITY;
	static const float PUSH_OUT_SPEED;

	static const vec3 WORLDUP;

	PlayerController();

	void setWorldView(const IWorldView* worldView);

	void setPosition(vec3 position);
	vec3 getPosition() const;
	// Look direction in degrees; used to derive the horizontal move axes, exactly like the camera.
	void setYaw(float yaw);
	void setPitch(float pitch);
	void setYawPitch(float yaw, float pitch);
	float getYaw() const;
	float getPitch() const;
	void setFlying(bool flying);
	bool getFlying() const;

	void setSprinting(bool sprinting);
	void move(Direction direction, float deltaTime);
	void update(float deltaTime);
	vec3 getMoveAndReset(float deltaTime);
	bool isInWater() const;
	bool isOnGround() const;
	bool intersectsBlock(ivec3 blockPos) const;
	// The velocity produced by the last getVelocityAndReset() call (units per second).
	vec3 getVelocity() const;
	vec3 getHorizontalDir() const;
	vec3 getVerticalDir() const;

private:
	enum class Fluid { NONE, WATER, LAVA };
	struct FluidPhysics {
		float jumpSpeed;
		float swimUpAcceleration;
		float gravity;
		float horizontalMultiplier;
		float verticalDrag; // fraction of vertical speed kept per second
	};
	// Lava is thicker than water: you barely climb out, sink slowly and move sluggishly.
	static constexpr FluidPhysics WATER_PHYSICS{ 7.f, 20.f, 10.f, 0.6f, 0.07f };
	static constexpr FluidPhysics LAVA_PHYSICS{ 4.f, 12.f, 6.f, 0.35f, 0.04f };
	static constexpr FluidPhysics fluidPhysics(Fluid fluid) {
		return fluid == Fluid::LAVA ? LAVA_PHYSICS : WATER_PHYSICS;
	}

	const IWorldView* m_worldView{ nullptr };

	vec3 m_position{ 0.f, 80.f, 0.f };
	float m_yaw{ -90.f };
	float m_pitch{ 0.f };
	bool m_flying = true;

	float m_verticalSpeed{ 0.f };
	vec3 m_horizontalDir{};
	vec3 m_verticalDir{};
	bool m_onTheGround = false;
	Fluid m_fluid = Fluid::NONE;
	bool m_sprinting = false;
	vec3 m_lastVelocity{};

	vec3 getFront() const;
	vec3 getRight() const;
	Fluid currentFluid() const;
	vec3 getVelocityAndReset();
	vec3 getMoveWithCollisionsAndReset(float deltaTime);
	vec3 getUnstuckShift(float deltaTime) const;
	std::vector<ivec3> getBroadphaseBlocks(const Box& hitbox, vec3 shift,
		std::function<bool(Block)> filter) const;
	Box makeHitbox() const;
};