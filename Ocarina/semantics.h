#pragma once

#include "parser.h"

class SemanticsContext {
public:
	void resolve_semantics(ParserContext &pc);
	Type infer_type(const ParserContext &pc, const Node &node) const;
	Type infer_operation_type(const ParserContext &pc, const Type &lhs, const Type &rhs) const;

private:
	std::vector<Scope> scopes;
	std::size_t current_scope = NoNode;
	std::size_t current_object = NoNode;
	std::size_t current_func = NoNode;
	const Node *constructor_first_statement = nullptr;
	unsigned stack_offset = 0;
	unsigned global_offset = 0;
	std::vector<std::size_t> declared_local_vars;

	void register_objects(ParserContext &pc);
	void register_data(ParserContext &pc, Node &node, std::size_t object_id);
	std::size_t register_func_data(ParserContext &pc, const Token &func_name, const Type &ret_type,
		NodeId params, std::size_t object_id, bool is_constructor);
	std::size_t register_non_local_var_data(ParserContext &pc, VarDeclAST &var_decl, std::size_t object_id);
	void layout_object(ParserContext &pc, std::size_t object_id, std::vector<unsigned> &state);
	void open_scope();
	void close_scope();
	void register_local_var_data(ParserContext &pc, VarDeclAST &var_decl);
	std::size_t find_var_data(const ParserContext &pc, const std::string &var_name) const;
	std::size_t find_func_data(const ParserContext &pc, const std::string &func_name) const;
	std::size_t find_member_var_data(const ParserContext &pc, std::size_t attr_of, const std::string &key) const;
	std::size_t find_member_func_data(const ParserContext &pc, std::size_t attr_of, const std::string &key) const;
	void check_semantics(ParserContext &pc, Node &node);
	std::size_t bind_super_call(ParserContext &pc, const Node &node) const;
	void check_value(ParserContext &pc, Node &node, const Type &expected);
	void bind_add_child(ParserContext &pc, Node &node, FuncCallAST &call);
	void check_child(ParserContext &pc, NodeId id);
	void check_syscall(ParserContext &pc, const FuncCallAST &func_call_ast);
	void check_semantics_body(ParserContext &pc, std::vector<Node> &body);
	void check_function(ParserContext &pc, NodeId params, std::vector<Node> &body,
		std::size_t func_id, std::vector<std::size_t> &declared_vars);
	void register_attr_data(ParserContext &pc, std::size_t attr_of, Node &node);
	void resolve_attr(ParserContext &pc, const Type &type_of_node, Node &node);
};

static void add_builtin_member(ParserContext &pc, ObjectData &object,
	const std::string &name,
	const std::string &type_name);