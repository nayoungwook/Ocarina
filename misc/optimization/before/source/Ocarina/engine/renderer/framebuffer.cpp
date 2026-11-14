#include "framebuffer.h"

#include "camera.h"
#include "display.h"
#include "error.h"

#include <GL/glew.h>

#include <algorithm>
#include <cmath>
#include <vector>

static std::vector<FrameBuffer *> dynamic_framebuffers;
static FrameBuffer *bound_framebuffer = nullptr;
static int cached_viewport_x = -1;
static int cached_viewport_y = -1;
static int cached_viewport_width = -1;
static int cached_viewport_height = -1;

static void bind_fbo(unsigned fbo) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

static void set_viewport(int x, int y, int width, int height) {
	width = std::max(1, width);
	height = std::max(1, height);
	if (cached_viewport_x == x && cached_viewport_y == y &&
		cached_viewport_width == width && cached_viewport_height == height)
		return;
	glViewport(x, y, width, height);
	cached_viewport_x = x;
	cached_viewport_y = y;
	cached_viewport_width = width;
	cached_viewport_height = height;
}

FrameBuffer::FrameBuffer() : FrameBuffer(SizeMode::RENDER, false) {}
FrameBuffer::FrameBuffer(bool pixel_perfect)
	: FrameBuffer(SizeMode::RENDER, pixel_perfect) {}

FrameBuffer::FrameBuffer(SizeMode mode) : FrameBuffer(mode, false) {}
FrameBuffer::FrameBuffer(SizeMode mode, bool pixel_perfect)
	: pixel_perfect(pixel_perfect), size_mode(mode) {
	if (mode == SizeMode::FIXED) {
		error("Use FrameBuffer(width, height) for a fixed framebuffer");
		size_mode = SizeMode::RENDER;
	}
	update_resolution_from_mode();
	initialize();
	register_dynamic();
}

FrameBuffer::FrameBuffer(int width, int height) : FrameBuffer(width, height, false) {}
FrameBuffer::FrameBuffer(int width, int height, bool pixel_perfect)
	: width(std::max(1, width)), height(std::max(1, height)),
	pixel_perfect(pixel_perfect), size_mode(SizeMode::FIXED) {
	pixel_view_width = this->width;
	pixel_view_height = this->height;
	initialize();
}

FrameBuffer::~FrameBuffer() {
	unregister_dynamic();
	dispose();
}

void FrameBuffer::initialize() {
	// A framebuffer owns a color texture and depth/stencil renderbuffer.
	disposed = false;
	glGenFramebuffers(1, &fbo);
	glGenTextures(1, &color_texture);
	glGenRenderbuffers(1, &rbo);
	bind_fbo(fbo);
	bound_framebuffer = this;
	allocate_storage();
	if (pixel_perfect) {
		resolve_width = pixel_view_width;
		resolve_height = pixel_view_height;
		glGenFramebuffers(1, &resolve_fbo);
		glGenTextures(1, &resolve_color_texture);
		glGenRenderbuffers(1, &resolve_rbo);
		allocate_resolve_storage();
	}
	bind_screen_framebuffer();
}

void FrameBuffer::allocate_storage() {
	glBindTexture(GL_TEXTURE_2D, color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, color_texture, 0);
	const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &draw_buffer);

	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
		GL_RENDERBUFFER, rbo);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		error("Framebuffer is not complete");
}

void FrameBuffer::allocate_resolve_storage() {
	bind_fbo(resolve_fbo);
	glBindTexture(GL_TEXTURE_2D, resolve_color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, resolve_width, resolve_height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, resolve_color_texture, 0);
	const GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &draw_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, resolve_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
		resolve_width, resolve_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
		GL_RENDERBUFFER, resolve_rbo);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		error("Framebuffer resolve target is not complete");
}

void FrameBuffer::bind() {
	bind_fbo(fbo);
	bound_framebuffer = this;
	set_viewport(0, 0, width, height);
}

void FrameBuffer::bind_resolve_target() {
	bind_fbo(resolve_fbo);
	bound_framebuffer = nullptr;
	set_viewport(0, 0, resolve_width, resolve_height);
}

void FrameBuffer::apply_viewport() const {
	set_viewport(0, 0, width, height);
}

void FrameBuffer::unbind() { bind_screen_framebuffer(); }

void FrameBuffer::bind_screen_framebuffer() {
	bind_fbo(0);
	bound_framebuffer = nullptr;

	Display *display = Display::get_current();

	if (display != nullptr) {
		set_viewport(display->get_framebuffer_viewport_x(),
			display->get_framebuffer_viewport_y(),
			display->get_framebuffer_viewport_width(),
			display->get_framebuffer_viewport_height());
	}
}

void FrameBuffer::invalidate_bind_cache() {
	cached_viewport_x = cached_viewport_y = -1;
	cached_viewport_width = cached_viewport_height = -1;
}

FrameBuffer *FrameBuffer::get_bound_framebuffer() { return bound_framebuffer; }

void FrameBuffer::clear(float r, float g, float b, float a) {
	FrameBuffer *previous = bound_framebuffer;
	bind();
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (previous != nullptr)
		previous->bind();
	else
		bind_screen_framebuffer();
}

void FrameBuffer::resize(int new_width, int new_height) {
	if (disposed)
		return;
	new_width = std::max(1, new_width);
	new_height = std::max(1, new_height);
	if (new_width == width && new_height == height)
		return;
	width = new_width;
	height = new_height;
	FrameBuffer *previous = bound_framebuffer;
	bind_fbo(fbo);
	allocate_storage();
	if (previous != nullptr)
		previous->bind();
	else
		bind_screen_framebuffer();
	if (pixel_perfect)
		resize_resolve_target(pixel_view_width, pixel_view_height);
}

void FrameBuffer::resize_resolve_target(int new_width, int new_height) {
	if (!pixel_perfect || (resolve_width == new_width && resolve_height == new_height))
		return;
	resolve_width = std::max(1, new_width);
	resolve_height = std::max(1, new_height);
	FrameBuffer *previous = bound_framebuffer;
	allocate_resolve_storage();
	if (previous != nullptr)
		previous->bind();
	else
		bind_screen_framebuffer();
}

void FrameBuffer::create(int new_width, int new_height) {
	resize(new_width, new_height);
}

void FrameBuffer::dispose() {
	if (disposed)
		return;
	unregister_dynamic();
	if (bound_framebuffer == this)
		bind_screen_framebuffer();
	if (fbo != 0)
		glDeleteFramebuffers(1, &fbo);
	if (rbo != 0)
		glDeleteRenderbuffers(1, &rbo);
	if (color_texture != 0)
		glDeleteTextures(1, &color_texture);
	if (resolve_fbo != 0)
		glDeleteFramebuffers(1, &resolve_fbo);
	if (resolve_rbo != 0)
		glDeleteRenderbuffers(1, &resolve_rbo);
	if (resolve_color_texture != 0)
		glDeleteTextures(1, &resolve_color_texture);
	fbo = rbo = color_texture = 0;
	disposed = true;
}

void FrameBuffer::bind_texture(int texture_unit) const {
	glActiveTexture(GL_TEXTURE0 + texture_unit);
	glBindTexture(GL_TEXTURE_2D, get_color_texture());
}
void FrameBuffer::bind_draw_texture(int texture_unit) const {
	glActiveTexture(GL_TEXTURE0 + texture_unit);
	glBindTexture(GL_TEXTURE_2D, color_texture);
}
void FrameBuffer::upload_texture(int texture_unit) const {
	bind_texture(texture_unit);
}

void FrameBuffer::update_resolution_from_mode(Camera *camera) {
	// CAMERA targets match the camera projection; RENDER targets match the
	// camera render resolution.
	if (size_mode == SizeMode::FIXED)
		return;
	if (camera == nullptr) {
		Display *display = Display::get_current();
		if (display != nullptr)
			camera = display->get_camera();
	}
	int target_width = width;
	int target_height = height;
	if (camera != nullptr) {
		target_width = std::max(1, camera->get_resolution_x());
		target_height = std::max(1, camera->get_resolution_y());
	}
	else {
		Display *display = Display::get_current();
		if (display != nullptr) {
			target_width = display->get_drawable_width();
			target_height = display->get_drawable_height();
		}
	}

	pixel_view_width = target_width;
	pixel_view_height = target_height;
	if (pixel_perfect && camera != nullptr) {
		const float zoom = std::max(0.0001f, std::abs(camera->zoom));
		const float cosine = std::abs(std::cos(camera->rotation));
		const float sine = std::abs(std::sin(camera->rotation));
		const float world_units_per_pixel = std::max(0.0001f, pixel_per_unit);
		const float source_target_width =
			std::ceil(target_width / world_units_per_pixel);
		const float source_target_height =
			std::ceil(target_height / world_units_per_pixel);
		const float visible_width =
			(cosine * source_target_width + sine * source_target_height) / zoom;
		const float visible_height =
			(sine * source_target_width + cosine * source_target_height) / zoom;
		// The source pass has one texel per configured world pixel. The resolve
		// target can be larger; it scales those texels to the logical output.
		// Extra source space is needed only for zooming out or rotating.
		width = std::max(1, static_cast<int>(std::ceil(std::max(
			source_target_width, visible_width))));
		height = std::max(1, static_cast<int>(std::ceil(std::max(
			source_target_height, visible_height))));
		// Keep the world origin on a texel boundary. An odd source dimension puts
		// it on a texel centre, shifting even-sized pixel sprites by half a texel.
		if ((width & 1) != 0) ++width;
		if ((height & 1) != 0) ++height;
	}
	else {
		width = target_width;
		height = target_height;
	}
}

void FrameBuffer::register_dynamic() {
	if (size_mode == SizeMode::FIXED || dynamic_registered)
		return;
	dynamic_framebuffers.push_back(this);
	dynamic_registered = true;
}
void FrameBuffer::unregister_dynamic() {
	if (!dynamic_registered)
		return;
	auto it = std::find(dynamic_framebuffers.begin(), dynamic_framebuffers.end(), this);
	if (it != dynamic_framebuffers.end())
		dynamic_framebuffers.erase(it);
	dynamic_registered = false;
}

void FrameBuffer::resize_camera_sized_framebuffers() {
	resize_camera_sized_framebuffers(nullptr);
}
void FrameBuffer::resize_camera_sized_framebuffers(Camera *camera) {
	// Resize every dynamic framebuffer after display or camera changes.
	for (FrameBuffer *framebuffer : dynamic_framebuffers) {
		if (framebuffer == nullptr)
			continue;
		const int old_width = framebuffer->width;
		const int old_height = framebuffer->height;
		framebuffer->update_resolution_from_mode(camera);
		int new_width = framebuffer->width;
		int new_height = framebuffer->height;
		framebuffer->width = old_width;
		framebuffer->height = old_height;
		framebuffer->resize(new_width, new_height);
		framebuffer->resize_resolve_target(framebuffer->pixel_view_width,
			framebuffer->pixel_view_height);
	}
}

unsigned FrameBuffer::get_color_texture() const {
	return resolve_color_texture != 0 ? resolve_color_texture : color_texture;
}
unsigned FrameBuffer::get_draw_color_texture() const { return color_texture; }
unsigned FrameBuffer::get_fbo() const { return fbo; }
int FrameBuffer::get_width() const { return width; }
int FrameBuffer::get_height() const { return height; }
float FrameBuffer::get_resolution_x() const { return static_cast<float>(width); }
float FrameBuffer::get_resolution_y() const { return static_cast<float>(height); }
FrameBuffer::SizeMode FrameBuffer::get_size_mode() const { return size_mode; }
bool FrameBuffer::is_pixel_perfect() const { return pixel_perfect; }
FrameBuffer &FrameBuffer::set_pixel_perfect(bool enabled) {
	if (pixel_perfect == enabled)
		return *this;
	pixel_perfect = enabled;
	if (enabled) {
		resolve_width = pixel_view_width = std::max(1, pixel_view_width);
		resolve_height = pixel_view_height = std::max(1, pixel_view_height);
		glGenFramebuffers(1, &resolve_fbo);
		glGenTextures(1, &resolve_color_texture);
		glGenRenderbuffers(1, &resolve_rbo);
		allocate_resolve_storage();
	}
	else {
		if (resolve_fbo != 0) glDeleteFramebuffers(1, &resolve_fbo);
		if (resolve_rbo != 0) glDeleteRenderbuffers(1, &resolve_rbo);
		if (resolve_color_texture != 0) glDeleteTextures(1, &resolve_color_texture);
		resolve_fbo = resolve_rbo = resolve_color_texture = 0;
	}
	bind();
	return *this;
}
void FrameBuffer::set_pixel_per_unit(float pixels, float world_size) {
	if (pixels <= 0.0f || world_size <= 0.0f)
		return;
	pixel_per_unit = world_size / pixels;
	if (pixel_perfect && size_mode != SizeMode::FIXED)
		resize_camera_sized_framebuffers();
}
float FrameBuffer::get_pixel_per_unit() const { return pixel_per_unit; }
int FrameBuffer::get_pixel_view_width() const { return pixel_view_width; }
int FrameBuffer::get_pixel_view_height() const { return pixel_view_height; }
bool FrameBuffer::has_resolve_target() const { return resolve_fbo != 0; }
