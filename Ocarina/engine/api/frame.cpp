#include "context.h"

#include "../renderer/framebuffer.h"
#include <algorithm>
#include <stdexcept>

float TonicContext::delta_time() const { return elapsed; }

void TonicContext::begin_frame() {
	require_engine();
	if (in_frame) throw std::runtime_error("Previous Tonic frame has not ended.");
	scene.start_frame();
	engine.get_renderer()->begin_frame();
	engine.get_input()->begin_new_frame();
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		engine.get_input()->process_event(event, &engine);
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
		if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
			engine.get_display()->update_viewport();
			engine.get_input()->refresh_mouse_position(&engine);
		}
	}
	Uint64 now = SDL_GetPerformanceCounter();
	elapsed = std::min(0.1f, static_cast<float>(static_cast<double>(now - previous) / static_cast<double>(SDL_GetPerformanceFrequency())));
	previous = now;
	FrameBuffer::bind_screen_framebuffer();
	engine.set_framebuffer(nullptr);
	in_frame = true;
}

void TonicContext::end_frame() {
	require_frame();
	flush_frame();
	SDL_GL_SwapWindow(engine.get_display()->get_window());
	engine.get_input()->end_update();
	in_frame = false;
	release_pending();
}

void TonicContext::flush_frame() {
	require_frame();
	scene.flush_render_context();
}
