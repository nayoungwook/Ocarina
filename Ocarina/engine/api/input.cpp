#include "context.h"

#include <stdexcept>

SDL_Scancode TonicContext::scancode(const std::string &key) {
	SDL_Scancode value = SDL_GetScancodeFromName(key.c_str());
	if (value == SDL_SCANCODE_UNKNOWN) throw std::runtime_error("Unknown key: " + key);
	return value;
}

bool TonicContext::key_down(const std::string &key) { require_engine(); return engine.get_input()->is_key_down(scancode(key)); }

bool TonicContext::key_pressed(const std::string &key) { require_engine(); return engine.get_input()->is_key_pressed(scancode(key)); }

bool TonicContext::key_released(const std::string &key) { require_engine(); return engine.get_input()->is_key_released(scancode(key)); }

bool TonicContext::mouse_down(int button) {
	require_engine();
	if (button < 1 || button > 5) throw std::runtime_error("Mouse button must be between 1 and 5.");
	return engine.get_input()->is_mouse_down(static_cast<Uint8>(button));
}

float TonicContext::mouse_x() { require_engine(); return engine.get_input()->get_ui_mouse_x(); }

float TonicContext::mouse_y() { require_engine(); return engine.get_input()->get_ui_mouse_y(); }
