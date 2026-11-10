#pragma once

#include "ast.h"

#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

enum ScopeData {
	ScopeNone, ScopeLocal, ScopeGlobal, ScopeObject, ScopeSyscall
};

struct FuncData {
	unsigned id = {};
	std::size_t symbol_id = NoNode;
	std::size_t owner_object_index = NoNode;
	std::string func_name;
	struct Type return_type = {};

	std::vector<struct Type> arg_types;
	unsigned arg_count = {};

	bool is_constructor = false;
	bool is_builtin = false;

	bool varargs = false;

	enum ScopeData scope_data = ScopeNone;
};

struct VarData {
	unsigned id = {};
	std::size_t symbol_id = NoNode;
	unsigned offset = {}; // offset data from stack pointer or object
	struct Type type = {};
	std::string var_name;

	enum ScopeData scope_data = ScopeNone;
};

struct ObjectData {
	unsigned id = {};
	unsigned instance_size = {};

	struct Type object_type, parent_type;

	std::unordered_map<std::string, struct VarData> member_vars;
	std::unordered_map<std::string, struct FuncData> member_funcs;

	struct FuncData constructor = {};
};

struct Scope {
	std::unordered_map<std::string, struct VarData> local_var_smtb;
	std::size_t prev_scope = NoNode;
};

struct SourceFile {
	std::string path;
	std::size_t first_node = 0, node_count = 0;
	std::vector<std::string> imports;
};

class ParserContext {
public:
	ParserContext();

	TokenizerContext tc;

	std::queue<TokenizerContext> first_pass_queue;
	std::queue<TokenizerContext> second_pass_queue;

	std::unordered_map<std::string, struct VarData> glob_var_smtb;  // VarData will be stored.
	std::unordered_map<std::string, struct FuncData> glob_func_smtb; // FuncData will be stored.

	std::unordered_map<std::string, struct Type> object_type_smtb;     // Type will be stored.
	std::unordered_map<std::string, struct Type> primitive_type_smtb; // Type will be stored.

	std::vector<struct Type> numeric_types;

	std::unordered_map<std::string, struct FuncData> syscall_smtb;
	std::vector<struct ObjectData> object_data;
	std::vector<struct FuncData> func_data;
	std::vector<struct VarData> var_data;

	std::vector<struct Node> nodes;
	std::vector<SourceFile> source_files;

	void compile_file(TokenizerContext input_tc);
	std::string register_file(const std::string &path);
	void compile_files();
	const struct Node &get_node(NodeId id) const;
	struct Node &get_node(NodeId id);
	const FuncData &get_func_data(const FuncCallAST &func_call_ast) const;
	struct Node parse_expr_node();
	struct Node parse_stmt();
	void parse_structure();
	void parse_preprocessor();

private:
	
	void resolve_preprocessor();
	void resolve_first_pass_queue();
	void resolve_second_pass_queue();
	void register_import(const std::string &path);
	std::unordered_map<std::string, std::string> registered_files;
	std::unordered_map<std::string, std::vector<std::string>> imports;
	void compile_source(const std::string &path,
		std::unordered_map<std::string, TokenizerContext> &files,
		std::unordered_set<std::string> &compiled);

	std::vector<struct Node> node_pool;
	struct ParamData parse_func_call_params();
	void add_primitive_numeric(const std::string &name, unsigned nbyte, unsigned rank, bool is_signed, bool is_integer);
	void init_primitive();
	void register_syscall(const std::string &func_name, const std::string &ret_type,
		unsigned id, bool is_varargs);
	void init_window_syscall();
	void init_syscall();

	template<class T>
	static struct Node pack(enum ASTType type, T ast) {
		struct Node result = {};
		result.type = type;
		result.ast = std::move(ast);
		return result;
	}

	NodeId store_node(struct Node node);
	Token parse_type_token();
	bool is_variable_target(const Node &node) const;
	struct Node gen_func_call_node(const struct Token &first);
	void parse_attribute(struct Node &ident_node);
	struct Node gen_ident_node(const struct Token &first);
	struct Node gen_func_param_node();
	std::vector<struct Node> gen_body();
	struct Node gen_constructor_node(const struct Token &first);
	struct Node gen_func_decl_node(const struct Token &first);
	struct Node gen_var_decl_node(const struct Token &first);
	enum IfStmtType get_if_stmt_type(const struct Token &first);
	struct Node gen_next_if_stmt_node(enum IfStmtType stmt_type);
	struct Node gen_if_stmt_node(const struct Token &first);
	struct Node gen_for_stmt_node(const struct Token &first);
	struct Node gen_ret_node(const struct Token &first);
	struct Node gen_object_decl_node(const struct Token &first);
	struct Node gen_new_node(const struct Token &first);
	Node gen_cast_node();
	static bool is_number_literal_integer(const std::string &num_lit_str);
	static short get_number_literal_byte(const std::string &num_lit_str);
	struct Type find_numeric_type(unsigned nbyte, bool is_integer, bool is_signed);
	struct Node parse_term();
	struct Node parse_simple_expression();
	struct Node parse_prefix();
	struct Node parse_postfix();
	struct Node parse_unary_expression();
	struct Node parse_compare_expression();
	struct Node parse_logical_expression();
	struct Node parse_expression();

	void pass_body();
	void pass_func_param();
	void parse_object_structure();
	void parse_func_structure();
	void parse_constructor_structure();
	void parse_var_structure();

};
