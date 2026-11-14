#include "vm.h"
#include "ir_load.h"
#include "util.h"

#include <algorithm>
#include <limits>
#include "ovm_panic.h"
#include <utility>

void OVM::clear() {
	if (executing)
		throw OVMPanic("Cannot clear a running OVM.");

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

void OVM::register_bytes(std::string fname, std::vector<byte> bytes) {
	byte_files[std::move(fname)] = std::move(bytes);
}

void OVM::register_ir(std::string fname, const IRContext &irc) {
	register_bytes(std::move(fname), irc.get_bytes());
}

void OVM::register_bytecode(const std::string &path) {
	std::string bytes = read_file(path);
	register_bytes(path, std::vector<byte>(bytes.begin(), bytes.end()));
}

void OVM::load_files() {
	if (executing) throw OVMPanic("Cannot load bytecode in a running OVM.");
	auto readers = IRLoader::read_files(byte_files);
	IRLoader::read_ir_files(*this, readers);
	engine.shutdown();
	clear_execution();
}
