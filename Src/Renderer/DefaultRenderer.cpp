#include "DefaultRenderer.h"

#include "game/DayCycle.h"
#include "resources/ResManager.h"
#include "util/Logger.h"
#include "view/Camera.h"
#include "world/ChunkMap.h"
#include "world/Section.h"

#include <filesystem>

namespace fs = std::filesystem;

std::vector<fs::path> getPaths() {
	std::string blockTexturesPath = "assets/Textures/Blocks";
	std::vector<fs::path> paths;
	for (const fs::directory_entry& entry : fs::directory_iterator(blockTexturesPath)) {
		if (entry.path().extension().string() == ".png") {
			paths.push_back(entry.path());
		}
	}
	return paths;
}

DefaultRenderer::DefaultRenderer(const Camera& camera, const DayCycle& dayCycle)
	: Renderer(camera, dayCycle), texArray{ getPaths(), ivec2{ 16, 16 }, GL_RGBA } {
	m_shader.loadFromFile("assets/Shaders/cube.vs", "assets/Shaders/cube.frag");

	getShader().use().set("distance", ChunkMap::SIDE);
}

void DefaultRenderer::render(const Section& section) const {
	render(section.getDefaultMesh());
}

void DefaultRenderer::update(sf::Time) {
	getShader().use().set("dayFactor", m_dayCycle.getDayFactor());
	getShader().use().set("view", m_camera.getViewMatrix());
	getShader().use().set("projection", m_camera.getProjMatrix());
	getShader().use().set("skyColor", m_dayCycle.getSkyColor());
}

void DefaultRenderer::render(const DefaultMesh& mesh) const {
	if (mesh.indicesNb != 0) {
		glActiveTexture(GL_TEXTURE0);
		texArray.bind();
		m_shader.use();
		mesh.draw();
		texArray.unbind();
	}
}

TextureArray & DefaultRenderer::getTextureArray() {
	return texArray;
}
