#pragma once

#include <vector>

#include <glad/glad.h>

#include "Maths/GlmCommon.h"
#include "ResManager/Shader.h"

// Draws animated explosion fireballs. Each is a stack of nested sphere shells, so it has real
// volume and hides correctly behind terrain instead of being a flat sprite. Spawned the moment an
// explosion happens and fades out on its own. All on the render thread, so no locking needed.
class ExplosionDrawer {
public:
	ExplosionDrawer();
	~ExplosionDrawer();

	ExplosionDrawer(const ExplosionDrawer&) = delete;
	ExplosionDrawer& operator=(const ExplosionDrawer&) = delete;

	// radius is the blast's size in blocks.
	void spawn(vec3 center, float radius);
	// Step each effect forward and remove the finished ones.
	void update(float dt);
	// Draw every active effect. clipPlane cuts the fireball at a plane (used at sea level in the
	// water reflection pass); skyColor is the fog it fades into in the distance.
	void render(const mat4& view, const mat4& projection, const vec4& clipPlane, const vec3& skyColor);

private:
	struct Explosion {
		vec3 center;
		float size;
		float seed;
		float age;
		float duration;
		float density;
		float bigness;
	};

	// More shells look smoother but cost more to draw.
	static constexpr int SHELL_COUNT = 100;

	// Build the unit sphere every shell is a scaled copy of.
	void buildMesh();

	Shader m_shader;
	GLuint m_VAO = 0, m_VBO = 0, m_EBO = 0;
	GLsizei m_indexCount = 0;
	std::vector<Explosion> m_explosions;
};
