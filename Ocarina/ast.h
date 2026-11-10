#pragma once

#include "token.h"
#include "type.h"

#include <variant>
#include <vector>
#include <limits>

#define ACMOD_PUBLIC 1
#define ACMOD_PRIVATE 2
#define ACMOD_PROTECTED 3
#define ACMOD_DEFAULT 4

using NodeId = std::size_t;
inline constexpr NodeId NoNode = std::numeric_limits<NodeId>::max();

struct Node;

enum ASTType {
	AST_None = -1,
	AST_NumberLiteral = 0,
	AST_Identifier = 1,
	AST_VariableDeclaration = 2,
	AST_VariableDeclarationBundle = 3,
	AST_BinExpr = 4,
	AST_IfStatement = 5,
	AST_UnaryExpr = 6,
	AST_FunctionDeclaration = 7,
	AST_FunctionCall = 8,
	AST_StringLiteral = 9,
	AST_ForStatement = 10,
	AST_IdentIncrease = 11,
	AST_IdentDecrease = 12,
	AST_Return = 13,
	AST_Object = 14,
	AST_Constructor = 15,
	AST_New = 16,
	AST_Null = 17,
	AST_BoolLiteral = 20,
	AST_Negative = 21,
	AST_This = 22,
	AST_Cast = 23,
};

struct NumberLiteralAST {
	struct Token num_tok;
	struct Type type;
};

struct StringLiteralAST {
	struct Token str_tok;
};

struct IdentifierAST {
	struct Token ident;
	bool is_attr = {};
	int vector_component = -1;

	// semantics
	std::size_t var_data = NoNode;
};

struct BoolLiteralAST {
	struct Token bool_tok;
};

struct VarDeclAST {
	struct Token var_name_tok;
	struct Token var_type_tok;
	NodeId decl = NoNode;
	int ac_mod = {};

	// semantics
	std::size_t var_data = NoNode;
};

struct VarDeclBundleAST {
	std::vector<struct Node> var_decls;
};

enum OperatorType {
	OpNone = 0,
	OpADD = 1,
	OpSUB = 2,
	OpMUL = 3,
	OpDIV = 4,
	OpEQUAL = 5,
	OpNOTEQUAL = 6,
	OpGREATER = 7,
	OpLESS = 8,
	OpEQUALGREATER = 9,
	OpEQUALLESS = 10,
	OpASSIGN = 11,
	OpOR = 12,
	OpAND = 13,
	OpPLUSASSIGN = 14,
	OpMINUSASSIGN = 15,
	OpMULTASSIGN = 16,
	OpDIVASSIGN = 17,
	OpMOD = 18,
	OpMODASSIGN = 19,
};

inline bool is_assignment_op(OperatorType op) {
	return op == OpASSIGN || op == OpPLUSASSIGN || op == OpMINUSASSIGN ||
		op == OpMULTASSIGN || op == OpDIVASSIGN || op == OpMODASSIGN;
}

struct BinExprAST {
	NodeId left = NoNode, right = NoNode;
	enum OperatorType op_type = OpNone;
};

struct UnaryExprAST {
	NodeId expr = NoNode;
};

enum IfStmtType { StmtNone, StmtIf, StmtElseIf, StmtElse };

struct IfStmtAST {
	enum IfStmtType stmt_type = StmtNone;
	NodeId cond = NoNode;
	NodeId next_stmt = NoNode;
	std::vector<struct Node> body;
};

struct FuncDeclAST {
	struct Token func_name_tok;
	struct Token ret_type_tok;
	NodeId params = NoNode; // variable declaration bundle.
	std::vector<struct Node> body;
	int ac_mod = {};

	// semantics
	std::size_t func_data = NoNode;
	std::vector<std::size_t> declared_vars;
};

struct ParamData {
	std::vector<struct Node> params;
};

enum class AttrSyscallType {
	ATTR_SYSCALL_NONE,
	ATTR_SYSCALL_ADD_CHILD,
	ATTR_SYSCALL_GET_CHILD,
	ATTR_SYSCALL_CHILD_SIZE,
};

struct FuncCallAST {
	struct Token func_name_tok;
	std::vector<struct Node> params;
	bool is_attr = {};

	AttrSyscallType attr_syscall_type = AttrSyscallType::ATTR_SYSCALL_NONE;
	bool is_syscall = false;
	bool is_super_method = false;

	// semantics
	std::size_t func_data = NoNode;
	std::size_t super_object = NoNode;
};

struct IdentIncreAST {
	NodeId ident_node = NoNode;
	bool is_attr = {};
};

struct IdentDecreAST {
	NodeId ident_node = NoNode;
	bool is_attr = {};
};

struct ForStmtAST {
	NodeId init = NoNode;
	NodeId cond = NoNode;
	NodeId step = NoNode;
	std::vector<struct Node> body;
};

struct ReturnAST {
	NodeId expr = NoNode;
};

struct ConstructorAST {
	NodeId params = NoNode;
	std::vector<struct Node> body;
	std::size_t object_data = NoNode;

	struct Token func_name;
	int ac_mod = {};

	// semantics
	std::size_t func_data = NoNode;
	std::vector<std::size_t> declared_vars;
};

struct ObjectAST {
	std::vector<struct Node> body;

	struct Token name_tok;
	struct Token parent_name_tok;

	// semantics
	std::size_t object_data = NoNode;
};

struct NewAST {
	struct Token name_tok;
	std::vector<struct Node> params;

	// semantics
	std::size_t object_data = NoNode;
};

struct NullAST {
	struct Token null_tok;
};

struct ThisAST {
	struct Token this_tok;
	std::size_t object_data = NoNode;
};

struct NegAST {
	NodeId ast = NoNode;
};

struct CastAST {
	Token target_type_tok;
	NodeId expr = NoNode;
	Type target_type;
};

struct Node {
	enum ASTType type = AST_None;
	std::variant<std::monostate, NumberLiteralAST, StringLiteralAST, IdentifierAST, BoolLiteralAST, VarDeclAST, VarDeclBundleAST, BinExprAST, UnaryExprAST, IfStmtAST, FuncDeclAST, FuncCallAST, IdentIncreAST, IdentDecreAST, ForStmtAST, ReturnAST, ConstructorAST, ObjectAST, NewAST, NullAST, NegAST, ThisAST, CastAST> ast;
	NodeId attr = NoNode;
};


