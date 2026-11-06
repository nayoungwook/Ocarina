#include "context.h"

#include "../util/util.h"
#include <limits>
#include <stdexcept>

void TonicContext::check_path(const std::string &path) {
	if (path.empty() || path.find('\0') != std::string::npos) throw std::runtime_error("Invalid asset path.");
	if (!file_exists(resolve_resource_path(path))) throw std::runtime_error("Asset not found: " + path);
}

int TonicContext::new_handle() {
	if (next_handle == std::numeric_limits<int>::max()) throw std::runtime_error("Tonic handle limit reached.");
	return next_handle++;
}

void TonicContext::release_pending() {
	for (int id : pending_textures) textures.erase(id);
	for (int id : pending_fonts) fonts.erase(id);
	for (int id : pending_sounds) sounds.erase(id);
	pending_textures.clear(); pending_fonts.clear(); pending_sounds.clear();
}
