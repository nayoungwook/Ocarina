#pragma once

#include "mathf.h"
#include "render_context.h"
#include <glm/glm.hpp>

class Engine;
class Texture;
class Shader;
class Mesh;
class FrameBuffer;

class GameObject {
public:
	GameObject(Engine *engine, const Vector &position, float width, float height);
	GameObject(Engine *engine, float x, float y, float width, float height);
	virtual ~GameObject() = default;

	Vector position;
	int z_order;
	Vector anchor = Vector(0.5f, 0.5f);
	float width, height;
	float rotation = 0.0f;

	Texture *texture;
	Mesh *mesh = nullptr;
	Shader *shader;
	bool flip_x = false;
	bool flip_y = false;
	bool batchable = true;
	float render_offset_x = 0.0f;
	float render_offset_y = 0.0f;
	float render_padding_x = 0.0f;
	float render_padding_y = 0.0f;
	int blend_source;
	int blend_destination;

	virtual void init();
	virtual void update();
	virtual void render();
	void render_to_framebuffer(FrameBuffer *framebuffer,
		Shader *override_shader, int source_blend, int destination_blend,
		bool include_uniforms = true);
	bool can_batch() const;
	void set_render_offset(float x, float y);
	void set_render_padding(float x, float y);
	GameObject &set_uniform(const std::string &name, float value);
	GameObject &set_uniform(const std::string &name, int value);
	GameObject &set_uniform(const std::string &name, const glm::vec2 &value);
	GameObject &set_uniform(const std::string &name, const glm::vec3 &value);
	GameObject &set_uniform(const std::string &name, const glm::vec4 &value);
	GameObject &set_uniform(const std::string &name, const glm::mat4 &value);
	GameObject &reset_uniform_after_draw(const std::string &name, float value);
	GameObject &reset_uniform_after_draw(const std::string &name, int value);
	GameObject &reset_uniform_after_draw(const std::string &name, const glm::vec2 &value);
	GameObject &reset_uniform_after_draw(const std::string &name, const glm::vec3 &value);
	GameObject &reset_uniform_after_draw(const std::string &name, const glm::vec4 &value);
	void clear_uniform(const std::string &name);
	void clear_uniforms();
	void set_opacity(float opacity);
	float get_opacity() const;
	void set_blend_mode(int source, int destination);
	int get_render_atlas_index() const;
	void set_render_atlas_index(int atlas_index);
	bool is_ui() const;
	void set_is_ui(bool is_ui);

	Engine *get_engine();

private:
	int slot;
	Engine *engine;
	bool ui = false;
	float opacity = 1.0f;
	UniformMap uniforms;
	UniformMap reset_uniforms;
};
