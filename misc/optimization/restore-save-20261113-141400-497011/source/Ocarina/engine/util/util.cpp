#include "util.h"
#include "error.h"

#include <cctype>
#include <array>

static bool is_separator(char value) {
	return value == '/' || value == '\\';
}

bool file_exists(const std::string &path) {
	if (path.empty())
		return false;
	// SDL uses UTF-8 paths on every platform, including Windows.
	SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "rb");
	if (file == nullptr)
		return false;
	SDL_CloseIO(file);
	return true;
}

bool is_absolute_path(const std::string &path) {
	if (path.empty())
		return false;
	if (is_separator(path[0]))
		return true;
	return path.size() > 2 &&
		std::isalpha(static_cast<unsigned char>(path[0])) &&
		path[1] == ':' && is_separator(path[2]);
}

std::string current_working_directory() {
	// The Windows narrow CRT returns the local code page, but SDL font and
	// sound loaders expect UTF-8. Keep resolved paths in the same encoding.
	char *path = SDL_GetCurrentDirectory();
	if (path == nullptr)
		return std::string();
	std::string result(path);
	SDL_free(path);
	return result;
}

std::string join_path(const std::string &base, const std::string &path) {
	if (base.empty() || is_absolute_path(path))
		return path;
	if (path.empty())
		return base;
	if (is_separator(base[base.size() - 1]))
		return base + path;
	return base + "/" + path;
}

std::string parent_path(const std::string &path) {
	if (path.empty())
		return std::string();
	size_t end = path.size();
	while (end > 0 && is_separator(path[end - 1]))
		--end;
	if (end == 0)
		return path.substr(0, 1);
	size_t separator = path.find_last_of("/\\", end - 1);
	if (separator == std::string::npos)
		return std::string();
	if (separator == 2 && path.size() > 2 && path[1] == ':')
		return path.substr(0, 3);
	if (separator == 0)
		return path.substr(0, 1);
	return path.substr(0, separator);
}

std::string file_name(const std::string &path) {
	if (path.empty())
		return std::string();
	size_t separator = path.find_last_of("/\\");
	return separator == std::string::npos ? path : path.substr(separator + 1);
}

std::string resolve_resource_path(const std::string &path) {
	if (is_absolute_path(path) && file_exists(path))
		return path;

	std::string cwd = current_working_directory();
	std::array<std::string, 4> candidates = {
		join_path(cwd, path),
		join_path(join_path(cwd, "resources"), path),
#ifdef ENGINE_RESOURCE_ROOT
		join_path(ENGINE_RESOURCE_ROOT, path),
#else
		std::string(),
#endif
		join_path("resources", path),
	};

	for (const auto &candidate : candidates) {
		if (!candidate.empty() && file_exists(candidate))
			return candidate;
	}

	for (std::string probe = cwd; !probe.empty();) {
		std::string candidate = join_path(join_path(probe, "resources"), path);
		if (file_exists(candidate))
			return candidate;
		std::string parent = parent_path(probe);
		if (probe == parent)
			break;
		probe = parent;
	}

	return join_path(cwd, path);
}

std::string read_shader_file(const std::string &path) {
        // Shader paths are often written relative to the project. In installed
        // builds, ENGINE_SHADER_ROOT lets the engine find them reliably.
	std::string resolved_path = path;
	if (!file_exists(resolved_path)) {
#ifdef ENGINE_SHADER_ROOT
		std::string requested = path;
		if ((file_name(parent_path(requested)) == "shaders" || file_name(parent_path(requested)) == "engine_shaders"))
			requested = file_name(requested);
		std::string candidate = join_path(ENGINE_SHADER_ROOT, requested);
		if (file_exists(candidate))
			resolved_path = candidate;
#endif
	}

	size_t size = 0;
	void *data = SDL_LoadFile(resolved_path.c_str(), &size);
	if (data == nullptr) {
		error("Failed to open file : " + path);
		return std::string();
	}

	// Return raw text; shader compilation or parsing is handled by callers.
	std::string contents(static_cast<const char *>(data), size);
	SDL_free(data);
	return contents;
}
