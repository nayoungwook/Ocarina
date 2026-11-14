#include "model.h"
#include <error.h>
#include "util.h"

#include <glm/glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <memory>
#include <stdexcept>

Model::Model(const std::string &path) {
	load_model(path);
}

void Model::load_model(const std::string &path) {
	const std::string full_path = resolve_resource_path(path);
	size_t size = 0;
	std::unique_ptr<void, decltype(&SDL_free)> data(
		SDL_LoadFile(full_path.c_str(), &size), SDL_free);

	if (!data)
		error("Failed to load model (obj file) : " + path);

	const std::string text(static_cast<const char *>(data.get()), size);
	data.reset();

	tinyobj::ObjReaderConfig config;
	config.triangulate = true;
	config.vertex_color = false;
	tinyobj::ObjReader reader;

	if (!reader.ParseFromString(text, "", config))
		error("Failed to parse model (obj file) : " + path);

	const auto &positions = reader.GetAttrib().vertices;
	vertices.assign(positions.begin(), positions.end());

	std::vector<float> packed_vertices;
	packed_vertices.reserve(vertices.size() / 3 * (3 + 2 + 3));
	for (size_t i = 0; i < vertices.size(); i += 3) {
		packed_vertices.insert(packed_vertices.end(), {
			vertices[i], // x
			vertices[i + 1], // y
			vertices[i + 2], // z
			0.0f,  // u
			0.0f,   // v
			0.0f, // nx
			0.0f, // ny
			0.0f, // nz
			});
	}
	vertices.swap(packed_vertices);

	std::size_t index_count = 0;

	for (const auto &shape : reader.GetShapes())
		index_count += shape.mesh.indices.size();

	indices.reserve(index_count);

	for (const auto &shape : reader.GetShapes()) {
		for (const auto &index : shape.mesh.indices) {
			if (index.vertex_index < 0 ||
				static_cast<std::size_t>(index.vertex_index) >= positions.size() / 3)
				error("OBJ vertex index out of range : " + full_path);

			indices.push_back(static_cast<unsigned>(index.vertex_index));
		}
	}

	for (size_t i = 0; i < indices.size(); i += 3) {
		unsigned a = indices[i];
		unsigned b = indices[i + 1];
		unsigned c = indices[i + 2];

		glm::vec3 p0 = get_position(a);
		glm::vec3 p1 = get_position(b);
		glm::vec3 p2 = get_position(c);

		glm::vec3 face_normal = glm::cross(p1 - p0, p2 - p0);

		add_normal(a, face_normal);
		add_normal(b, face_normal);
		add_normal(c, face_normal);
	}
	
	// normalize normals
	for (size_t i = 0; i < vertices.size(); i += 8) {
		glm::vec3 normal(
			vertices[i + 5],
			vertices[i + 6],
			vertices[i + 7]
		);

		float length_squared = glm::dot(normal, normal);

		if (length_squared > 0.000001f) {
			normal = glm::normalize(normal);
		}

		vertices[i + 5] = normal.x;
		vertices[i + 6] = normal.y;
		vertices[i + 7] = normal.z;
	}

	if (vertices.empty() || indices.empty())
		error("OBJ has no triangle geometry: " + full_path);
}

const std::vector<float> &Model::get_vertices() const { return vertices; }
const std::vector<unsigned> &Model::get_indices() const { return indices; }


glm::vec3 Model::get_position(unsigned index) const {
	size_t offset = static_cast<size_t>(index) * 8;
	return glm::vec3(vertices[offset], vertices[offset + 1], vertices[offset + 2]);
}

void Model::add_normal(unsigned index, const glm::vec3 &normal) {
	size_t offset = static_cast<size_t>(index) * 8;
	vertices[offset + 5] += normal.x;
	vertices[offset + 6] += normal.y;
	vertices[offset + 7] += normal.z;
}
