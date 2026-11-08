#include "engine.h"

#include "camera.h"
#include "display.h"
#include "error.h"
#include "framebuffer.h"
#include "input.h"
#include "renderer.h"
#include "render_context.h"
#include "scene.h"
#include "shader.h"
#include "shader_manager.h"

#include <algorithm>
#include <utility>

Engine::~Engine() {
	shutdown();
}

void Engine::shutdown() {
	after_scene_update = {};
	current_scene = nullptr;
	current_framebuffer = nullptr;
	lights.clear();
	delete renderer;
	renderer = nullptr;
	delete shader_manager;
	shader_manager = nullptr;
	delete input;
	input = nullptr;
	delete camera;
	camera = nullptr;
	if (display != nullptr) {
		delete display;
		display = nullptr;
	} else if (sdl_initialized)
		SDL_Quit();
	sdl_initialized = false;
	width = 0;
	height = 0;
	cached_frames = 0;
	cached_ticks = 0;
	loop_started = false;
	previous_frame = 0;
	update_lag = 0.0;
	fps_timer = 0.0;
	frame_count = 0;
	tick_count = 0;
}

void Engine::init_window(const std::string &title, int width, int height) {
	if (width <= 0 || height <= 0 || title.find('\0') != std::string::npos)
		error("Invalid window settings.");
	if (display != nullptr)
		error("An engine window is already open.");
	if (!sdl_initialized)
		init_engine();
	try {
		camera = new Camera(width, height);
		input = new Input();
		display = new Display(title, width, height);
		display->set_camera(camera);
		display->update_viewport(width, height);

		shader_manager = new ShaderManager();
		shader_manager->fetch_shader(
			"sprite", new Shader("engine_shaders/sprite.vert", "engine_shaders/sprite.frag"));
		shader_manager->fetch_shader(
			"default", new Shader("engine_shaders/default.vert", "engine_shaders/default.frag"));
		shader_manager->fetch_shader(
			"shape", new Shader("engine_shaders/shape.vert", "engine_shaders/shape.frag"));
		shader_manager->fetch_shader(
			"quad",
			new Shader("engine_shaders/framebuffer.vert", "engine_shaders/framebuffer.frag"));

		shader_manager->fetch_shader(
			"model", new Shader("engine_shaders/model.vert", "engine_shaders/model.frag"));
		renderer = new Renderer(this, camera, shader_manager->get_shader("sprite"));
		this->width = static_cast<float>(width);
		this->height = static_cast<float>(height);
	} catch (...) {
		shutdown();
		throw;
	}
}

void Engine::init_engine() {
	shutdown();
	try {
		if (!SDL_Init(SDL_INIT_VIDEO))
			error(std::string("SDL_Init Error: ") + SDL_GetError());
		sdl_initialized = true;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	} catch (...) {
		shutdown();
		throw;
	}
}

float Engine::get_width() {
	return width;
}

float Engine::get_height() {
	return height;
}

Input *Engine::get_input() {
	return input;
}

Light *Engine::add_light(const Light &light) {
	return lights.add(light);
}

bool Engine::remove_light(const Light *light) {
	return lights.remove(light);
}

void Engine::clear_lights() {
	lights.clear();
}

int Engine::get_light_count() const {
	return lights.get_count();
}

void Engine::upload_lights(Shader *shader) const {
	lights.upload(shader);
}

Scene *Engine::get_current_scene() {
	return current_scene;
}

void Engine::set_scene(Scene *scene) {
	// A scene becomes active immediately and gets its init hook before the
	// first frame so it can allocate resources and framebuffers.
	this->current_scene = scene;
	scene->init();
}

void Engine::set_after_scene_update(std::function<void(Engine *)> callback) {
	after_scene_update = std::move(callback);
}

FrameBuffer *Engine::get_framebuffer() {
	return current_framebuffer;
}

void Engine::set_framebuffer(FrameBuffer *framebuffer) {
	this->current_framebuffer = framebuffer;
}

void Engine::set_automatic_presentation(bool enabled) {
	automatic_presentation = enabled;
}

bool Engine::is_automatic_presentation() const {
	return automatic_presentation;
}

int Engine::get_frames() {
	return cached_frames;
}

int Engine::get_ticks() {
	return cached_ticks;
}

void Engine::start() {
	while (run_frame()) {
	}
}

bool Engine::run_frame() {
	if (display == nullptr || renderer == nullptr || input == nullptr)
		return false;
	if (!loop_started) {
		previous_frame = SDL_GetPerformanceCounter();
		loop_started = true;
		std::cout << "Starting engine... (Powered by Tonic.)" << std::endl;
	}

	renderer->begin_frame();
	if (current_scene != nullptr)
		current_scene->start_frame();
	input->begin_new_frame();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		input->process_event(event, this);
		switch (event.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			return false;
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			display->update_viewport(event.window.data1, event.window.data2);
			input->refresh_mouse_position(this);
			break;
		}
	}

	const Uint64 current = SDL_GetPerformanceCounter();
	const double elapsed = static_cast<double>(current - previous_frame) / SDL_GetPerformanceFrequency();
	previous_frame = current;
	// Shell commands can take time; avoid an unbounded catch-up after they finish.
	update_lag += std::min(elapsed, 0.25);
	fps_timer += elapsed;
	constexpr double TIME_STEP = 1.0 / 60.0;
	while (update_lag >= TIME_STEP) {
		if (current_scene != nullptr)
			current_scene->update();
		if (after_scene_update)
			after_scene_update(this);
		input->end_update();
		++tick_count;
		update_lag -= TIME_STEP;
	}

	FrameBuffer::resize_camera_sized_framebuffers(camera);
	FrameBuffer::bind_screen_framebuffer();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	current_framebuffer = nullptr;
	if (current_scene != nullptr) {
		current_scene->render();
		current_scene->flush_render_context();
	}
	current_framebuffer = nullptr;
	FrameBuffer::bind_screen_framebuffer();
	SDL_GL_SwapWindow(display->get_window());
	++frame_count;
	if (fps_timer >= 1.0) {
		cached_frames = frame_count;
		cached_ticks = tick_count;
		frame_count = 0;
		tick_count = 0;
		fps_timer = 0.0;
	}
	return true;
}

ShaderManager *Engine::get_shader_manager() {
	return this->shader_manager;
}

Renderer *Engine::get_renderer() {
	return this->renderer;
}

Camera *Engine::get_camera() {
	return this->camera;
}

Display *Engine::get_display() {
	return this->display;
}
