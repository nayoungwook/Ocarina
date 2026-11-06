#include "ir.h"
#include "error.h"

void IRContext::gen_array_decl_ir(const ArrayDeclAST &array_ast) {
	const Type array_type = find_type(pc, array_ast.ele_type_tok.str);
	for (const Node &element : array_ast.elements)
		gen_node_ir(element, true);
	emit_byte(OP_NEW_ARRAY);
	emit_int(array_element_type(pc, array_type).nbyte);
	emit_int(static_cast<unsigned>(array_ast.elements.size()));
}

void IRContext::gen_array_access_ir(const Node &node, bool load_value) {
	const ArrayAccessAST &access = std::get<ArrayAccessAST>(node.ast);
	Type type = semantics.infer_type(pc, pc.get_node(access.target_array));
	gen_child_ir(access.target_array);
	for (std::size_t i = 0; i < access.indexes.size(); ++i) {
		type = array_element_type(pc, type);
		gen_node_ir(access.indexes[i], true);
		if (load_value || i + 1 < access.indexes.size()) {
			emit_byte(OP_ARRAY_LOAD);
			emit_int(type.nbyte);
		}
	}
}

void IRContext::gen_array_call_ir(const FuncCallAST &call, bool value_required) {
	if (value_required)
		panic("Array method has no return value.", pc.tc);
	for (const Node &param : call.params)
		gen_node_ir(param, true);
	const bool push = call.func_name_tok.str == "push";
	emit_byte(push ? OP_ARRAY_PUSH : OP_ARRAY_REMOVE);
	emit_int(push ? 0 : 1);
	emit_int(static_cast<unsigned>(call.params.size()));
}
