#include "semantics.h"
#include "error.h"

void SemanticsContext::check_syscall(ParserContext &pc, const FuncCallAST &func_call_ast) {
	const FuncData &func_data = pc.get_func_data(func_call_ast);

	if (func_data.func_name == "vector") {
		if (func_call_ast.params.size() > 3)
			panic("vector accepts at most three components.", pc.tc);

		for (const Node &param : func_call_ast.params) {
			Type type = infer_type(pc, param);

			if (type.type_kind != TK_Numeric && type.type_str != "bool" && type.type_str != "char")
				panic("vector components must be numeric.", pc.tc);
		}
	} else if (func_data.func_name == "window") {
		for (std::size_t i = 0; i < func_data.arg_types.size(); ++i) {
			Type type = infer_type(pc, func_call_ast.params.at(i));

			if (type.type_str != func_data.arg_types[i].type_str)
				panic("window expects (string, int, int).", pc.tc);
		}
	}
}
