#pragma once

#include "color.h"
#include "render_context.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

class Vector;
class Texture;
class Mesh;
class Engine;
class FrameBuffer;
class Shader;
class Camera;
class TTFont;
class SpriteBatch;
struct SpriteInstance;

class Renderer {
public:
	Camera *get_camera();

	Renderer(Engine *engine, Camera *camera, Shader *shader);
	~Renderer();

	void render_texture(Texture *texture,
		Shader *shader, const Vector &position, float width,
		float height);
	void render_texture(Texture *texture, Shader *shader,
		const Vector &position, float width, float height, float rotation,
		const Color &color, int atlas_slot = -1, bool is_ui = false, int z_order = 0);
	void render_image(Texture *texture, const Vector &position, float width,
		float height);
	void render_image(Texture *texture, const Vector &position, float width,
		float height, float rotation, int z_order = 0);
	void render_ui_image(Texture *texture, const Vector &position,
		float width, float height);
	void render_ui_image(Texture *texture, const Vector &position,
		float width, float height, const Color &color, int z_order = 0);
	void render_ui_image(Texture *texture, const Vector &position,
		float width, float height, float rotation, int z_order = 0);
	void render_rect(const Vector &position, float width, float height);
	void render_rect(const Vector &position, float width, float height,
		float rotation, int z_order = 0);
	void render_ui_rect(const Vector &position, float width, float height);
	void render_ui_rect(const Vector &position, float width, float height,
		float rotation, int z_order = 0);
	void render_oval(const Vector &position, float width, float height);
	void render_oval(const Vector &position, float width, float height,
		float rotation, int z_order = 0);
	void render_ui_oval(const Vector &position, float width, float height);
	void render_ui_oval(const Vector &position, float width, float height,
		float rotation, int z_order = 0);
	void render_framebuffer(FrameBuffer *source, const Vector &position,
		float width, float height);
	void render_framebuffer(FrameBuffer *source, const Vector &position,
		float width, float height, float rotation, int z_order = 0);
	void render_ui_framebuffer(FrameBuffer *source, const Vector &position,
		float width, float height);
	void render_ui_framebuffer(FrameBuffer *source, const Vector &position,
		float width, float height, float rotation, int z_order = 0);
	void render_framebuffer_part(FrameBuffer *source, const Vector &position,
		float width, float height, const glm::vec4 &uv,
		bool is_ui = false, int z_order = 0);
	void render_framebuffer_part(FrameBuffer *source, const Vector &position,
		float width, float height, float rotation, const glm::vec4 &uv,
		bool is_ui = false, int z_order = 0);
	void render_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		float width, float height);
	void render_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		float width, float height, float rotation, int z_order = 0);
	void render_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		const Vector &scale, float rotation = 0.0f, int z_order = 0);
	void render_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		const Vector &scale, const Vector &rotation, int z_order = 0);
	void render_ui_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		float width, float height);
	void render_ui_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		float width, float height, float rotation, int z_order = 0);
	void render_ui_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		const Vector &scale, float rotation = 0.0f, int z_order = 0);
	void render_ui_mesh(Mesh *mesh, Shader *shader, const Vector &position,
		const Vector &scale, const Vector &rotation, int z_order = 0);
	void render_font(TTFont *font, const std::string &text,
		const Vector &position, const std::string &align = "center", int z_order = 0);
	void render_font(TTFont *font, const std::string &text,
		const Vector &position, const std::string &align,
		const Color &outline_color, float outline_width, int z_order = 0);
	void render_font(TTFont *font, const std::string &text,
		const Vector &position, float rotation, int z_order = 0);
	void render_font(TTFont *font, const std::string &text,
		const Vector &position, float rotation, const std::string &align,
		const Color &outline_color, float outline_width, int z_order = 0);
	void render_ui_font(TTFont *font, const std::string &text,
		const Vector &position, const std::string &align = "center", int z_order = 0);
	void render_ui_font(TTFont *font, const std::string &text,
		const Vector &position, const std::string &align,
		const Color &outline_color, float outline_width, int z_order = 0);
	void render_ui_font(TTFont *font, const std::string &text,
		const Vector &position, float rotation, const std::string &align = "center",
		int z_order = 0);
	void render_ui_font(TTFont *font, const std::string &text,
		const Vector &position, float rotation, const std::string &align,
		const Color &outline_color, float outline_width, int z_order = 0);

	// UI masks are captured into queued draw commands. Nested masks use the
	// intersection of their rectangles.
	void push_ui_mask(const Vector &position, float width, float height);
	void pop_ui_mask();
	void clear_ui_masks();
	bool has_ui_mask() const;

	void begin_frame();
	void apply_pixel_perfect_camera(FrameBuffer *framebuffer);
	void clear();
	void clear(float r, float g, float b, float a = 1.0f);
	void clear(const Color &color);
	void set_color(const Color &color);
	Color get_color() const;

	Shader *get_shader();
	void set_shader(Shader *shader);
	void set_sprite_matrices(const glm::mat4 &view,
		const glm::mat4 &projection);
	const glm::mat4 &get_sprite_view() const;
	const glm::mat4 &get_sprite_projection() const;
	Texture *get_white_texture();
	void draw_shape(const class RenderContext &rc, const glm::mat4 &vp);
	void draw_raw_texture(unsigned texture_id, const class RenderContext &rc,
		const glm::mat4 &vp);
	void draw_mesh(const class RenderContext &rc, const glm::mat4 &vp);

private:
	friend class Scene;

	void init_quad();
	void draw_sprite_batch(Texture *texture, const SpriteInstance *instances, int count);
	void queue_shape(ShapeType shape_type, const Vector &position,
		float width, float height, float rotation, bool is_ui, int z_order = 0);
	void queue_font(TTFont *font, const std::string &text,
		const Vector &position, const std::string &align,
		const Color &outline_color, float outline_width, float rotation, bool is_ui,
		int z_order);
	void apply_ui_mask(RenderContext &context, bool is_ui) const;
	void upload_ui_mask(Shader *shader, const RenderContext &context) const;

	Engine *engine;
	Shader *shader;
	Shader *default_shader;
	Texture *white_texture = nullptr;
	std::unique_ptr<SpriteBatch> sprite_batch;
	Camera *camera;
	Color color = Color::white();
	glm::mat4 sprite_view{ 1.0f };
	glm::mat4 sprite_projection{ 1.0f };
	unsigned quad_vao = 0;
	unsigned quad_vbo = 0;
	unsigned quad_ebo = 0;
	std::vector<glm::vec4> ui_mask_stack;
};
