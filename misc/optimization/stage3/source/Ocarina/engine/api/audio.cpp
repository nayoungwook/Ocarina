#include "context.h"

#include <algorithm>
#include "../../ovm_panic.h"

int TonicContext::load_sound(const std::string &path) {
	require_engine(); check_path(path);
	if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && !SDL_InitSubSystem(SDL_INIT_AUDIO))
		throw OVMPanic(std::string("Audio initialization failed: ") + SDL_GetError());
	int id = new_handle(); sounds.try_emplace(id, path);
	if (!sounds.at(id).is_loaded()) { sounds.erase(id); throw OVMPanic("Sound failed to load."); }
	return id;
}

void TonicContext::unload_sound(int handle) {
	require_engine();
	if (!sounds.contains(handle) || pending_sounds.contains(handle)) throw OVMPanic("Invalid sound handle.");
	if (in_frame) pending_sounds.insert(handle); else sounds.erase(handle);
}

void TonicContext::play_sound(int handle) {
	require_engine();
	if (!sounds.contains(handle) || pending_sounds.contains(handle)) throw OVMPanic("Invalid sound handle.");
	sounds.at(handle).play();
}

void TonicContext::stop_sound(int handle) {
	require_engine();
	if (!sounds.contains(handle) || pending_sounds.contains(handle)) throw OVMPanic("Invalid sound handle.");
	sounds.at(handle).stop();
}

void TonicContext::set_sound_volume(int handle, float volume) {
	require_engine();
	if (!sounds.contains(handle) || pending_sounds.contains(handle)) throw OVMPanic("Invalid sound handle.");
	sounds.at(handle).set_volume(std::clamp(volume, 0.0f, 1.0f));
}
