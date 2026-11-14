#include "render_context.h"

#include "framebuffer.h"

#include <GL/glew.h>
#include <iostream>

// RenderContext factories normalize all renderer submissions into one command
// shape. Scene can then sort, batch, and dispatch without knowing which public
// Renderer/GameObject helper created the command.
RenderContext gen_render_context(FrameBuffer *framebuffer, Texture *texture,
	Shader *shader, const Vector &position, int slot,
	float rotation, float width, float height,
	const Color &color,
	bool is_ui, int z_order) {
	RenderContext result = {};

	result.framebuffer = framebuffer;
	result.render_type = RT_TEXTURE;
	result.texture = texture;
	result.mesh = nullptr;
	result.shader = shader;
	result.position = position;
	result.z_order = z_order;
	result.color = color;
	result.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	result.rotation = Vector(0.0f, 0.0f, rotation);
	result.width = width;
	result.height = height;
	result.pixel_per_unit = 1.0f;
	result.blend_source = GL_SRC_ALPHA;
	result.blend_destination = GL_ONE_MINUS_SRC_ALPHA;
	result.slot = slot;
	result.raw_texture = 0;
	result.is_ui = is_ui;
	result.cullable = !is_ui;
	result.shape_type = SHAPE_RECT;

	return result;
}

RenderContext gen_clear_render_context(FrameBuffer *framebuffer) {
	RenderContext result = {};

	result.render_type = RT_CLEAR;
	result.framebuffer = framebuffer;
	result.blend_source = GL_SRC_ALPHA;
	result.blend_destination = GL_ONE_MINUS_SRC_ALPHA;

	return result;
}

RenderContext gen_shape_render_context(FrameBuffer *framebuffer,
	ShapeType shape_type,
	const Vector &position, float rotation,
	float width, float height,
	const Color &color,
	bool is_ui, int z_order) {
	RenderContext result = {};

	result.render_type = RT_SHAPE;
	result.framebuffer = framebuffer;
	result.position = position;
	result.z_order = z_order;
	result.rotation = Vector(0.0f, 0.0f, rotation);
	result.width = width;
	result.height = height;
	result.pixel_per_unit = 1.0f;
	result.color = color;
	result.blend_source = GL_SRC_ALPHA;
	result.blend_destination = GL_ONE_MINUS_SRC_ALPHA;
	result.is_ui = is_ui;
	result.shape_type = shape_type;

	return result;
}

RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position,
	float rotation, float width,
	float height, const Color &color,
	bool is_ui, int z_order) {
	return gen_mesh_render_context(framebuffer, mesh, shader, position,
		rotation, Vector(width, height, 1.0f), color, is_ui, z_order);
}

RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position, float rotation, const Vector &scale,
	const Color &color, bool is_ui, int z_order) {
	return gen_mesh_render_context(framebuffer, mesh, shader, position,
		Vector(0.0f, 0.0f, rotation), scale, color, is_ui, z_order);
}

RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position, const Vector &rotation, const Vector &scale,
	const Color &color, bool is_ui, int z_order) {
	RenderContext result = {};

	result.render_type = RT_MESH;
	result.framebuffer = framebuffer;
	result.mesh = mesh;
	result.shader = shader;
	result.position = position;
	result.scale = scale;
	result.z_order = z_order;
	result.rotation = rotation;
	result.width = scale.x;
	result.height = scale.y;
	result.pixel_per_unit = 1.0f;
	result.color = color;
	result.blend_source = GL_SRC_ALPHA;
	result.blend_destination = GL_ONE_MINUS_SRC_ALPHA;
	result.is_ui = is_ui;

	return result;
}

RenderContext gen_framebuffer_render_context(FrameBuffer *target_framebuffer,
	FrameBuffer *source_framebuffer,
	const Vector &position,
	float rotation, float width,
	float height,
	const Color &color,
	bool is_ui, int z_order) {
	RenderContext result = {};

	result.render_type = RT_FRAMEBUFFER;
	result.framebuffer = target_framebuffer;
	result.position = position;
	result.z_order = z_order;
	result.rotation = Vector(0.0f, 0.0f, rotation);
	result.width = width;
	result.height = height;
	result.source_width = source_framebuffer == nullptr ? 0.0f : source_framebuffer->get_width();
	result.source_height = source_framebuffer == nullptr ? 0.0f : source_framebuffer->get_height();
	result.pixel_output_width = source_framebuffer == nullptr ? width : source_framebuffer->get_pixel_view_width();
	result.pixel_output_height = source_framebuffer == nullptr ? height : source_framebuffer->get_pixel_view_height();
	result.pixel_per_unit = source_framebuffer == nullptr ? 1.0f : source_framebuffer->get_pixel_per_unit();
	result.color = color;
	result.blend_source = GL_SRC_ALPHA;
	result.blend_destination = GL_ONE_MINUS_SRC_ALPHA;
	result.is_ui = is_ui;
	result.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	result.raw_texture =
		source_framebuffer == nullptr ? 0 : source_framebuffer->get_color_texture();

	return result;
}
