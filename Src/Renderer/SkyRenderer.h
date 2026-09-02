#pragma once

#include <glad/glad.h>
#include "Maths/GlmCommon.h"
#include "ResManager/Shader.h"
#include "World/Mesh.h"

class Camera;

struct SkyVertex {
	vec3 pos;
	vec2 uv;
};

struct SkyMesh : Mesh<SkyVertex, SkyMesh> {
	void loadAttributes() {
		addFloatVertexAttribPointer(0, 3, 0);
		addFloatVertexAttribPointer(1, 2, sizeof(vec3));
	}
};

// Renders the day/night sky dome (a unit sphere centered on the camera) sampled from the Craft sky
// strip, so it can be drawn before the terrain without a depth clear. Must be loaded after GL is up.
class SkyRenderer {
public:
	SkyRenderer() = default;

	void load();

	// Draws the dome with depth test and depth write off, so terrain overdraws it.
	void render(const Camera& camera, float timer);

private:
	Shader m_shader;
	GLuint m_texture = 0;
	SkyMesh m_mesh;
};