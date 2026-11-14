#include "context.h"

#include "../../ovm_panic.h"

bool TonicContext::active_session = false;

TonicContext::TonicContext() : scene(engine) {}

TonicContext::~TonicContext() { shutdown(); }

void TonicContext::require_engine() const {
	if (!initialized) throw OVMPanic("Tonic engine is not initialized.");
}

void TonicContext::require_frame() const {
	require_engine();
	if (!in_frame) throw OVMPanic("Drawing requires begin_frame().");
}

void TonicContext::init_engine(const std::string &title, int width, int height) {
	if (width <= 0 || height <= 0 || title.find('\0') != std::string::npos) throw OVMPanic("Invalid Tonic window settings.");
	if (active_session && !initialized) throw OVMPanic("Only one Tonic window can be active.");
	shutdown();
	try {
		engine.init_engine();
		engine.init_window(title, width, height);
		engine.set_scene(&scene);
		initialized = true; active_session = true; running = true;
		previous = SDL_GetPerformanceCounter();
		elapsed = 1.0f / 60.0f;
	}
	catch (...) {
		engine.shutdown();
		throw;
	}
}

void TonicContext::shutdown() {
	if (!initialized) return;
	scene.start_frame();
	textures.clear(); fonts.clear(); sounds.clear();
	pending_textures.clear(); pending_fonts.clear(); pending_sounds.clear();
	engine.shutdown();
	initialized = false; running = false; in_frame = false; active_session = false;
}

bool TonicContext::is_running() const { return initialized && running; }

void TonicContext::quit() { running = false; }
