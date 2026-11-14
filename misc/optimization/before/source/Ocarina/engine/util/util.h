#pragma once

#include "../../util.h"

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <stb/stb_image.h>

struct Rect {
        float left;
        float right;
        float bottom;
        float top;
};

// File paths are UTF-8, including the returned current working directory.
std::string read_shader_file(const std::string &path);
bool file_exists(const std::string &path);
bool is_absolute_path(const std::string &path);
std::string current_working_directory();
std::string join_path(const std::string &base, const std::string &path);
std::string parent_path(const std::string &path);
std::string file_name(const std::string &path);
std::string resolve_resource_path(const std::string &path);
