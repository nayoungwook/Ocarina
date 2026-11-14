#include "mesh.h"

Mesh::Mesh(const float *vertices, int vertex_count, const unsigned *indices,
	int index_count, int vertex_float_count)
	: index_count(index_count), vertex_float_count(vertex_float_count) {
	// Mesh is the low-level VAO/VBO/EBO wrapper for custom geometry.
	init(vertices, vertex_count, indices, index_count);
}

Mesh::Mesh(const std::vector<float> &vertices,
	const std::vector<unsigned> &indices, int vertex_float_count)
	: Mesh(vertices.data(), static_cast<int>(vertices.size()), indices.data(),
		static_cast<int>(indices.size()), vertex_float_count) {
}

Mesh::~Mesh() {
	if (ebo != 0)
		glDeleteBuffers(1, &ebo);
	if (vbo != 0)
		glDeleteBuffers(1, &vbo);
	if (vao != 0)
		glDeleteVertexArrays(1, &vao);
}

void Mesh::init(const float *vertices, int vertex_count,
	const unsigned *indices, int index_count) {
	// Attribute 0 is position. Attribute 1 is optional UV data when each
	// vertex contains at least five floats: x, y, z, u, v.
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(float), vertices,
		GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(unsigned),
		indices, GL_STATIC_DRAW);

	int stride = vertex_float_count * static_cast<int>(sizeof(float));
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
	glEnableVertexAttribArray(0);

	// for UV
	if (vertex_float_count >= 5) {
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
			(void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}

	// for Normal
	if (vertex_float_count >= 8) {
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
			(void *)(5 * sizeof(float)));
		glEnableVertexAttribArray(2);
	}

	glBindVertexArray(0);
}

unsigned Mesh::get_vao() const { return vao; }

int Mesh::get_index_count() const { return index_count; }

GLenum Mesh::get_draw_mode() const { return draw_mode; }

void Mesh::set_draw_mode(GLenum draw_mode) { this->draw_mode = draw_mode; }
