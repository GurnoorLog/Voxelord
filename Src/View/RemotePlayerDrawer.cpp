#include "RemotePlayerDrawer.h"

#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "physics/PlayerController.h"

namespace {
	// A unit cube centered on the origin, laid out as 4 vertices per face (24 total) so the
	// shared corner vertices keep per-face UV-free flat coloring trivial.
	const std::array<vec3, 24> cubeVertices{ {
		{ -0.5f, -0.5f,  0.5f }, {  0.5f, -0.5f,  0.5f }, {  0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }, // front
		{  0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f, -0.5f }, // back
		{ -0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f,  0.5f }, {  0.5f,  0.5f,  0.5f }, {  0.5f,  0.5f, -0.5f }, // top
		{ -0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f,  0.5f }, { -0.5f, -0.5f,  0.5f }, // bottom
		{ -0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f, -0.5f }, // left
		{  0.5f, -0.5f,  0.5f }, {  0.5f, -0.5f, -0.5f }, {  0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f,  0.5f }  // right
	} };

	const std::array<GLuint, 36> cubeIndices{
		0, 1, 2,  2, 3, 0,      4, 5, 6,  6, 7, 4,
		8, 9, 10,  10, 11, 8,   12, 13, 14,  14, 15, 12,
		16, 17, 18,  18, 19, 16, 20, 21, 22,  22, 23, 20
	};
}

RemotePlayerDrawer::RemotePlayerDrawer() {
	m_shader.loadFromFile("assets/Shaders/flat.vs", "assets/Shaders/flat.frag");
	buildMesh();
}

RemotePlayerDrawer::~RemotePlayerDrawer() {
	if (m_VAO != 0)
		glDeleteVertexArrays(1, &m_VAO);
	if (m_VBO != 0)
		glDeleteBuffers(1, &m_VBO);
	if (m_EBO != 0)
		glDeleteBuffers(1, &m_EBO);
}

void RemotePlayerDrawer::buildMesh() {
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &m_EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (GLvoid*)0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RemotePlayerDrawer::drawCube(const mat4& model, const vec3& color) {
	m_shader.set("model", model);
	m_shader.set("color", vec4(color, 1.f));
	glBindVertexArray(m_VAO);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cubeIndices.size()), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void RemotePlayerDrawer::render(const std::vector<RemotePlayerVisual>& players, const mat4& view, const mat4& projection) {
	if (players.empty())
		return;

	m_shader.use();
	m_shader.set("view", view);
	m_shader.set("projection", projection);

	const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_TEST);

	// Body: 0.6 x 1.2 x 0.6, feet on the ground. Head: 0.5 cube resting on top. Eyes sit at the
	// same height as the physics camera so remote players align with where the local player looks from.
	const float EYE_HEIGHT = PlayerController::PLAYER_HEAD_HEIGHT;
	const float BODY_BOTTOM = EYE_HEIGHT - 1.32f; // body spans [feet, feet + 1.2]
	const float HEAD_BOTTOM = EYE_HEIGHT - 0.42f;

	for (const RemotePlayerVisual& player : players) {
		vec3 feet = player.eyePosition - vec3(0.f, EYE_HEIGHT, 0.f);
		float yawRad = glm::radians(player.yaw);

		mat4 body = glm::translate(mat4(1.f), feet + vec3(0.f, BODY_BOTTOM + 0.6f, 0.f));
		body = glm::rotate(body, yawRad, vec3(0.f, 1.f, 0.f));
		body = glm::scale(body, vec3(0.6f, 1.2f, 0.6f));
		drawCube(body, player.color);

		mat4 head = glm::translate(mat4(1.f), feet + vec3(0.f, HEAD_BOTTOM + 0.25f, 0.f));
		head = glm::rotate(head, yawRad, vec3(0.f, 1.f, 0.f));
		head = glm::scale(head, vec3(0.5f, 0.5f, 0.5f));
		drawCube(head, player.color);
	}

	if (!depthWasEnabled)
		glDisable(GL_DEPTH_TEST);
}