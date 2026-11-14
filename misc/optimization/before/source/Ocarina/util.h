#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

std::string read_file(const std::string &path);
void write_file(const std::string &path, std::string_view data);
void write_file(const std::string &path, const std::vector<std::uint8_t> &data);
