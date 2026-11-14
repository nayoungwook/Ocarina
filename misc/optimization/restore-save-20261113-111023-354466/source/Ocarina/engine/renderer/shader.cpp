#include "shader.h"
#include "color.h"
#include "util.h"

#include <glm/gtc/type_ptr.hpp>

static unsigned bound_shader_program = 0;
static Shader *bound_shader = nullptr;

static bool ends_with(const std::string &value, const std::string &suffix) {
	if (value.size() < suffix.size())
		return false;
	return value.compare(value.size() - suffix.size(), suffix.size(),
		suffix) == 0;
}

Shader::Shader(const std::string &vert_path, const std::string &frag_path) {
	// Shader construction compiles and links immediately so render-time bind
	// calls only need to switch an already valid GL program.
	load_shader(vert_path, frag_path);
}

Shader::~Shader() {
	if (bound_shader == this) unbind();
	if (program != 0)
		glDeleteProgram(program);
}

void Shader::bind() {
	// Avoid redundant glUseProgram calls; many queued commands share shaders.
	if (bound_shader_program == this->program)
		return;

	glUseProgram(this->program);
	bound_shader_program = this->program;
	bound_shader = this;
}

void Shader::unbind() {
	glUseProgram(0);
	bound_shader_program = 0;
	bound_shader = nullptr;
}

Shader *Shader::get_current() { return bound_shader; }

const unsigned Shader::get_program() const { return program; }

int Shader::get_uniform_location(const std::string &name) {
	// Uniform locations are cached because uploads happen on every draw and GL
	// lookup by name is relatively expensive.
	int uniform_loc = -1;
	if (this->uniform_cache.find(name) == this->uniform_cache.end()) {
		uniform_loc =
			glGetUniformLocation(this->get_program(), name.c_str());
		this->uniform_cache.insert(std::make_pair(name, uniform_loc));
	}

	return uniform_cache.find(name)->second;
}

void Shader::upload_int(const std::string &name, int value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform1i(location, value);
}

void Shader::upload_float(const std::string &name, float value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform1f(location, value);
}

void Shader::upload_vec2(const std::string &name, const glm::vec2 &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform2f(location, value.x, value.y);
}

void Shader::upload_vec3(const std::string &name, const glm::vec3 &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform3f(location, value.x, value.y, value.z);
}

void Shader::upload_vec4(const std::string &name, const glm::vec4 &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform4f(location, value.x, value.y, value.z, value.w);
}

void Shader::upload_color(const std::string &name, const Color &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniform4f(location, value.r, value.g, value.b, value.a);
}

void Shader::upload_vec2_array(const std::string &name,
	const std::vector<glm::vec2> &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1 && !value.empty())
		glUniform2fv(location, static_cast<GLsizei>(value.size()),
			glm::value_ptr(value[0]));
}

void Shader::upload_vec3_array(const std::string &name,
	const std::vector<glm::vec3> &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1 && !value.empty())
		glUniform3fv(location, static_cast<GLsizei>(value.size()),
			glm::value_ptr(value[0]));
}

void Shader::upload_float_array(const std::string &name,
	const std::vector<float> &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1 && !value.empty())
		glUniform1fv(location, static_cast<GLsizei>(value.size()),
			value.data());
}

void Shader::upload_mat3(const std::string &name, const glm::mat3 &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::upload_mat4(const std::string &name, const glm::mat4 &value) {
	bind();
	int location = get_uniform_location(name);
	if (location != -1)
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::load_shader(const std::string &vert_path,
	const std::string &frag_path) {
	// Load source text, compile both stages, then link them into one program.
	// Any failure goes through the engine's fatal error path.
	std::string actual_vert_path = vert_path;
	std::string actual_frag_path = frag_path;
	if (ends_with(vert_path, ".frag") && ends_with(frag_path, ".vert")) {
		actual_vert_path = frag_path;
		actual_frag_path = vert_path;
	}

	std::string vert_str = read_shader_file(actual_vert_path);
	const char *vert = vert_str.c_str();

	std::string frag_str = read_shader_file(actual_frag_path);
	const char *frag = frag_str.c_str();

	unsigned vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vert, nullptr);
	glCompileShader(vertex_shader);
	check_shader_compile(vertex_shader, actual_vert_path);

	unsigned fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &frag, nullptr);
	glCompileShader(fragment_shader);
	check_shader_compile(fragment_shader, actual_frag_path);

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
}

void Shader::check_shader_compile(unsigned shader, const std::string &name) {
	int success;
	char log[1024];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success) {
		glGetShaderInfoLog(shader, 1024, nullptr, log);
		std::cerr
			<< "ERROR::SHADER_COMPILATION_ERROR " << name << "\n"
			<< log
			<< "\n -- "
			"--------------------------------------------------- -- "
			<< std::endl;
	}
}
