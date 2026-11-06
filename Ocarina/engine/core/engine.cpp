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

Engine::~Engine() { shutdown(); }

void Engine::shutdown() {
	after_scene_update = {};
	current_scene = nullptr;
	current_framebuffer = nullptr;
	lights.clear();
	delete renderer; renderer = nullptr;
	delete shader_manager; shader_manager = nullptr;
	delete input; input = nullptr;
	delete camera; camera = nullptr;
	if (display != nullptr) { delete display; display = nullptr; }
	else if (sdl_initialized) SDL_Quit();
	sdl_initialized = false;
	width = 0; height = 0;
	cached_frames = 0; cached_ticks = 0;
}

void Engine::init_engine(const std::string &title, int width, int height) {
	shutdown();
	try {
		if (!SDL_Init(SDL_INIT_VIDEO)) error(std::string("SDL_Init Error: ") + SDL_GetError());
		sdl_initialized = true;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		this->width = static_cast<float>(width);
		this->height = static_cast<float>(height);
		camera = new Camera(width, height);
		display = new Display(title, width, height);
		display->set_camera(camera);
		display->update_viewport(width, height);
		input = new Input();
		shader_manager = new ShaderManager();
		shader_manager->fetch_shader("sprite", new Shader("engine_shaders/sprite.vert", "engine_shaders/sprite.frag"));
		shader_manager->fetch_shader("default", new Shader("engine_shaders/default.vert", "engine_shaders/default.frag"));
		shader_manager->fetch_shader("shape", new Shader("engine_shaders/shape.vert", "engine_shaders/shape.frag"));
		shader_manager->fetch_shader("quad", new Shader("engine_shaders/framebuffer.vert", "engine_shaders/framebuffer.frag"));
		shader_manager->fetch_shader("model", new Shader("engine_shaders/model.vert", "engine_shaders/model.frag"));
		renderer = new Renderer(this, camera, shader_manager->get_shader("sprite"));
	} catch (...) {
		shutdown();
		throw;
	}
}

float Engine::get_width() { return width; }

float Engine::get_height() { return height; }

Input *Engine::get_input() { return input; }

Light *Engine::add_light(const Light &light) { return lights.add(light); }

bool Engine::remove_light(const Light *light) { return lights.remove(light); }

void Engine::clear_lights() { lights.clear(); }

int Engine::get_light_count() const { return lights.get_count(); }

void Engine::upload_lights(Shader *shader) const { lights.upload(shader); }

Scene *Engine::get_current_scene() { return current_scene; }

void Engine::set_scene(Scene *scene) {
	// A scene becomes active immediately and gets its init hook before the
	// first frame so it can allocate resources and framebuffers.
	this->current_scene = scene;
	scene->init();
}

void Engine::set_after_scene_update(std::function<void(Engine *)> callback) {
	after_scene_update = std::move(callback);
}

FrameBuffer *Engine::get_framebuffer() { return current_framebuffer; }

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
	// Main loop: poll input, run fixed-step updates, render queued work, then
	// swap the SDL OpenGL window.
	bool running = true;
	Uint64 previous = SDL_GetPerformanceCounter();
	double lag = 0.0;

	const int TARGET_FPS = 60;
	const float TIME_STEP = 1.0f / TARGET_FPS;

	SDL_Event event;
	int frames = 0;
	int ticks = 0;
	double fps_timer = 0.0;

	while (running) {
		renderer->begin_frame();
		if (current_scene != nullptr) {
			current_scene->start_frame();
		}
		input->begin_new_frame();

		while (SDL_PollEvent(&event)) {
			// SDL events are folded into Input before the scene update so the
			// scene sees a consistent one-frame snapshot.

			input->process_event(event, this);

			switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;

			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				int w = event.window.data1;
				int h = event.window.data2;
				this->display->update_viewport(w, h);
				this->input->refresh_mouse_position(this);
			}
		}

		Uint64 current = SDL_GetPerformanceCounter();
		double elapsed = (double)(current - previous) /
			SDL_GetPerformanceFrequency();
		previous = current;
		lag += elapsed;
		fps_timer += elapsed;

		while (lag >= TIME_STEP) {
			// Fixed timestep keeps game logic stable even if rendering speed
			// fluctuates. Rendering still happens once per outer loop.
			if (current_scene != nullptr) {
				current_scene->update();
			}
			if (after_scene_update)
				after_scene_update(this);
			input->end_update();
			ticks++;
			lag -= TIME_STEP;
		}

		if (current_scene != nullptr) {
			FrameBuffer::resize_camera_sized_framebuffers(camera);
			FrameBuffer::bind_screen_framebuffer();
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			current_framebuffer = nullptr;
			FrameBuffer::bind_screen_framebuffer();

			current_scene->render();
			current_scene->flush_render_context();

			current_framebuffer = nullptr;
			FrameBuffer::bind_screen_framebuffer();
		}

		frames++;
		SDL_GL_SwapWindow(display->get_window());

		if (fps_timer >= 1.0) {
			cached_frames = frames;
			cached_ticks = ticks;
			frames = 0;
			ticks = 0;
			fps_timer -= 1.0;
		}
	}
}

ShaderManager *Engine::get_shader_manager() { return this->shader_manager; }

Renderer *Engine::get_renderer() { return this->renderer; }

Camera *Engine::get_camera() { return this->camera; }

Display *Engine::get_display() {
	return this->display;
}
