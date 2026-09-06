#pragma once

#include <glad/glad.h>

#include <vector>

#include "maths/GlmCommon.h"
#include "resources/Shader.h"

// A remote player's render state: eye position (same convention as Camera) plus yaw, and a
// body color derived from the player id so teammates are easy to tell apart.
struct RemotePlayerVisual {
	vec3 eyePosition;
	float yaw = 0.f;
	vec3 color{ 1.f };
};

// Draws the other connected players as simple two-cube figures (body + head) that move with
// the positions broadcast by the server. Rendered in the terrain pass so they are occluded by
// the world and visible through water, like any other block geometry.
class RemotePlayerDrawer {
public:
	RemotePlayerDrawer();
	~RemotePlayerDrawer();

	RemotePlayerDrawer(const RemotePlayerDrawer&) = delete;
	RemotePlayerDrawer& operator=(const RemotePlayerDrawer&) = delete;

	void render(const std::vector<RemotePlayerVisual>& players, const mat4& view, const mat4& projection);

private:
	void buildMesh();
	void drawCube(const mat4& model, const vec3& color);

	Shader m_shader;
	GLuint m_VAO = 0, m_VBO = 0, m_EBO = 0;
};