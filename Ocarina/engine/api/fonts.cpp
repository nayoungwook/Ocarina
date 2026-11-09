#include "context.h"

#include <cmath>
#include "../../ovm_panic.h"

int TonicContext::load_font(const std::string &path, float size) {
	require_engine(); check_path(path);
	if (!std::isfinite(size) || size <= 0) throw OVMPanic("Font size must be positive.");
	int id = new_handle(); fonts.try_emplace(id, path, size); return id;
}

void TonicContext::unload_font(int handle) {
	require_engine();
	if (!fonts.contains(handle) || pending_fonts.contains(handle)) throw OVMPanic("Invalid font handle.");
	if (in_frame) pending_fonts.insert(handle); else fonts.erase(handle);
}

void TonicContext::draw_text(int handle, const std::string &text, float x, float y, int z_order) {
	require_frame();
	if (!fonts.contains(handle) || pending_fonts.contains(handle)) throw OVMPanic("Invalid font handle.");
	engine.get_renderer()->render_font(&fonts.at(handle), text, Vector(x, y), "left", z_order);
}
