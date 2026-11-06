#include "context.h"

#include <cmath>
#include <stdexcept>

void TonicContext::clear(float r, float g, float b, float a) {
	require_frame(); engine.get_renderer()->clear(r, g, b, a);
}

void TonicContext::set_color(float r, float g, float b, float a) {
	require_engine(); engine.get_renderer()->set_color(Color(r, g, b, a));
}

void TonicContext::draw_rect(float x, float y, float width, float height, float rotation, int z_order) {
	require_frame(); engine.get_renderer()->render_rect(Vector(x, y), width, height, rotation, z_order);
}

void TonicContext::draw_oval(float x, float y, float width, float height, float rotation, int z_order) {
	require_frame(); engine.get_renderer()->render_oval(Vector(x, y), width, height, rotation, z_order);
}

void TonicContext::set_camera(float x, float y, float zoom, float rotation) {
	require_engine();
	if (!std::isfinite(zoom) || zoom <= 0) throw std::runtime_error("Camera zoom must be positive.");
	engine.get_camera()->position = Vector(x, y);
	engine.get_camera()->zoom = zoom;
	engine.get_camera()->rotation = rotation;
}
