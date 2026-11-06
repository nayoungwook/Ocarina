#pragma once

class Camera;

class FrameBuffer {
public:
	enum class SizeMode { RENDER, CAMERA, FIXED };

	FrameBuffer();
	explicit FrameBuffer(bool pixel_perfect);
	explicit FrameBuffer(SizeMode size_mode);
	FrameBuffer(SizeMode size_mode, bool pixel_perfect);
	FrameBuffer(int width, int height);
	FrameBuffer(int width, int height, bool pixel_perfect);
	~FrameBuffer();

	void bind();
	void apply_viewport() const;
	void unbind();
	static void bind_screen_framebuffer();
	static void invalidate_bind_cache();
	static FrameBuffer *get_bound_framebuffer();

	void clear(float r = 0.0f, float g = 0.0f, float b = 0.0f,
		float a = 1.0f);
	void resize(int width, int height);
	void dispose();

	unsigned get_color_texture() const;
	unsigned get_draw_color_texture() const;
	unsigned get_fbo() const;
	int get_width() const;
	int get_height() const;
	float get_resolution_x() const;
	float get_resolution_y() const;
	SizeMode get_size_mode() const;
	bool is_pixel_perfect() const;
	FrameBuffer &set_pixel_perfect(bool enabled);
	void set_pixel_per_unit(float pixels, float world_size);
	float get_pixel_per_unit() const;
	int get_pixel_view_width() const;
	int get_pixel_view_height() const;
	bool has_resolve_target() const;
	void bind_resolve_target();

	void bind_texture(int texture_unit) const;
	void bind_draw_texture(int texture_unit) const;
	void upload_texture(int texture_unit) const;

	static void resize_camera_sized_framebuffers();
	static void resize_camera_sized_framebuffers(Camera *camera);

	void create(int width, int height);

private:
	void initialize();
	void allocate_storage();
	void allocate_resolve_storage();
	void resize_resolve_target(int width, int height);
	void update_resolution_from_mode(Camera *camera = nullptr);
	void register_dynamic();
	void unregister_dynamic();

	unsigned fbo = 0;
	unsigned color_texture = 0;
	unsigned rbo = 0;
	unsigned resolve_fbo = 0;
	unsigned resolve_color_texture = 0;
	unsigned resolve_rbo = 0;
	int resolve_width = 1;
	int resolve_height = 1;
	int width = 1;
	int height = 1;
	int pixel_view_width = 1;
	int pixel_view_height = 1;
	bool disposed = false;
	bool pixel_perfect = false;
	SizeMode size_mode = SizeMode::FIXED;
	bool dynamic_registered = false;
	float pixel_per_unit = 1.0f;
};
