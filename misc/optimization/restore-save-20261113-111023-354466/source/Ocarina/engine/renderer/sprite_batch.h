#pragma once

#include "color.h"

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;
class Texture;

struct SpriteInstance {
	glm::vec4 transform;
	Color color;
	glm::vec4 uv;
	float rotation;
	float z = 0.0f;
};

class SpriteBatch {
public:
	SpriteBatch();
	~SpriteBatch();
	SpriteBatch(const SpriteBatch &) = delete;
	SpriteBatch &operator=(const SpriteBatch &) = delete;

	void draw(Texture *texture, Shader *shader,
		const SpriteInstance *instances, int count);

private:
	struct Vertex {
		float x, y, z;
		float u, v;
		unsigned char r, g, b, a;
	};

	void append_sprite(const SpriteInstance &instance, int index);

	unsigned vao = 0, vbo = 0, ebo = 0;
	std::vector<Vertex> vertices;
	std::vector<unsigned> indices;
};
