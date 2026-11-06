#include "semantics.h"
#include "error.h"

static void check_array_element_type(const ParserContext &pc, const Type &expected, Type actual) {
	if (actual.type_kind == TK_Null &&
		(expected.type_kind == TK_Array || expected.type_kind == TK_Object ||
			expected.type_str == "string"))
		return;

	while (actual.type_kind == TK_Object && actual.type_str != expected.type_str)
		actual = pc.object_data.at(actual.data.object_data).parent_type;

	if (expected.type_str != actual.type_str)
		panic("Array element type mismatch: expected " + expected.type_str + ".", pc.tc);
}

void SemanticsContext::check_value(ParserContext &pc,
	Node &node,
	const Type &expected,
	bool strict) {
	if (node.type == AST_ArrayDeclaration) {
		if (expected.type_kind != TK_Array)
			panic("Array literal requires an array target.", pc.tc);
		Token &type_tok = std::get<ArrayDeclAST>(node.ast).ele_type_tok;
		type_tok.type = TokenType::TokIdent;
		type_tok.str = expected.type_str;
	}
	check_semantics(pc, node);
	if (strict || expected.type_kind == TK_Array)
		check_array_element_type(pc, expected, infer_type(pc, node));
}

void SemanticsContext::check_array_literal(ParserContext &pc, ArrayDeclAST &array_ast) {
	if (array_ast.ele_type_tok.str.empty())
		panic("Array literal requires a target type.", pc.tc);

	const Type element = array_element_type(pc, find_type(pc, array_ast.ele_type_tok.str));
	for (Node &value : array_ast.elements) {
		check_value(pc, value, element, true);
	}
}

void SemanticsContext::check_array_access(ParserContext &pc, Node &node) {
	ArrayAccessAST &access = std::get<ArrayAccessAST>(node.ast);
	check_child(pc, access.target_array);
	Type type = infer_type(pc, pc.get_node(access.target_array));
	for (Node &index : access.indexes) {
		if (type.type_kind != TK_Array)
			panic("Indexing requires an array value.", pc.tc);
		check_semantics(pc, index);
		if (infer_type(pc, index).type_str != "int")
			panic("Array index must be int.", pc.tc);
		type = array_element_type(pc, type);
	}
	resolve_attr(pc, type, node);
}

void SemanticsContext::bind_array_attr(ParserContext &pc, const Type &array_type, Node &node) {
	if (node.type == AST_Identifier) {
		IdentifierAST &ident = std::get<IdentifierAST>(node.ast);
		if (ident.ident.str != "length" || node.attr != NoNode)
			panic("Unknown array attribute.", pc.tc);
		ident.is_array_length = true;
		ident.var_data = NoNode;
		return;
	}
	if (node.type != AST_FunctionCall)
		panic("Invalid array attribute.", pc.tc);

	FuncCallAST &call = std::get<FuncCallAST>(node.ast);
	const bool push = call.func_name_tok.str == "push";
	if (!push && call.func_name_tok.str != "remove")
		panic("Unknown array method.", pc.tc);
	if (call.params.size() != 1 || node.attr != NoNode)
		panic("Array method requires one argument and has no return value.", pc.tc);

	call.is_array = true;
	call.is_syscall = false;
	call.is_attr = true;
	call.func_data = NoNode;
	const Type argument = push ? array_element_type(pc, array_type) : find_type(pc, "int");
	check_value(pc, call.params[0], argument, true);
}
