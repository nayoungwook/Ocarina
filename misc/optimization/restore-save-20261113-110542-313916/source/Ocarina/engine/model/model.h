#pragma once

#include <string>
#include <vector>
#include <glm/vec3.hpp>

class Model {
public:
	explicit Model(const std::string &path);

	const std::vector<float> &get_vertices() const;
	const std::vector<unsigned> &get_indices() const;

private:
	glm::vec3 get_position(unsigned index) const;
	void add_normal(unsigned index, const glm::vec3 &normal);
	void load_model(const std::string &path);

	std::vector<float> vertices;
	std::vector<unsigned> indices;
};
