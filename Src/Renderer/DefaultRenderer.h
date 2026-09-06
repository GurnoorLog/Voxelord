#pragma once

#include "maths/GlmCommon.h"
#include "glad/glad.h"
#include "renderer/Renderer.h"
#include "resources/TextureArray.h"
#include "world/Mesh.h"

class Section;

class DefaultRenderer : public Renderer {
public:
	DefaultRenderer(const Camera& camera, const DayCycle& dayCycle);

	void render(const Section& section) const override;
	void render(const DefaultMesh& mesh) const;
	void update(sf::Time dt) override;
	TextureArray& getTextureArray();

private:
	TextureArray texArray;
};
