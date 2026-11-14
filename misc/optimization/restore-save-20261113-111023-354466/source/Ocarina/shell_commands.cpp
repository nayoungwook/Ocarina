#include "shell_commands.h"
#include "util.h"
#include "vm.h"
#include "ir_load.h"

#include <cctype>
#include <filesystem>
#include <iostream>
#include "ovm_panic.h"
#include <unordered_map>
#include <utility>

namespace {
std::filesystem::path checked_path(const std::string &name, const std::string &extension) {
	std::filesystem::path path(std::u8string(name.begin(), name.end()));
	std::string actual = path.extension().string();
	for (char &ch : actual)
		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	if (actual != extension)
		throw OVMPanic("Expected a " + extension + " file: " + name);
	return std::filesystem::canonical(std::filesystem::absolute(path));
}

std::string utf8_path(const std::filesystem::path &path) {
	const auto name = path.u8string();
	return std::string(name.begin(), name.end());
}

void compile_sources(const std::vector<std::string> &command) {
	if (command.size() < 2)
		throw OVMPanic("Usage: compile file.oc [file2.oc ...]");
	ParserContext pc;
	for (std::size_t i = 1; i < command.size(); ++i)
		pc.register_file(utf8_path(checked_path(command[i], ".oc")));
	pc.compile_files();
	IRContext irc;
	const auto files = irc.gen_files(std::move(pc));
	for (const IRFile &file : files) {
		std::filesystem::path output(std::u8string(file.source.begin(), file.source.end()));
		output.replace_extension(".ob");
		const std::string path = utf8_path(output);
		const std::string bytes(file.bytes.begin(), file.bytes.end());
		if (std::filesystem::exists(output) && read_file(path) == bytes) continue;
		write_file(path, file.bytes);
	}
}

void register_bytecode(OVM &vm, const std::vector<std::string> &command) {
	if (command.size() < 2)
		throw OVMPanic("Usage: register file.ob [file2.ob ...]");
	std::unordered_map<std::string, std::vector<byte>> files;
	for (std::size_t i = 1; i < command.size(); ++i) {
		const auto name = utf8_path(checked_path(command[i], ".ob"));
		const auto bytes = read_file(name);
		files[name] = std::vector<byte>(bytes.begin(), bytes.end());
	}
	const auto readers = IRLoader::read_files(files);
	files.clear();
	for (const IRLoader &reader : readers) files.emplace(reader.get_source(), reader.get_bytes());
	vm.clear();
	vm.byte_files = std::move(files);
}
}

void execute_shell_command(OVM &vm, const std::vector<std::string> &command) {
	if (command[0] == "compile")
		compile_sources(command);
	else if (command[0] == "register")
		register_bytecode(vm, command);
	else if (command[0] == "run") {
		vm.engine.shutdown();
		vm.load_files();
		vm.run(std::cout, OVM::RunMode::ReturnAfterMain);
		std::cout << '\n';
	} else if (command[0] == "stop")
		vm.engine.shutdown();
	else
		throw OVMPanic("Unknown command: " + command[0]);
}
