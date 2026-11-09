#include "parser.h"
#include "util.h"
#include "error.h"

#include <cctype>
#include <filesystem>
#include "ovm_panic.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

static std::filesystem::path executable_directory() {
#ifdef _WIN32
	std::wstring path(256, L'\0');
	while (true) {
		DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (size == 0) throw OVMPanic("Failed to locate the executable.");
		if (size < path.size()) {
			path.resize(size);
			return std::filesystem::path(path).parent_path();
		}
		path.resize(path.size() * 2);
	}
#elif defined(__linux__)
	return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#else
	return std::filesystem::current_path();
#endif
}

std::string ParserContext::register_file(const std::string &path) {
	if (path.find('\0') != std::string::npos) throw OVMPanic("File path contains a null character.");
	std::filesystem::path file_path(std::u8string(path.begin(), path.end()));
	if (file_path.is_relative()) file_path = executable_directory() / file_path;
	file_path = std::filesystem::canonical(file_path);
	const auto utf8 = file_path.u8string();
	const std::string source_name(utf8.begin(), utf8.end());
	std::string key = source_name;
#ifdef _WIN32
	for (char &ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif
	if (registered_files.contains(key)) return registered_files.at(key);
	first_pass_queue.emplace(read_file(source_name), source_name);
	registered_files.emplace(std::move(key), source_name);
	return source_name;
}

void ParserContext::register_import(const std::string &path) {
	std::filesystem::path file_path(std::u8string(path.begin(), path.end()));
	if (file_path.is_relative()) {
		std::filesystem::path source_path(std::u8string(tc.source_name.begin(), tc.source_name.end()));
		std::filesystem::path base = source_path.parent_path();
		if (!source_path.is_absolute()) base = executable_directory() / base;
		file_path = base / file_path;
	}
	try {
		file_path = std::filesystem::canonical(file_path);
		const auto utf8 = file_path.u8string();
		const std::string source = register_file(std::string(utf8.begin(), utf8.end()));
		imports[tc.source_name].push_back(source);
	}
	catch (const std::exception &error) { panic("Failed to import " + path + ": " + error.what(), tc); }
}

void ParserContext::resolve_first_pass_queue() {
	while (!first_pass_queue.empty()) {
		tc = std::move(first_pass_queue.front());
		first_pass_queue.pop();
		while (tc.peek().type != TokenType::TokEOF) parse_structure();
		tc.init_tc();
		second_pass_queue.push(std::move(tc));
	}
}

void ParserContext::resolve_second_pass_queue() {
	std::unordered_map<std::string, TokenizerContext> files;
	std::vector<std::string> names;
	while (!second_pass_queue.empty()) {
		TokenizerContext source = std::move(second_pass_queue.front());
		second_pass_queue.pop();
		names.push_back(source.source_name);
		files.emplace(names.back(), std::move(source));
	}
	std::unordered_set<std::string> compiled;
	for (const std::string &name : names) compile_source(name, files, compiled);
}

void ParserContext::compile_source(const std::string &path,
	std::unordered_map<std::string, TokenizerContext> &files,
	std::unordered_set<std::string> &compiled) {
	if (!compiled.insert(path).second) return;
	for (const std::string &dependency : imports[path]) compile_source(dependency, files, compiled);
	compile_file(std::move(files.at(path)));
}

void ParserContext::resolve_preprocessor() {
	std::queue<TokenizerContext> preprocessed_files;
	while (!first_pass_queue.empty()) {
		tc = std::move(first_pass_queue.front());
		first_pass_queue.pop();
		while (tc.peek().type != TokenType::TokEOF) parse_preprocessor();
		tc.init_tc();
		preprocessed_files.push(std::move(tc));
	}
	first_pass_queue = std::move(preprocessed_files);
}

void ParserContext::compile_files() {
	resolve_preprocessor();
	resolve_first_pass_queue();
	resolve_second_pass_queue();
}
