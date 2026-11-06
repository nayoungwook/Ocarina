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

	clear_execution();
	byte_files.clear();
	byte_formats.clear();
	function_layouts.clear();
	object_layouts.clear();
	object_data.clear();
	function_data.clear();
	variable_data.clear();
	initializer = {};
	vm_string_pool = {};
	main_func_id = std::numeric_limits<unsigned>::max();
	loaded_format = IRFormat::Ocarina;
}

void CVM::register_bytes(std::string fname, std::vector<byte> bytes, IRFormat format) {
	function_layouts.erase(fname);
	object_layouts.erase(fname);
	byte_formats[fname] = format;
	byte_files[std::move(fname)] = std::move(bytes);
}

void CVM::register_ir(std::string fname, const IRContext &irc) {
	register_bytes(fname, irc.get_bytes());
	function_layouts[fname] = irc.get_function_layouts();
	object_layouts[std::move(fname)] = irc.get_object_layouts();
}

void CVM::register_bytecode(const std::string &path, IRFormat format) {
	std::string bytes = read_file(path);
	register_bytes(path, std::vector<byte>(bytes.begin(), bytes.end()), format);
}

void CVM::load_files() {
	std::vector<std::string> ordered;

	for (const auto &entry : byte_files)
		ordered.push_back(entry.first);

	std::sort(ordered.begin(), ordered.end());
	std::vector<IRLoader> readers;
	readers.reserve(ordered.size());

	for (const std::string &name : ordered) {
		IRFormat format = byte_formats.contains(name) ? byte_formats.at(name) : IRFormat::Ocarina;
		readers.emplace_back(byte_files.at(name), format, name);
	}

	CVM loaded;
	IRLoader::read_ir_files(loaded, readers);

	std::map<std::string, unsigned> runtime_objects;
	for (const auto &[id, object] : loaded.object_data)
		runtime_objects[object.name] = id;

	for (const auto &[file, layouts] : object_layouts) {
		std::map<unsigned, unsigned> object_ids;
		for (const auto &[name, layout] : layouts)
			object_ids[layout.id] = runtime_objects.at(name);

		for (const auto &[name, layout] : layouts) {
			VMObjectData &object = loaded.object_data.at(runtime_objects.at(name));
			object.parent_id = layout.parent_id == 0 ? 0 : object_ids.at(layout.parent_id);
			object.size = layout.size;
			object.variable_data = layout.variable_data;
			object.layout_known = true;
			object.parent_resolved = true;
		}
	}

	for (const auto &[file, layouts] : function_layouts) {
		for (const auto &[key, layout] : layouts) {
			const auto &[owner, id] = key;
			unsigned object_id = 0;
			if (!owner.empty()) {
				const auto object = runtime_objects.find(owner);
				if (object == runtime_objects.end())
					throw std::runtime_error("Compiled object metadata is missing: " + owner);
				object_id = object->second;
			}

			auto &table = object_id == 0 ? loaded.function_data
										 : loaded.object_data.at(object_id).function_data;
			VMFunctionData &function = table.at(id);
			function.argument_count = layout.argument_count;
			function.argument_types = layout.argument_types;
			function.local_types = layout.local_types;
			function.stack_size = layout.stack_size;
			function.metadata_complete = true;
		}
	}

	object_data = std::move(loaded.object_data);
	function_data = std::move(loaded.function_data);
	variable_data = std::move(loaded.variable_data);
	initializer = std::move(loaded.initializer);
	vm_string_pool = std::move(loaded.vm_string_pool);
	main_func_id = loaded.main_func_id;
	loaded_format = loaded.loaded_format;
}
