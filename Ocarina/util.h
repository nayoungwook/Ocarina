#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

std::string read_file(const std::string &path);
void write_file(const std::string &path, std::string_view data);
void write_file(const std::string &path, std::span<const std::uint8_t> data);
