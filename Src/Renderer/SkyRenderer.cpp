#include "SkyRenderer.h"

#include "View/Camera.h"
#include "Util/Logger.h"

#include "stb_image/stb_image.h"

#include <array>
#include <cmath>
#include <vector>

namespace {
	constexpr float PI = 3.14159265358979f;

	// Recursive octahedron subdivision, a port of Craft's make_sphere. Each level splits a triangle
	// into four and pushes the midpoints onto the unit sphere. detail = 3 yields 512 triangles.
	void makeSkySphere(std::vector<SkyVertex>& vertices, float r, int detail,
			const vec3& a, const vec3& b, const vec3& c,
			const vec2& ta, const vec2& tb, const vec2& tc) {
		if (detail == 0) {
			vertices.push_back({ a * r, ta });
			vertices.push_back({ b * r, tb });
			vertices.push_back({ c * r, tc });
			return;
		}
		const vec3 ab = glm::normalize((a + b) * 0.5f);
		const vec3 ac = glm::normalize((a + c) * 0.5f);
		const vec3 bc = glm::normalize((b + c) * 0.5f);
		// v = 1 at the zenith (y = 1) down to 0 at the nadir, matching the sky strip's top-to-bottom.
		auto vCoord = [](const vec3& n) { return 1.f - std::acos(n.y) / PI; };
		const vec2 tab{ 0.f, vCoord(ab) }, tac{ 0.f, vCoord(ac) }, tbc{ 0.f, vCoord(bc) };
		makeSkySphere(vertices, r, detail - 1, a, ab, ac, ta, tab, tac);
		makeSkySphere(vertices, r, detail - 1, b, bc, ab, tb, tbc, tab);
		makeSkySphere(vertices, r, detail - 1, c, ac, bc, tc, tac, tbc);
		makeSkySphere(vertices, r, detail - 1, ab, bc, ac, tab, tbc, tac);
	}

	std::vector<SkyVertex> buildSkySphere(float r, int detail) {
		std::vector<SkyVertex> vertices;
		// Base octahedron corners; the u coordinate is ignored, the strip only uses the altitude.
		static const std::array<vec3, 6> POSITIONS{ {
			{ 0.f, 0.f, -1.f }, { 1.f, 0.f, 0.f }, { 0.f, -1.f, 0.f },
			{ -1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f }
		} };
		static const std::array<vec2, 6> UVS{ {
			{ 0.f, 0.5f }, { 0.f, 0.5f }, { 0.f, 0.f },
			{ 0.f, 0.5f }, { 0.f, 1.f }, { 0.f, 0.5f }
		} };
		static const std::array<std::array<int, 3>, 8> TRIANGLES{ {
			{ 4, 3, 0 }, { 1, 4, 0 },
			{ 3, 4, 5 }, { 4, 1, 5 },
			{ 0, 3, 2 }, { 0, 2, 1 },
			{ 5, 2, 3 }, { 5, 1, 2 }
		} };
		for (const auto& tri : TRIANGLES)
			makeSkySphere(vertices, r, detail, POSITIONS[tri[0]], POSITIONS[tri[1]], POSITIONS[tri[2]],
				UVS[tri[0]], UVS[tri[1]], UVS[tri[2]]);
		return vertices;
	}
}

void SkyRenderer::load() {
	m_shader.loadFromFile("Data/Shaders/sky.vs", "Data/Shaders/sky.frag");

	// Loaded here rather than through Texture2D: its loadFromFile uploads a local copy that is
	// destroyed at return, so this texture would never receive an id.
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // PNG top = v = 1 = zenith
	unsigned char* data = stbi_load("Data/Textures/Sky/sky.png", &width, &height, &nrChannels, 4);
	if (data == nullptr) {
		LOG(Level::ERROR) << "Failed to load sky texture" << std::endl;
		return;
	}
	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	// The strip loops seamlessly at midnight, so u can wrap; the altitude axis clamps at the poles.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(data);

	const std::vector<SkyVertex> vertices = buildSkySphere(1.f, 3);
	std::vector<GLuint> indices(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
		indices[i] = static_cast<GLuint>(i);
	m_mesh.loadBuffers(vertices, indices);
	m_mesh.loadVAOs();
}

void SkyRenderer::render(const Camera& camera, float timer) {
	// The dome follows the camera: drop the view translation so the unit sphere stays centered on it.
	mat4 view = camera.getViewMatrix();
	view[3][0] = view[3][1] = view[3][2] = 0.f;
	const mat4 mvp = camera.getProjMatrix() * view;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	m_shader.use();
	m_shader.set("mvp", mvp);
	m_shader.set("timer", timer);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	m_mesh.draw();
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}