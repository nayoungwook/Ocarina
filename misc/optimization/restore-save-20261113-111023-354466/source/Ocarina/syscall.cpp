#include "parser.h"

void ParserContext::register_syscall(const std::string &func_name,
	const std::string &ret_type,
	unsigned id,
	bool is_varargs) {
	FuncData func_data = {};
	func_data.id = id;
	func_data.func_name = func_name;
	func_data.return_type = find_type(*this, ret_type);
	func_data.varargs = is_varargs;
	func_data.scope_data = ScopeSyscall;

	syscall_smtb.emplace(func_name, std::move(func_data));
}

void ParserContext::init_window_syscall() {
	FuncData &window_func_data = syscall_smtb.at("window");
	window_func_data.arg_types = {
		find_type(*this, "string"),
		find_type(*this, "int"),
		find_type(*this, "int")
	};
	window_func_data.arg_count = static_cast<unsigned>(window_func_data.arg_types.size());
}

void ParserContext::init_texture_syscall() {
	FuncData &texture_func_data = syscall_smtb.at("texture");

	texture_func_data.arg_types = {
		find_type(*this, "string"),
	};

	texture_func_data.arg_count = 1;
}

void ParserContext::init_render_texture_syscall() {
	FuncData &func_data = syscall_smtb.at("render_texture");
	func_data.arg_types = {
		find_type(*this, "texture"),
		find_type(*this, "vector"),
		find_type(*this, "float"),
		find_type(*this, "float")
	};
	func_data.arg_count = static_cast<unsigned>(func_data.arg_types.size());
}

void ParserContext::init_set_scene_syscall() {
	FuncData &func_data = syscall_smtb.at("set_scene");
	func_data.arg_types = { find_type(*this, "scene") };
	func_data.arg_count = static_cast<unsigned>(func_data.arg_types.size());
}

void ParserContext::init_syscall() {
	register_syscall("print", "void", 0, true);
	register_syscall("vector", "vector", 1, true);
	register_syscall("texture", "texture", 2, false);
	register_syscall("window", "window", 3, false);
	register_syscall("render_texture", "void", 4, false);
	register_syscall("set_scene", "void", 5, false);

	init_window_syscall();
	init_texture_syscall();
	init_render_texture_syscall();
	init_set_scene_syscall();
}
