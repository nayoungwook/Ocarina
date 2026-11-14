#include "scene.h"

#include "camera.h"
#include "engine.h"
#include "framebuffer.h"
#include "mesh.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"

#include <algorithm>
#include <cmath>

static bool z_order_less(const RenderContext &a, const RenderContext &b) {
	return a.z_order < b.z_order;
}

static bool same_texture_batch(const RenderContext &first, const RenderContext &next) {
	const bool same_mask = first.mask_enabled == next.mask_enabled &&
		(!first.mask_enabled ||
			(first.mask_rect.x == next.mask_rect.x &&
			 first.mask_rect.y == next.mask_rect.y &&
			 first.mask_rect.z == next.mask_rect.z &&
			 first.mask_rect.w == next.mask_rect.w));
	
	return first.render_type == RT_TEXTURE && next.render_type == RT_TEXTURE &&
		!first.uniform_state && !next.uniform_state &&
		first.texture == next.texture && first.shader == next.shader &&
		first.framebuffer == next.framebuffer && first.is_ui == next.is_ui &&
		first.blend_source == next.blend_source &&
		first.blend_destination == next.blend_destination && same_mask;
}

static void stable_sort_z_segments(std::vector<RenderContext> &queue) {
	auto begin = queue.begin();
	for (auto it = queue.begin(); it != queue.end(); ++it) {
		if (it->render_type != RT_CLEAR)
			continue;
		std::stable_sort(begin, it, z_order_less);
		begin = it + 1;
	}
	std::stable_sort(begin, queue.end(), z_order_less);
}

static void upload_uniform_map(Shader *shader, const UniformMap &uniforms) {
	if (shader == nullptr)
		return;
	for (const auto &entry : uniforms) {
		const UniformValue &value = entry.second;
		switch (value.type) {
		case UniformValue::Float:
			shader->upload_float(entry.first, value.float_value);
			break;
		case UniformValue::Int:
			shader->upload_int(entry.first, value.int_value);
			break;
		case UniformValue::Vec2:
			shader->upload_vec2(entry.first, value.vec2_value);
			break;
		case UniformValue::Vec3:
			shader->upload_vec3(entry.first, value.vec3_value);
			break;
		case UniformValue::Vec4:
			shader->upload_vec4(entry.first, value.vec4_value);
			break;
		case UniformValue::Mat4:
			shader->upload_mat4(entry.first, value.mat4_value);
			break;
		}
	}
}

Scene::Scene(Engine *engine) : engine(engine) {
	render_context_queue.reserve(4096);
	instance_batch.reserve(4096);
}

Engine *Scene::get_engine() { return engine; }

// RenderContext 의 culling 을 위한 화면에 렌더링되는지 여부.
bool Scene::is_visible(const RenderContext &context) const {
	if (context.is_ui || context.render_type == RT_CLEAR ||
		context.render_type == RT_FRAMEBUFFER)
		return true;
	Camera *camera = engine->get_camera();
	if (camera == nullptr || std::abs(camera->zoom) < 0.0001f)
		return false;
	float half_width = camera->get_resolution_x() * 0.5f /
		std::abs(camera->zoom);
	float half_height = camera->get_resolution_y() * 0.5f /
		std::abs(camera->zoom);
	float culling_center_x = camera->position.x;
	float culling_center_y = camera->position.y;
	float safety_margin = 0.0f;

	if (context.framebuffer != nullptr &&
		context.framebuffer->is_pixel_perfect()) {
		const float units = context.framebuffer->get_pixel_per_unit();
		half_width = context.framebuffer->get_width() *
			units * 0.5f;
		half_height = context.framebuffer->get_height() *
			units * 0.5f;
		culling_center_x = std::floor(camera->position.x / units) * units;
		culling_center_y = std::floor(camera->position.y / units) * units;
		safety_margin = units;
	}

	const float radius = std::sqrt(half_width * half_width +
		half_height * half_height) +
		std::sqrt(context.width * context.width + context.height * context.height) *
		0.5f + safety_margin;
	const float dx = context.position.x - culling_center_x;
	const float dy = context.position.y - culling_center_y;
	return dx * dx + dy * dy <= radius * radius;
}

void Scene::add_render_context(const RenderContext &context) {
	last_render_context = context;
	has_last_render_context = true;

	if (!context.cullable || is_visible(context)) {
		if (context.render_type != RT_CLEAR &&
			!render_context_queue.empty() &&
			render_context_queue.back().render_type != RT_CLEAR &&
			render_context_queue.back().z_order > context.z_order) {
			render_context_queue_sorted = false;
		}

		render_context_queue.push_back(context);
	}
}

glm::mat4 Scene::view_projection_for(const RenderContext &context) const {
	Camera *camera = engine->get_camera();

	if (context.is_ui) // UI 의 경우 카메라 위치를 고려하지 않기 때문에 projection 만.
		return camera->get_projection();

	if (context.framebuffer != nullptr && context.framebuffer->is_pixel_perfect()) { // pixel perfect viewprojection
		return camera->get_pixel_perfect_projection(context.framebuffer->get_width(),
			context.framebuffer->get_height(), context.framebuffer->get_pixel_per_unit()) *
			camera->get_pixel_perfect_view(context.framebuffer->get_pixel_per_unit());
	}

	return camera->get_view_projection();
}

void Scene::flush_batch(Renderer *renderer,
	std::vector<SpriteInstance> &batch, Texture *texture) {

	if (batch.empty() || texture == nullptr)
		return;

	renderer->draw_sprite_batch(texture, batch.data(),
		static_cast<int>(batch.size()));
	batch.clear();
}

void Scene::render_queue(std::vector<RenderContext> &queue, bool needs_sort,
	bool separate_alpha_blending) {
	if (queue.empty())
		return;

	if (needs_sort)
		stable_sort_z_segments(queue);

	Renderer *renderer = engine->get_renderer();
	Camera *camera = engine->get_camera();
	camera->calculate_matrix();

	for (std::size_t index = 0; index < queue.size();) {
		const RenderContext &context = queue[index];

		if (context.framebuffer != nullptr) {
			context.framebuffer->bind();
		}
		else {
			FrameBuffer::bind_screen_framebuffer();
		}

		if (context.render_type == RT_CLEAR) {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			++index;
			continue;
		}

		const glm::mat4 view_projection = view_projection_for(context);
		if (separate_alpha_blending && context.blend_source == GL_SRC_ALPHA &&
			context.blend_destination == GL_ONE_MINUS_SRC_ALPHA) {
			// Shape shaders already output premultiplied RGB.
			glBlendFuncSeparate(context.render_type == RT_SHAPE ? GL_ONE : GL_SRC_ALPHA,
				GL_ONE_MINUS_SRC_ALPHA,
				GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		else {
			glBlendFunc(context.blend_source, context.blend_destination);
		}

		if (context.render_type == RT_TEXTURE && context.texture != nullptr &&
			context.shader != nullptr) {
			const bool pixel_perfect_target = context.framebuffer != nullptr &&
				context.framebuffer->is_pixel_perfect() && !context.is_ui;
			const glm::mat4 sprite_projection = pixel_perfect_target ?
				camera->get_pixel_perfect_projection(context.framebuffer->get_width(),
					context.framebuffer->get_height(), context.framebuffer->get_pixel_per_unit()) :
				camera->get_projection();
			const glm::mat4 sprite_view = pixel_perfect_target ?
				camera->get_pixel_perfect_view(context.framebuffer->get_pixel_per_unit()) :
				(context.is_ui ? glm::mat4(1.0f) : camera->get_view());

			context.shader->bind();
			context.shader->upload_int("uTexture", 0);
			context.shader->upload_mat4("uViewProjection", view_projection);
			context.shader->upload_mat4("uProjection", sprite_projection);
			context.shader->upload_mat4("uView", sprite_view);
			renderer->upload_ui_mask(context.shader, context);
			renderer->set_sprite_matrices(sprite_view, sprite_projection);
			renderer->set_shader(context.shader);

			if (context.uniform_state) {
				upload_uniform_map(context.shader, context.uniform_state->values);
				SpriteInstance instance{
					{context.position.x, context.position.y, context.width, context.height},
					context.color, context.texture->get_atlas_uv(context.slot),
					context.rotation.z, context.position.z };
				renderer->draw_sprite_batch(context.texture, &instance, 1);
				upload_uniform_map(context.shader,
					context.uniform_state->reset_values);
				++index;
				continue;
			}

			std::size_t batch_end = index + 1;

			while (batch_end < queue.size() &&
				same_texture_batch(context, queue[batch_end]))
				++batch_end;

			instance_batch.resize(batch_end - index);

			for (std::size_t i = index; i < batch_end; ++i) {
				const RenderContext &item = queue[i];
				instance_batch[i - index] = {
					{item.position.x, item.position.y, item.width, item.height},
					item.color, item.texture->get_atlas_uv(item.slot),
					item.rotation.z, item.position.z };
			}

			flush_batch(renderer, instance_batch, context.texture);
			index = batch_end;
			continue;
		}

		if (context.uniform_state)
			upload_uniform_map(context.shader, context.uniform_state->values);
		if (context.render_type == RT_SHAPE)
			renderer->draw_shape(context, view_projection);
		else if (context.render_type == RT_FRAMEBUFFER)
			renderer->draw_raw_texture(context.raw_texture, context,
				view_projection);
		else if (context.render_type == RT_MESH)
			renderer->draw_mesh(context, view_projection);
		if (context.uniform_state)
			upload_uniform_map(context.shader,
				context.uniform_state->reset_values);
		++index;
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	queue.clear();
}

void Scene::flush_render_context(bool separate_alpha_blending) {
	bool needs_sort = !render_context_queue_sorted;

	render_queue(render_context_queue, needs_sort, separate_alpha_blending);
	has_last_render_context = false;
	render_context_queue_sorted = true;
}

bool Scene::get_last_render_context(RenderContext &render_context) const {
	if (!has_last_render_context || last_render_context.render_type == RT_CLEAR)
		return false;
	render_context = last_render_context;
	return true;
}

bool Scene::copy_last_render_context_to_framebuffer(FrameBuffer *framebuffer) {
	if (framebuffer == nullptr || render_context_queue.empty() ||
		render_context_queue.back().render_type == RT_CLEAR)
		return false;
	RenderContext copy = render_context_queue.back();
	copy.framebuffer = framebuffer;
	add_render_context(copy);
	return true;
}

void Scene::start_frame() {
	render_context_queue.clear();
	has_last_render_context = false;
	render_context_queue_sorted = true;
}
