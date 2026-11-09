#include "context.h"

#include "../../ovm_panic.h"

int TonicContext::load_texture(const std::string &path) {
	require_engine(); check_path(path);
	int id = new_handle(); textures.try_emplace(id, path); return id;
}

void TonicContext::unload_texture(int handle) {
	require_engine();
	if (!textures.contains(handle) || pending_textures.contains(handle)) throw OVMPanic("Invalid texture handle.");
	if (in_frame) pending_textures.insert(handle); else textures.erase(handle);
}

void TonicContext::draw_texture(int handle, float x, float y, float width, float height, float rotation, int z_order) {
	require_frame();
	if (!textures.contains(handle) || pending_textures.contains(handle)) throw OVMPanic("Invalid texture handle.");
	engine.get_renderer()->render_image(&textures.at(handle), Vector(x, y), width, height, rotation, z_order);
}
