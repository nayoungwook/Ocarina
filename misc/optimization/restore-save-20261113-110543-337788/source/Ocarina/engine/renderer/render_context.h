#pragma once

#include "color.h"
#include <glm/glm.hpp>
#include "mathf.h"
#include <string>
#include <unordered_map>
#include <memory>

class Vector;
class Shader;
class Texture;
class FrameBuffer;
class Mesh;

struct UniformValue {
	enum Type {
		Float,
		Int,
		Vec2,
		Vec3,
		Vec4,
		Mat4,
	};

	UniformValue() : type(Float), float_value(0.0f), int_value(0) {}
	UniformValue(float value)
		: type(Float), float_value(value), int_value(0) {}
	UniformValue(int value)
		: type(Int), float_value(0.0f), int_value(value) {}
	UniformValue(const glm::vec2 &value)
		: type(Vec2), float_value(0.0f), int_value(0), vec2_value(value) {}
	UniformValue(const glm::vec3 &value)
		: type(Vec3), float_value(0.0f), int_value(0), vec3_value(value) {}
	UniformValue(const glm::vec4 &value)
		: type(Vec4), float_value(0.0f), int_value(0), vec4_value(value) {}
	UniformValue(const glm::mat4 &value)
		: type(Mat4), float_value(0.0f), int_value(0), mat4_value(value) {}

	Type type;
	float float_value;
	int int_value;
	glm::vec2 vec2_value;
	glm::vec3 vec3_value;
	glm::vec4 vec4_value;
	glm::mat4 mat4_value;
};
using UniformMap = std::unordered_map<std::string, UniformValue>;

// Keep uncommon custom uniforms outside the common render command.
struct UniformState {
	UniformMap values;
	UniformMap reset_values;
};

enum RenderType {
	RT_TEXTURE = 0,
	RT_CLEAR = 1,
	RT_SHAPE = 2,
	RT_FRAMEBUFFER = 3,
	RT_MESH = 4,
};

enum ShapeType {
	SHAPE_RECT = 0,
	SHAPE_OVAL = 1,
};

class RenderContext {
public:
	enum RenderType render_type;
	FrameBuffer *framebuffer;
	Texture *texture;
	Mesh *mesh;
	Shader *shader;
	Vector position;
	Vector scale = Vector(1.0f, 1.0f, 1.0f);

	int z_order;

	Color color;
	glm::vec4 uv;

	int slot;
	unsigned raw_texture;
	float width, height;
	float source_width, source_height;
	float pixel_output_width, pixel_output_height;
	Vector rotation = Vector(0.0f, 0.0f, 0.0f); // XYZ angles in radians.
	int blend_source;
	int blend_destination;
	bool is_ui;
	ShapeType shape_type;
	bool mask_enabled;
	glm::vec4 mask_rect;

	bool cullable;
	bool is_pixel_perfect_framebuffer;
	float pixel_per_unit;
	std::shared_ptr<UniformState> uniform_state;
};

RenderContext gen_render_context(FrameBuffer *framebuffer, Texture *texture,
	Shader *shader, const Vector &position, int slot,
	float rotation, float width, float height,
	const Color &color = Color::white(),
	bool is_ui = false, int z_order = 0);
RenderContext gen_clear_render_context(FrameBuffer *framebuffer);
RenderContext gen_shape_render_context(FrameBuffer *framebuffer,
	ShapeType shape_type, const Vector &position, float rotation,
	float width, float height, const Color &color,
	bool is_ui = false, int z_order = 0);
RenderContext gen_framebuffer_render_context(FrameBuffer *target_framebuffer,
	FrameBuffer *source_framebuffer, const Vector &position, float rotation,
	float width, float height, const Color &color,
	bool is_ui = false, int z_order = 0);
RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position, float rotation, float width,
	float height, const Color &color, bool is_ui = false, int z_order = 0);
RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position, float rotation, const Vector &scale,
	const Color &color, bool is_ui = false, int z_order = 0);
RenderContext gen_mesh_render_context(FrameBuffer *framebuffer, Mesh *mesh,
	Shader *shader, const Vector &position, const Vector &rotation, const Vector &scale,
	const Color &color, bool is_ui = false, int z_order = 0);
