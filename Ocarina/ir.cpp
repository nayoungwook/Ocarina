#include "ir.h"
#include "error.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

void IRContext::init_irc() {
	bytes.clear();
	bytes.reserve(BYTE_CHUNK);
	label_id = 0;
	str_rodata.clear();
	function_layouts.clear();
	object_layouts.clear();
	current_object = NoNode; current_func = NoNode; return_label = NoNode;
}

void IRContext::emit_byte(byte _b) {
	if (bytes.size() == std::numeric_limits<std::uint32_t>::max()) panic("IR byte buffer overflow.", pc.tc);
	bytes.push_back(_b);
}

void IRContext::emit_str(const std::string &str) {
	if (str.find('\0') != std::string::npos) panic("Embedded null cannot be stored in an IR string.", pc.tc);
	for (unsigned char ch : str) emit_byte(ch);
	emit_byte(0);
}

void IRContext::emit_int(std::uint32_t si) {
	for (unsigned i = 0; i < 4; ++i) {
		emit_byte(static_cast<byte>(si & 0xff));
		si >>= 8;
	}
}

void IRContext::emit_float(float f) {
	emit_int(std::bit_cast<std::uint32_t>(f));
}

void IRContext::emit_double(double f) {
	std::uint64_t fb = std::bit_cast<std::uint64_t>(f);
	for (unsigned i = 0; i < 8; ++i) {
		emit_byte(static_cast<byte>(fb & 0xff));
		fb >>= 8;
	}
}

void IRContext::gen_func_metadata(const FuncData &fd) {
	emit_byte(fd.is_constructor ? META_CONSTRUCTOR : META_FUNC);
	emit_int(fd.id);
	emit_str(fd.func_name);
	if (!fd.is_constructor) emit_str(fd.return_type.type_str);
}

void IRContext::gen_var_metadata(const VarData &vd) {
	emit_byte(META_VAR);
	emit_int(vd.id);
	emit_str(vd.var_name);
	emit_str(vd.type.type_str);
}

void IRContext::gen_object_metadata(const ObjectData &cd) {
	VMObjectData layout;
	layout.id = cd.id;
	layout.name = cd.object_type.type_str;
	layout.size = cd.instance_size;
	layout.layout_known = true;
	layout.parent_resolved = true;
	if (!cd.parent_type.type_str.empty())
		layout.parent_id = pc.object_data.at(cd.parent_type.data.object_data).id;

	emit_byte(META_OBJECT);
	emit_int(cd.id);
	emit_str(cd.object_type.type_str);
	for (const FuncData &fd : pc.func_data) {
		if (fd.scope_data == ScopeObject && pc.object_data.at(fd.object_data).id == cd.id) gen_func_metadata(fd);
	}
	for (const VarData &vd : pc.var_data) {
		auto member = cd.member_vars.find(vd.var_name);
		if (member != cd.member_vars.end() && member->second.symbol_id == vd.symbol_id) {
			gen_var_metadata(vd);
			VMVariableData variable;
			variable.id = vd.id;
			variable.name = vd.var_name;
			variable.type = vd.type.type_str;
			variable.offset = vd.offset;
			variable.size = vd.type.nbyte;
			variable.operand_type = vm_operand_type(variable.type);
			variable.layout_known = true;
			layout.variable_data[vd.id] = std::move(variable);
		}
	}
	emit_byte(META_TERM);
	object_layouts[layout.name] = std::move(layout);
}

void IRContext::gen_metadata() {
	emit_byte(META_BEGIN);
	for (const ObjectData &cd : pc.object_data) gen_object_metadata(cd);
	for (const FuncData &fd : pc.func_data) if (fd.scope_data == ScopeGlobal) gen_func_metadata(fd);
	for (const VarData &vd : pc.var_data) if (vd.scope_data == ScopeGlobal) gen_var_metadata(vd);
	emit_byte(META_END);
}

const std::vector<byte> &IRContext::get_bytes() const { return bytes; }

const std::map<std::pair<std::string, unsigned>, VMFunctionData> &IRContext::get_function_layouts() const {
	return function_layouts;
}

const std::map<std::string, VMObjectData> &IRContext::get_object_layouts() const {
	return object_layouts;
}

unsigned IRContext::get_total_stack_size_of_func(const std::vector<std::size_t> &declared_vars) const {
	unsigned stack_size = 0;
	for (std::size_t id : declared_vars) {
		const VarData &var_data = pc.var_data.at(id);
		if (var_data.type.nbyte > std::numeric_limits<unsigned>::max() - var_data.offset)
			panic("Function stack size overflow.", pc.tc);
		unsigned end = var_data.offset + var_data.type.nbyte;
		if (end > stack_size) stack_size = end;
	}
	return stack_size;
}

RODATA_Str IRContext::add_str_rodata(const std::string &str) {
	auto rodata_str = str_rodata.find(str);
	if (rodata_str != str_rodata.end()) return rodata_str->second;
	RODATA_Str result = { str, static_cast<unsigned>(str_rodata.size()) };
	str_rodata[str] = result;
	return result;
}

byte IRContext::get_op_byte(OperatorType op_type) const {
	switch (op_type) {
	case OpADD: return OP_ADD;
	case OpSUB: return OP_SUB;
	case OpMUL: return OP_MUL;
	case OpDIV: return OP_DIV;
	case OpEQUAL: return OP_EQUAL;
	case OpNOTEQUAL: return OP_NOTEQUAL;
	case OpGREATER: return OP_GREATER;
	case OpLESS: return OP_LESS;
	case OpEQUALGREATER: return OP_EQUALGREATER;
	case OpEQUALLESS: return OP_EQUALLESS;
	case OpASSIGN: return OP_ASSIGN;
	case OpOR: return OP_OR;
	case OpAND: return OP_AND;
	default: panic("Unknown IR operator.", pc.tc);
	}
}

void IRContext::gen_ident_load_ir(const VarData &var_data, bool is_attr) {
	if (is_attr) emit_byte(OP_LOAD_ATTR);
	else {
		switch (var_data.scope_data) {
		case ScopeLocal: emit_byte(OP_SP_LOAD); break;
		case ScopeGlobal: emit_byte(OP_LOAD_GLOBAL); break;
		case ScopeObject: emit_byte(OP_LOAD_OBJECT); break;
		default: panic("Invalid variable scope.", pc.tc);
		}
	}
	emit_int(var_data.offset);
	emit_int(var_data.type.nbyte);
}

void IRContext::gen_ident_save_ir(const VarData &var_data, bool is_attr) {
	if (is_attr) emit_byte(OP_SAVE_ATTR);
	else {
		switch (var_data.scope_data) {
		case ScopeLocal: emit_byte(OP_SP_SAVE); break;
		case ScopeGlobal: emit_byte(OP_SAVE_GLOBAL); break;
		case ScopeObject: emit_byte(OP_SAVE_OBJECT); break;
		default: panic("Invalid variable scope.", pc.tc);
		}
	}
	emit_int(var_data.offset);
	emit_int(var_data.type.nbyte);
}

void IRContext::gen_ident_incre_ir(const VarData &var_data, bool is_attr) {
	gen_ident_update_ir(var_data, is_attr, false);
}

void IRContext::gen_ident_decre_ir(const VarData &var_data, bool is_attr) {
	gen_ident_update_ir(var_data, is_attr, true);
}

void IRContext::gen_ident_update_ir(const VarData &var_data, bool is_attr, bool decrease) {
	if (var_data.type.type_kind != TK_Numeric) panic("Increment/decrement requires a numeric variable.", pc.tc);
	if (is_attr) emit_byte(decrease ? OP_DECRE_ATTR : OP_INCRE_ATTR);
	else {
		switch (var_data.scope_data) {
		case ScopeLocal: emit_byte(decrease ? OP_SP_DECRE : OP_SP_INCRE); break;
		case ScopeGlobal: emit_byte(decrease ? OP_DECRE_GLOBAL : OP_INCRE_GLOBAL); break;
		case ScopeObject: emit_byte(decrease ? OP_DECRE_OBJECT : OP_INCRE_OBJECT); break;
		default: panic("Invalid variable scope.", pc.tc);
		}
	}
	emit_int(var_data.offset);
	emit_int(var_data.type.nbyte);
}

void IRContext::gen_ident_ir(const Node &node, bool value_required) {
	const IdentifierAST &ident_ast = std::get<IdentifierAST>(node.ast);
	if (ident_ast.is_array_length) {
		emit_byte(OP_ARRAY_LENGTH);
		return;
	}
	gen_ident_load_ir(pc.var_data.at(ident_ast.var_data), ident_ast.is_attr);
	if (node.attr != NoNode) gen_child_ir(node.attr, value_required);
}

const Node &IRContext::gen_assignment_target(const Node &node) {
	if (node.attr == NoNode) {
		if (node.type != AST_Identifier && node.type != AST_ArrayAccess)
			panic("Assignment requires a variable, member or array element.", pc.tc);
		return node;
	}
	Node receiver = node;
	receiver.attr = NoNode;
	gen_node_ir(receiver, true);
	return gen_assignment_target(pc.get_node(node.attr));
}

void IRContext::gen_number_ir(const NumberLiteralAST &num_lit_ast, bool negative) {
	std::string str = num_lit_ast.num_tok.str;
	if (str.back() == 'f') str.pop_back();
	if (negative) str.insert(str.begin(), '-');
	if (num_lit_ast.type.data.numeric_data.is_integer) {
		std::int32_t value = 0;
		auto result = std::from_chars(str.data(), str.data() + str.size(), value);
		if (result.ec != std::errc{} || result.ptr != str.data() + str.size()) panic("Integer literal exceeds int32 range.", pc.tc);
		emit_byte(OP_LDC_I4);
		emit_int(static_cast<std::uint32_t>(value));
	}
	else if (num_lit_ast.type.nbyte == 4) {
		float value = 0;
		auto result = std::from_chars(str.data(), str.data() + str.size(), value);
		if (result.ec != std::errc{} || result.ptr != str.data() + str.size() || !std::isfinite(value)) panic("Invalid float literal.", pc.tc);
		emit_byte(OP_LDC_F4);
		emit_float(value);
	}
	else {
		double value = 0;
		auto result = std::from_chars(str.data(), str.data() + str.size(), value);
		if (result.ec != std::errc{} || result.ptr != str.data() + str.size() || !std::isfinite(value)) panic("Invalid double literal.", pc.tc);
		emit_byte(OP_LDC_F8);
		emit_double(value);
	}
}

void IRContext::gen_zero(const Type &type) {
	if (type.type_kind == TK_Object || type.type_kind == TK_Array || type.type_str == "string") emit_byte(OP_PUSH_NULL);
	else if (type.type_str == "float") { emit_byte(OP_LDC_F4); emit_float(0); }
	else if (type.type_str == "double") { emit_byte(OP_LDC_F8); emit_double(0); }
	else { emit_byte(OP_LDC_I4); emit_int(0); }
}

unsigned IRContext::next_label() {
	if (label_id == std::numeric_limits<unsigned>::max()) panic("IR label overflow.", pc.tc);
	return ++label_id;
}

void IRContext::emit_jump(byte op, unsigned label) {
	emit_byte(op); emit_int(label);
}

void IRContext::gen_child_ir(NodeId id, bool value_required) {
	if (id != NoNode) gen_node_ir(pc.get_node(id), value_required);
}

void IRContext::gen_function_ir(const FuncData &func_data, const std::vector<std::size_t> &declared_vars,
	const std::vector<Node> &body) {
	current_func = func_data.symbol_id;
	return_label = NoNode;
	unsigned total_stack_size = get_total_stack_size_of_func(declared_vars);

	VMFunctionData layout;
	layout.id = func_data.id;
	layout.stack_size = total_stack_size;
	layout.argument_count = func_data.arg_count;
	layout.metadata_complete = true;
	for (const Type &type : func_data.arg_types)
		layout.argument_types.push_back(type.type_str);
	for (std::size_t id : declared_vars) {
		const VarData &var_data = pc.var_data.at(id);
		layout.local_types[var_data.offset] = vm_operand_type(var_data.type.type_str);
	}

	const std::string owner = func_data.scope_data == ScopeObject ?
		pc.object_data.at(func_data.object_data).object_type.type_str : std::string{};
	function_layouts[{owner, func_data.id}] = std::move(layout);

	emit_byte(CODE_FUNC); emit_int(func_data.id);
	if (total_stack_size != 0) { emit_byte(OP_SP_PUSH); emit_int(total_stack_size); }
	for (const Node &node : body) gen_node_ir(node);
	if (return_label != NoNode) emit_jump(OP_LABEL, static_cast<unsigned>(return_label));
	if (total_stack_size != 0) { emit_byte(OP_SP_POP); emit_int(total_stack_size); }
	emit_byte(OP_RET);
	emit_byte(CODE_TERM);
	current_func = NoNode;
	return_label = NoNode;
}

void IRContext::gen_if_ir(const IfStmtAST &if_stmt_ast) {
	unsigned next_label_id = next_label();
	unsigned end_label_id = next_label();
	if (if_stmt_ast.cond != NoNode) {
		gen_child_ir(if_stmt_ast.cond);
		emit_jump(OP_JNE, next_label_id);
	}
	for (const Node &node : if_stmt_ast.body) gen_node_ir(node);
	if (if_stmt_ast.next_stmt != NoNode) emit_jump(OP_GOTO, end_label_id);
	emit_jump(OP_LABEL, next_label_id);
	gen_child_ir(if_stmt_ast.next_stmt, false);
	emit_jump(OP_LABEL, end_label_id);
}

void IRContext::gen_node_ir(const Node &node, bool value_required) {
	switch (node.type) {
	case AST_None: return;
	case AST_Null: emit_byte(OP_PUSH_NULL); break;
	case AST_NumberLiteral: gen_number_ir(std::get<NumberLiteralAST>(node.ast)); break;
	case AST_BoolLiteral:
		emit_byte(OP_LDC_I4);
		emit_int(std::get<BoolLiteralAST>(node.ast).bool_tok.type == TokenType::TokTrue ? 1 : 0);
		break;
	case AST_StringLiteral: {
		const StringLiteralAST &str_lit_ast = std::get<StringLiteralAST>(node.ast);
		RODATA_Str rodata = add_str_rodata(str_lit_ast.str_tok.str);
		emit_byte(OP_LOAD_STR); emit_int(rodata.id);
		break;
	}
	case AST_Identifier: gen_ident_ir(node, value_required); break;
	case AST_IdentIncrease:
	case AST_IdentDecrease: {
		NodeId id = node.type == AST_IdentIncrease ? std::get<IdentIncreAST>(node.ast).ident_node : std::get<IdentDecreAST>(node.ast).ident_node;
		const Node &ident_node = pc.get_node(id);
		if (value_required && ident_node.attr != NoNode) panic("Member increment cannot be used as a value in this IR.", pc.tc);
		const Node &target = gen_assignment_target(ident_node);
		const IdentifierAST &ident_ast = std::get<IdentifierAST>(target.ast);
		const VarData &var_data = pc.var_data.at(ident_ast.var_data);
		if (node.type == AST_IdentDecrease) gen_ident_decre_ir(var_data, ident_ast.is_attr);
		else gen_ident_incre_ir(var_data, ident_ast.is_attr);
		if (value_required) gen_ident_load_ir(var_data, false);
		break;
	}
	case AST_VariableDeclarationBundle:
		for (const Node &var_decl : std::get<VarDeclBundleAST>(node.ast).var_decls) gen_node_ir(var_decl);
		break;
	case AST_VariableDeclaration: {
		const VarDeclAST &var_decl_ast = std::get<VarDeclAST>(node.ast);
		const VarData &var_data = pc.var_data.at(var_decl_ast.var_data);
		if (var_decl_ast.decl != NoNode) gen_child_ir(var_decl_ast.decl);
		else gen_zero(var_data.type);
		gen_ident_save_ir(var_data, false);
		break;
	}
	case AST_BinExpr: {
		const BinExprAST &bin_expr_ast = std::get<BinExprAST>(node.ast);
		if (bin_expr_ast.op_type == OpASSIGN) {
			const Node &left = pc.get_node(bin_expr_ast.left);
			if (value_required && left.attr != NoNode) panic("Member assignment cannot be used as a value in this IR.", pc.tc);
			const Node &target = gen_assignment_target(left);
			if (target.type == AST_ArrayAccess) {
				if (value_required)
					panic("Array assignment cannot be used as a value.", pc.tc);
				gen_array_access_ir(target, false);
				gen_child_ir(bin_expr_ast.right);
				emit_byte(OP_ARRAY_SAVE);
				emit_int(semantics.infer_type(pc, target).nbyte);
				break;
			}
			const IdentifierAST &ident_ast = std::get<IdentifierAST>(target.ast);
			if (ident_ast.is_array_length)
				panic("Array length is read-only.", pc.tc);
			const VarData &var_data = pc.var_data.at(ident_ast.var_data);
			gen_child_ir(bin_expr_ast.right);
			gen_ident_save_ir(var_data, ident_ast.is_attr);
			if (value_required) gen_ident_load_ir(var_data, false);
		}
		else {
			gen_child_ir(bin_expr_ast.left); gen_child_ir(bin_expr_ast.right);
			emit_byte(OP_EXPR_OP); emit_byte(get_op_byte(bin_expr_ast.op_type));
		}
		break;
	}
	case AST_UnaryExpr:
		gen_child_ir(std::get<UnaryExprAST>(node.ast).expr);
		emit_byte(OP_LDC_I4); emit_int(0); emit_byte(OP_EXPR_OP); emit_byte(OP_EQUAL);
		break;
	case AST_Negative: {
		const Node &operand = pc.get_node(std::get<NegAST>(node.ast).ast);
		if (operand.type == AST_NumberLiteral) gen_number_ir(std::get<NumberLiteralAST>(operand.ast), true);
		else {
			Type type = semantics.infer_type(pc, operand);
			if (type.type_kind != TK_Numeric) panic("Negation requires a numeric expression.", pc.tc);
			gen_zero(type); gen_node_ir(operand, true); emit_byte(OP_EXPR_OP); emit_byte(OP_SUB);
		}
		break;
	}
	case AST_ForStatement: {
		const ForStmtAST &for_stmt_ast = std::get<ForStmtAST>(node.ast);
		gen_child_ir(for_stmt_ast.init, false);
		unsigned end_label_id = next_label(), begin_label_id = next_label();
		emit_jump(OP_GOTO, end_label_id); emit_jump(OP_LABEL, begin_label_id);
		for (const Node &body_node : for_stmt_ast.body) gen_node_ir(body_node);
		gen_child_ir(for_stmt_ast.step, false);
		emit_jump(OP_LABEL, end_label_id); gen_child_ir(for_stmt_ast.cond);
		emit_jump(OP_JE, begin_label_id);
		break;
	}
	case AST_IfStatement: gen_if_ir(std::get<IfStmtAST>(node.ast)); break;
	case AST_FunctionCall: {
		const FuncCallAST &func_call_ast = std::get<FuncCallAST>(node.ast);
		if (func_call_ast.is_array) {
			gen_array_call_ir(func_call_ast, value_required);
			break;
		}
		const FuncData &func_data = pc.get_func_data(func_call_ast);

		if (value_required && node.attr == NoNode && func_data.return_type.type_str == "void") panic("Void call cannot be used as a value.", pc.tc);

		for (const Node &param : func_call_ast.params) gen_node_ir(param, true);

		if (func_call_ast.is_attr) emit_byte(OP_CALL_ATTR);
		else if (func_data.scope_data == ScopeSyscall) emit_byte(OP_SYSCALL);
		else if (func_data.scope_data == ScopeObject) emit_byte(OP_CALL_OBJECT);
		else emit_byte(OP_CALL_GLOBAL);

		emit_int(func_data.id); emit_int(static_cast<unsigned>(func_call_ast.params.size()));
		if (node.attr != NoNode) gen_child_ir(node.attr, value_required);
		break;
	}
	case AST_Object: {
		const ObjectAST &object_ast = std::get<ObjectAST>(node.ast);
		current_object = object_ast.object_data;
		emit_byte(CODE_OBJECT); emit_int(pc.object_data.at(current_object).id);
		for (const Node &member : object_ast.body) gen_node_ir(member);
		emit_byte(CODE_TERM); current_object = NoNode;
		break;
	}
	case AST_FunctionDeclaration: {
		const FuncDeclAST &func_decl_ast = std::get<FuncDeclAST>(node.ast);
		gen_function_ir(pc.func_data.at(func_decl_ast.func_data), func_decl_ast.declared_vars, func_decl_ast.body);
		break;
	}
	case AST_Constructor: {
		const ConstructorAST &constructor_ast = std::get<ConstructorAST>(node.ast);
		gen_function_ir(pc.func_data.at(constructor_ast.func_data), constructor_ast.declared_vars, constructor_ast.body);
		break;
	}
	case AST_Return: {
		if (current_func == NoNode) panic("Return outside function.", pc.tc);
		NodeId expr = std::get<ReturnAST>(node.ast).expr;
		const Type &type = pc.func_data.at(current_func).return_type;
		if ((expr == NoNode) != (type.type_str == "void")) panic("Return value does not match void/non-void signature.", pc.tc);
		if (expr != NoNode) { gen_child_ir(expr); emit_byte(OP_RET_VAL); }
		if (return_label == NoNode) return_label = next_label();
		emit_jump(OP_GOTO, static_cast<unsigned>(return_label));
		break;
	}
	case AST_New: {
		const NewAST &new_ast = std::get<NewAST>(node.ast);
		const ObjectData &object = pc.object_data.at(new_ast.object_data);
		const FuncData &constructor = object.constructor;
		const unsigned argument_count = static_cast<unsigned>(new_ast.params.size());
		if (argument_count != (constructor.is_constructor ? constructor.arg_count : 0))
			panic("Wrong constructor argument count.", pc.tc);

		for (const Node &param : new_ast.params)
			gen_node_ir(param, true);

		emit_byte(OP_NEW_OBJECT);
		emit_int(object.id);
		emit_int(object.instance_size);
		emit_int(constructor.is_constructor ? constructor.id : UINT32_MAX);
		emit_int(argument_count);
		if (node.attr != NoNode)
			gen_child_ir(node.attr, value_required);
		break;
	}
	case AST_ArrayDeclaration:
		gen_array_decl_ir(std::get<ArrayDeclAST>(node.ast));
		if (node.attr != NoNode)
			gen_child_ir(node.attr, value_required);
		break;
	case AST_ArrayAccess:
		gen_array_access_ir(node);
		if (node.attr != NoNode)
			gen_child_ir(node.attr, value_required);
		break;
	}
}

void IRContext::gen_rodata() {
	emit_byte(RODATA_BEGIN);
	std::vector<RODATA_Str> ordered(str_rodata.size());
	for (const auto &entry : str_rodata) ordered.at(entry.second.id) = entry.second;
	for (const RODATA_Str &rodata_str : ordered) {
		emit_byte(RODATA_STR); emit_int(rodata_str.id); emit_str(rodata_str.str);
	}
	emit_byte(RODATA_END);
}

void IRContext::gen_ir(ParserContext input_pc) {
	init_irc();
	pc = std::move(input_pc);
	try {
		semantics.resolve_semantics(pc);
		gen_metadata();
		emit_byte(CODE_BEGIN);
		for (const Node &node : pc.nodes) gen_node_ir(node);
		emit_byte(CODE_END);
		gen_rodata();
	}
	catch (...) {
		init_irc();
		throw;
	}
}
