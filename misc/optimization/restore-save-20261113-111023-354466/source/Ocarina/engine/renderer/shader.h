#pragma once

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

struct Color;

class Shader {
public:
	Shader(const std::string &vert_path, const std::string &frag_path);
	~Shader();

	void bind();
	void unbind();
	static Shader *get_current();

	const unsigned get_program() const;
	int get_uniform_location(const std::string &name);
	void upload_int(const std::string &name, int value);
	void upload_float(const std::string &name, float value);
	void upload_vec2(const std::string &name, const glm::vec2 &value);
	void upload_vec3(const std::string &name, const glm::vec3 &value);
	void upload_vec4(const std::string &name, const glm::vec4 &value);
	void upload_color(const std::string &name, const Color &value);
	void upload_vec2_array(const std::string &name,
		const std::vector<glm::vec2> &value);
	void upload_vec3_array(const std::string &name,
		const std::vector<glm::vec3> &value);
	void upload_float_array(const std::string &name,
		const std::vector<float> &value);
	void upload_mat3(const std::string &name, const glm::mat3 &value);
	void upload_mat4(const std::string &name, const glm::mat4 &value);

private:

	std::unordered_map<std::string, int> uniform_cache;

	void check_shader_compile(unsigned id, const std::string &name);
	unsigned fragment, vertex, program;
	void load_shader(const std::string &vert_path, const std::string &frag_path);
};
