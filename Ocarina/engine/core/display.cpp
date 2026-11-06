#include <display.h>

#include <camera.h>
#include <error.h>
#include <framebuffer.h>

#include <algorithm>

static Display* current_display = nullptr;

Display::Display(const std::string& title, int width, int height)
	: title(title), width(std::max(1, width)), height(std::max(1, height)) {
	try {
		// Display owns the SDL window and GL context, then exposes the drawable
		// viewport that framebuffer and screen-quad rendering must respect.
		window = SDL_CreateWindow(title.c_str(), this->width, this->height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		if (window == nullptr)
			error(std::string("Window creation failed: ") + SDL_GetError());
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		gl_context = SDL_GL_CreateContext(window);
		if (gl_context == nullptr)
			error(std::string("OpenGL context creation failed: ") + SDL_GetError());
		SDL_GL_SetSwapInterval(1); // V-sync on.
		glewExperimental = GL_TRUE;
		if (glewInit() != GLEW_OK)
			error("GLEW initialization failed");
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		current_display = this;
		update_viewport(this->width, this->height);
	} catch (...) {
		if (current_display == this) current_display = nullptr;
		if (gl_context != nullptr) SDL_GL_DestroyContext(gl_context);
		if (window != nullptr) SDL_DestroyWindow(window);
		throw;
	}
}

Display::~Display() {
	if (current_display == this)
		current_display = nullptr;
	if (gl_context != nullptr)
		SDL_GL_DestroyContext(gl_context);
	if (window != nullptr)
		SDL_DestroyWindow(window);
	SDL_Quit();
}

void Display::set_camera(Camera* value) {
	camera = value;
	update_viewport();
}

Camera* Display::get_camera() const { return camera; }

void Display::update_viewport(int window_width, int window_height) {
	if (window != nullptr) {
		int queried_width = 0;
		int queried_height = 0;
		SDL_GetWindowSize(window, &queried_width, &queried_height);

		if (queried_width > 0 && queried_height > 0) {
			width = queried_width;
			height = queried_height;
		}

		SDL_GetWindowSizeInPixels(window, &queried_width, &queried_height);
		drawable_width = std::max(1, queried_width);
		drawable_height = std::max(1, queried_height);
	}
	else {
		width = std::max(1, window_width);
		height = std::max(1, window_height);
		drawable_width = width;
		drawable_height = height;
	}

	calculate_viewport();
	apply_screen_viewport();
	FrameBuffer::resize_camera_sized_framebuffers(camera);
}

void Display::calculate_viewport() {
	viewport_x = 0;
	viewport_y = 0;
	viewport_width = width;
	viewport_height = height;
	framebuffer_viewport_x = 0;
	framebuffer_viewport_y = 0;
	framebuffer_viewport_width = drawable_width;
	framebuffer_viewport_height = drawable_height;

	if (camera != nullptr)
		camera->fit_projection_to_display(drawable_width, drawable_height);
}

void Display::apply_screen_viewport() const {
	glViewport(framebuffer_viewport_x, framebuffer_viewport_y,
		framebuffer_viewport_width, framebuffer_viewport_height);
	FrameBuffer::invalidate_bind_cache();
}

void Display::apply_current_screen_viewport() {
	if (current_display != nullptr)
		current_display->apply_screen_viewport();
}

Display* Display::get_current() { return current_display; }

void Display::set_fullscreen() {
	SDL_SetWindowFullscreen(window, true);
	update_viewport();
}
void Display::set_windowed(int new_width, int new_height) {
	SDL_SetWindowFullscreen(window, false);
	SDL_SetWindowSize(window, std::max(1, new_width), std::max(1, new_height));
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	update_viewport();
}
void Display::start() {}
SDL_Window* Display::get_window() { return window; }
std::string Display::get_title() const { return title; }
int Display::get_width() const { return width; }
int Display::get_height() const { return height; }
int Display::get_drawable_width() const { return drawable_width; }
int Display::get_drawable_height() const { return drawable_height; }
int Display::get_viewport_x() const { return viewport_x; }
int Display::get_viewport_y() const { return viewport_y; }
int Display::get_viewport_width() const { return viewport_width; }
int Display::get_viewport_height() const { return viewport_height; }
int Display::get_framebuffer_viewport_x() const { return framebuffer_viewport_x; }
int Display::get_framebuffer_viewport_y() const { return framebuffer_viewport_y; }
int Display::get_framebuffer_viewport_width() const { return framebuffer_viewport_width; }
int Display::get_framebuffer_viewport_height() const { return framebuffer_viewport_height; }
