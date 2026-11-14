#include "sprite_batch.h"

#include "shader.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

static constexpr int VERTICES_PER_SPRITE = 4;
static constexpr int INDICES_PER_SPRITE = 6;

static unsigned char color_channel_to_byte(float value) {
	return static_cast<unsigned char>(std::round(
		std::max(0.0f, std::min(value, 1.0f)) * 255.0f));
}

SpriteBatch::SpriteBatch() {
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

	constexpr GLsizei STRIDE = sizeof(Vertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE,
		(void *)offsetof(Vertex, x));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, STRIDE,
		(void *)offsetof(Vertex, u));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, STRIDE,
		(void *)offsetof(Vertex, r));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
}

SpriteBatch::~SpriteBatch() {
	if (ebo != 0)
		glDeleteBuffers(1, &ebo);
	if (vbo != 0)
		glDeleteBuffers(1, &vbo);
	if (vao != 0)
		glDeleteVertexArrays(1, &vao);
}

void SpriteBatch::append_sprite(const SpriteInstance &instance, int index) {
	const float half_width = instance.transform.z * 0.5f;
	const float half_height = instance.transform.w * 0.5f;
	const float cosine = std::cos(instance.rotation);
	const float sine = std::sin(instance.rotation);
	const float local_x[VERTICES_PER_SPRITE] = {
		-half_width, half_width, half_width, -half_width };
	const float local_y[VERTICES_PER_SPRITE] = {
		half_height, half_height, -half_height, -half_height };
	const float tex_u[VERTICES_PER_SPRITE] = {
		instance.uv.x, instance.uv.z, instance.uv.z, instance.uv.x };
	const float tex_v[VERTICES_PER_SPRITE] = {
		instance.uv.y, instance.uv.y, instance.uv.w, instance.uv.w };
	const unsigned char red = color_channel_to_byte(instance.color.r);
	const unsigned char green = color_channel_to_byte(instance.color.g);
	const unsigned char blue = color_channel_to_byte(instance.color.b);
	const unsigned char alpha = color_channel_to_byte(instance.color.a);

	for (int vertex = 0; vertex < VERTICES_PER_SPRITE; ++vertex) {
		const float x = local_x[vertex];
		const float y = local_y[vertex];
		Vertex &output = vertices[
			static_cast<std::size_t>(index) * VERTICES_PER_SPRITE + vertex];
		output.x = instance.transform.x + x * cosine - y * sine;
		output.y = instance.transform.y + x * sine + y * cosine;
		output.z = instance.z;
		output.u = tex_u[vertex];
		output.v = tex_v[vertex];
		output.r = red;
		output.g = green;
		output.b = blue;
		output.a = alpha;
	}

	const unsigned base = static_cast<unsigned>(index * VERTICES_PER_SPRITE);
	unsigned *sprite_indices = indices.data() +
		static_cast<std::size_t>(index) * INDICES_PER_SPRITE;
	sprite_indices[0] = base;
	sprite_indices[1] = base + 1;
	sprite_indices[2] = base + 2;
	sprite_indices[3] = base + 2;
	sprite_indices[4] = base + 3;
	sprite_indices[5] = base;
}

void SpriteBatch::draw(Texture *texture, Shader *shader,
	const SpriteInstance *instances, int count) {
	if (texture == nullptr || shader == nullptr || instances == nullptr || count <= 0)
		return;

	vertices.resize(static_cast<std::size_t>(count) * VERTICES_PER_SPRITE);
	indices.resize(static_cast<std::size_t>(count) * INDICES_PER_SPRITE);
	for (int i = 0; i < count; ++i)
		append_sprite(instances[i], i);

	shader->bind();
	texture->bind(0);
	shader->upload_mat4("uModel", glm::mat4(1.0f));
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
		vertices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)),
		indices.data(), GL_DYNAMIC_DRAW);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
		GL_UNSIGNED_INT, nullptr);
}
