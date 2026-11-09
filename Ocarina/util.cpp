#include "util.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include "ovm_panic.h"

static std::filesystem::path file_path(const std::string &path) {
	if (path.find('\0') != std::string::npos) throw OVMPanic("File path contains a null character.");
	return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

std::string read_file(const std::string &path) {
	std::ifstream fp(file_path(path), std::ios::binary | std::ios::ate);
	if (!fp) throw OVMPanic("Failed to open file : " + path);
	std::streamoff size = fp.tellg();
	std::string buffer;
	if (size < 0 || static_cast<std::uintmax_t>(size) > buffer.max_size() ||
		static_cast<std::uintmax_t>(size) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
		throw OVMPanic("Failed to read file size : " + path);
	buffer.resize(static_cast<std::size_t>(size));
	fp.seekg(0);
	if (!buffer.empty()) fp.read(buffer.data(), static_cast<std::streamsize>(size));
	if (!fp) throw OVMPanic("Failed to read file : " + path);
	return buffer;
}

void write_file(const std::string &path, std::string_view data) {
	const std::size_t len = data.size();
	if (len > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
		throw OVMPanic("File data is too large : " + path);
	std::ofstream fp(file_path(path), std::ios::binary | std::ios::trunc);
	if (!fp) throw OVMPanic("Failed to open file : " + path);
	if (len != 0) fp.write(data.data(), static_cast<std::streamsize>(len));
	fp.close();
	if (!fp) throw OVMPanic("Failed to write file : " + path);
}

void write_file(const std::string &path, std::span<const std::uint8_t> data) {
	if (data.empty()) write_file(path, std::string_view{});
	else write_file(path, std::string_view(reinterpret_cast<const char *>(data.data()), data.size()));
}
