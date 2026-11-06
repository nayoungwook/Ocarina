#include "vm.h"
#include "ir_load.h"
#include "util.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

void CVM::clear() {
	if (syscalls)
		throw std::runtime_error("Cannot clear a running CVM.");

	engine.shutdown();
	clear_execution();
	byte_files.clear();
	object_data.clear();
	function_data.clear();
	variable_data.clear();
	initializer = {};
	vm_string_pool = {};
	main_func_id = std::numeric_limits<unsigned>::max();
}

void CVM::register_bytes(std::string fname, std::vector<byte> bytes) {
	byte_files[std::move(fname)] = std::move(bytes);
}

void CVM::register_ir(std::string fname, const IRContext &irc) {
	register_bytes(std::move(fname), irc.get_bytes());
}

void CVM::register_bytecode(const std::string &path) {
	std::string bytes = read_file(path);
	register_bytes(path, std::vector<byte>(bytes.begin(), bytes.end()));
}

void CVM::load_files() {
	std::vector<std::string> ordered;

	for (const auto &entry : byte_files)
		ordered.push_back(entry.first);

	std::sort(ordered.begin(), ordered.end());
	std::vector<IRLoader> readers;
	readers.reserve(ordered.size());

	for (const std::string &name : ordered)
		readers.emplace_back(byte_files.at(name), name);

	IRLoader::read_ir_files(*this, readers);
}
