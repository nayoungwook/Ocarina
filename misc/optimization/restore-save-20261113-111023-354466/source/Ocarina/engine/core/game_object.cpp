#include "game_object.h"
#include "engine.h"
#include "error.h"
#include "mesh.h"
#include "render_context.h"
#include "renderer.h"
#include "scene.h"
#include "shader_manager.h"
#include "texture.h"

#include <GL/glew.h>
#include <algorithm>
#include <cmath>

GameObject::GameObject(Engine *engine, const Vector &position, float width,
	float height)
	: engine(engine), position(position), width(width), height(height) {
	// GameObject is a convenience wrapper that turns object state into a
	// RenderContext; the Scene still owns the actual draw ordering.
	this->init();
	
	this->z_order = 1;
	this->slot = 0;
	this->texture = nullptr;
	this->shader = nullptr;
	this->blend_source = GL_SRC_ALPHA;
	this->blend_destination = GL_ONE_MINUS_SRC_ALPHA;
}

GameObject::GameObject(Engine *engine, float x, float y, float width,
	float height)
	: GameObject(engine, Vector(x, y), width, height) {}

void GameObject::render() {
	render_to_framebuffer(engine->get_framebuffer(), nullptr,
		blend_source, blend_destination, true);
}

void GameObject::render_to_framebuffer(FrameBuffer *framebuffer,
	Shader *override_shader, int source_blend, int destination_blend,
	bool include_uniforms) {
	Scene *current_scene = this->engine->get_current_scene();
	if (current_scene == nullptr) {
		error("current scene is null");
	}

	Shader *render_shader = override_shader == nullptr ? shader : override_shader;
	if (render_shader == nullptr) {
		render_shader = Shader::get_current();
		if (render_shader == nullptr)
			render_shader = mesh != nullptr
			? this->engine->get_shader_manager()->get_shader("default")
			: this->engine->get_renderer()->get_shader();
	}
	float render_x = position.x + render_offset_x;
	float render_y = position.y + render_offset_y;

	float render_width = width + render_padding_x;
	float render_height = height + render_padding_y;

	const float anchor_x = (anchor.x - 0.5f) * width;
	const float anchor_y = (anchor.y - 0.5f) * height;
	const float model_rotation = -rotation;
	const float cosine = std::cos(model_rotation);
	const float sine = std::sin(model_rotation);
	render_x += anchor_x - (cosine * anchor_x - sine * anchor_y);
	render_y += anchor_y - (sine * anchor_x + cosine * anchor_y);

	Vector render_position(render_x, render_y, position.z);

	if (flip_x)
		render_width *= -1.0f;
	if (flip_y)
		render_height *= -1.0f;

	RenderContext rc = {};

	if (mesh != nullptr) {
		rc = gen_mesh_render_context(framebuffer,
			mesh, render_shader, render_position, model_rotation,
			render_width, render_height, Color(1.0f, 1.0f, 1.0f, opacity), ui);
	}
	else {
		if (texture == nullptr) {
			error("GameObject has no texture or mesh");
			return;
		}

		rc = gen_render_context(framebuffer,
			this->texture, render_shader, render_position, this->slot,
			model_rotation, render_width, render_height,
			Color(1.0f, 1.0f, 1.0f, opacity), ui);
	}

	rc.z_order = z_order;
	rc.blend_source = source_blend;
	rc.blend_destination = destination_blend;
	rc.cullable = true;

	if (include_uniforms && (!uniforms.empty() || !reset_uniforms.empty())) {
		rc.uniform_state = std::make_shared<UniformState>(
			UniformState{ uniforms, reset_uniforms });
	}
	current_scene->add_render_context(rc);
}

void GameObject::init() {}
void GameObject::update() {}

bool GameObject::can_batch() const {
	return batchable && mesh == nullptr && uniforms.empty() &&
		blend_source == GL_SRC_ALPHA &&
		blend_destination == GL_ONE_MINUS_SRC_ALPHA;
}

void GameObject::set_render_offset(float x, float y) {
	render_offset_x = x;
	render_offset_y = y;
}

void GameObject::set_render_padding(float x, float y) {
	render_padding_x = x;
	render_padding_y = y;
}

GameObject &GameObject::set_uniform(const std::string &name, float value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::set_uniform(const std::string &name, int value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::set_uniform(const std::string &name, const glm::vec2 &value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::set_uniform(const std::string &name, const glm::vec3 &value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::set_uniform(const std::string &name, const glm::vec4 &value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::set_uniform(const std::string &name, const glm::mat4 &value) {
	uniforms[name] = value; return *this;
}
GameObject &GameObject::reset_uniform_after_draw(const std::string &name, float value) {
	reset_uniforms[name] = value; return *this;
}
GameObject &GameObject::reset_uniform_after_draw(const std::string &name, int value) {
	reset_uniforms[name] = value; return *this;
}
GameObject &GameObject::reset_uniform_after_draw(const std::string &name, const glm::vec2 &value) {
	reset_uniforms[name] = value; return *this;
}
GameObject &GameObject::reset_uniform_after_draw(const std::string &name, const glm::vec3 &value) {
	reset_uniforms[name] = value; return *this;
}
GameObject &GameObject::reset_uniform_after_draw(const std::string &name, const glm::vec4 &value) {
	reset_uniforms[name] = value; return *this;
}
void GameObject::clear_uniform(const std::string &name) { uniforms.erase(name); }
void GameObject::clear_uniforms() { uniforms.clear(); reset_uniforms.clear(); }

void GameObject::set_opacity(float opacity) {
	this->opacity = std::max(0.0f, std::min(1.0f, opacity));
}

float GameObject::get_opacity() const { return opacity; }

void GameObject::set_blend_mode(int source, int destination) {
	blend_source = source;
	blend_destination = destination;
}

int GameObject::get_render_atlas_index() const { return slot; }

void GameObject::set_render_atlas_index(int atlas_index) {
	slot = atlas_index;
}

bool GameObject::is_ui() const { return ui; }

void GameObject::set_is_ui(bool is_ui) { ui = is_ui; }
