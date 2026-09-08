#include "PostProcessingRenderer.h"

#include "game/DayCycle.h"
#include "world/CubeData.h"
#include "resources/ResManager.h"
#include "util/RenderTexture.h"

PostProcessingRenderer::PostProcessingRenderer(ivec2 windowSize, const DayCycle& dayCycle)
	: m_dayCycle(dayCycle) {
	m_shader.loadFromFile("assets/Shaders/post_processing.vs", "assets/Shaders/post_processing.frag");

	getShader().use().set("renderTexture", 0);

	std::vector<PostProcessingMesh::Vertex> vertices;
	std::vector<GLuint> indices;
	auto& face = CubeData::dirToFace[static_cast<int>(Dir3D::DOWN)];
	for (int i = 0; i < 4; ++i) {
		vec2 pos = vec2{ face[i].x, face[i].z } * 2.f - 1.f;
		vec2 tex = CubeData::faceCoords[(i + 1) % 4];
		vertices.push_back({ pos, tex });
	}

	for (int i = 0; i < 6; ++i) {
		indices.push_back(CubeData::faceElementIndices[i]);
	}

	m_mesh.loadBuffers(vertices, indices);
	m_mesh.loadVAOs();

	onChangedSize(windowSize);
}

void PostProcessingRenderer::prepare(std::function<void()> renderFunc, std::function<void()> clearFunc) {
	m_renderTexture.setActive(true);
	clearFunc();
	renderFunc();
	m_renderTexture.display();
}

void PostProcessingRenderer::render() {
	m_shader.use();
	glActiveTexture(GL_TEXTURE0);
	sf::Texture::bind(&m_renderTexture.getTexture());
	m_mesh.draw();
}

void PostProcessingRenderer::update(sf::Time dt) {
	m_lavaTime = std::fmod(m_lavaTime + dt.asSeconds(), 3600.f);

	getShader().use().set("underwater", m_underwater);
	getShader().use().set("inLava", m_inLava);
	getShader().use().set("time", m_lavaTime);
}

void PostProcessingRenderer::setUnderwater(bool underwater) {
	m_underwater = underwater;
}

void PostProcessingRenderer::setInLava(bool inLava) {
	m_inLava = inLava;
}

void PostProcessingRenderer::onChangedSize(ivec2 windowSize) {
	// Disable antialiasing as it creates artifacts for distant blocks.
	createRenderTexture(m_renderTexture, windowSize, "render texture");
	// Linear filtering so the FXAA pass can sample between texels.
	m_renderTexture.setSmooth(true);
}

const Shader& PostProcessingRenderer::getShader() const {
	return m_shader;
}

sf::RenderTexture& PostProcessingRenderer::getRenderTexture() {
	return m_renderTexture;
}


