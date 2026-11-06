#include "semantics.h"
#include "error.h"

#include <algorithm>
#include <limits>

static unsigned add_size(const ParserContext &pc, unsigned offset, unsigned size) {
	if (size > std::numeric_limits<unsigned>::max() - offset) panic("Storage size overflow.", pc.tc);
	return offset + size;
}

void SemanticsContext::register_objects(ParserContext &pc) {
	for (Node &node : pc.nodes) {
		if (node.type != AST_Object) continue;
		ObjectAST &object_ast = std::get<ObjectAST>(node.ast);
		const std::string &object_name = object_ast.name_tok.str;
		if (pc.primitive_type_smtb.contains(object_name)) panic("Object name conflicts with a primitive type: " + object_name, pc.tc);
		for (const ObjectData &data : pc.object_data) {
			if (data.object_type.type_str == object_name) panic("Duplicate object: " + object_name, pc.tc);
		}
		object_ast.object_data = pc.object_data.size();
		ObjectData data = {};
		data.id = static_cast<unsigned>(object_ast.object_data + 1);
		data.object_type = gen_object_type(object_name);
		data.object_type.data.object_data = object_ast.object_data;
		pc.object_type_smtb[object_name] = data.object_type;
		pc.object_data.push_back(std::move(data));
	}
	for (Node &node : pc.nodes) {
		if (node.type != AST_Object) continue;
		ObjectAST &object_ast = std::get<ObjectAST>(node.ast);
		if (object_ast.parent_name_tok.type == TokenType::TokEOF) continue;
		Type parent_type = find_type(pc, object_ast.parent_name_tok.str);
		if (parent_type.type_kind != TK_Object) panic("Parent must be an object.", pc.tc);
		pc.object_data.at(object_ast.object_data).parent_type = std::move(parent_type);
	}
}

std::size_t SemanticsContext::register_func_data(ParserContext &pc, const Token &func_name,
	const Type &ret_type, NodeId params, std::size_t object_id, bool is_constructor) {
	FuncData data = {};
	data.symbol_id = pc.func_data.size();
	data.object_data = object_id;
	data.func_name = func_name.str;
	data.return_type = ret_type;
	data.is_constructor = is_constructor;
	data.scope_data = object_id == NoNode ? ScopeGlobal : ScopeObject;
	const VarDeclBundleAST &params_ast = std::get<VarDeclBundleAST>(pc.get_node(params).ast);
	for (const Node &param : params_ast.var_decls) {
		const VarDeclAST &param_ast = std::get<VarDeclAST>(param.ast);
		Type type = find_type(pc, param_ast.var_type_tok.str);
		if (type.type_str == "void") panic("Parameter cannot have void type.", pc.tc);
		data.arg_types.push_back(std::move(type));
	}
	data.arg_count = static_cast<unsigned>(data.arg_types.size());
	if (object_id == NoNode) {
		if (pc.glob_func_smtb.contains(data.func_name) || pc.syscall_smtb.contains(data.func_name))
			panic("Duplicate function: " + data.func_name, pc.tc);
		data.id = static_cast<unsigned>(pc.glob_func_smtb.size() + 1);
		pc.glob_func_smtb[data.func_name] = data;
	}
	else {
		ObjectData &object_data = pc.object_data.at(object_id);
		if (object_data.member_funcs.contains(data.func_name)) panic("Duplicate member function: " + data.func_name, pc.tc);
		data.id = static_cast<unsigned>(object_data.member_funcs.size() + 1);
		object_data.member_funcs[data.func_name] = data;
		if (is_constructor) {
			if (object_data.constructor.is_constructor) panic("Duplicate constructor.", pc.tc);
			if (data.func_name != object_data.object_type.type_str) panic("Constructor name must match the object.", pc.tc);
			object_data.constructor = data;
		}
	}
	pc.func_data.push_back(data);
	return data.symbol_id;
}

std::size_t SemanticsContext::register_non_local_var_data(ParserContext &pc, VarDeclAST &var_decl, std::size_t object_id) {
	VarData data = {};
	data.symbol_id = pc.var_data.size();
	data.var_name = var_decl.var_name_tok.str;
	data.type = find_type(pc, var_decl.var_type_tok.str);
	if (data.type.type_str == "void") panic("Variable cannot have void type.", pc.tc);
	if (object_id == NoNode) {
		if (pc.glob_var_smtb.contains(data.var_name)) panic("Duplicate global variable: " + data.var_name, pc.tc);
		data.scope_data = ScopeGlobal;
		data.id = static_cast<unsigned>(pc.glob_var_smtb.size() + 1);
		data.offset = global_offset;
		global_offset = add_size(pc, global_offset, data.type.nbyte);
		pc.glob_var_smtb[data.var_name] = data;
	}
	else {
		ObjectData &object_data = pc.object_data.at(object_id);
		if (object_data.member_vars.contains(data.var_name)) panic("Duplicate member variable: " + data.var_name, pc.tc);
		data.scope_data = ScopeObject;
		data.id = static_cast<unsigned>(object_data.member_vars.size() + 1);
		object_data.member_vars[data.var_name] = data;
	}
	pc.var_data.push_back(data);
	return data.symbol_id;
}

void SemanticsContext::register_data(ParserContext &pc, Node &node, std::size_t object_id) {
	switch (node.type) {
	case AST_Object: {
		ObjectAST &object_ast = std::get<ObjectAST>(node.ast);
		if (object_id != NoNode) panic("Nested object declarations are not supported.", pc.tc);
		for (Node &member : object_ast.body) register_data(pc, member, object_ast.object_data);
		break;
	}
	case AST_FunctionDeclaration: {
		FuncDeclAST &func_decl_ast = std::get<FuncDeclAST>(node.ast);
		func_decl_ast.func_data = register_func_data(pc, func_decl_ast.func_name_tok,
			find_type(pc, func_decl_ast.ret_type_tok.str), func_decl_ast.params, object_id, false);
		break;
	}
	case AST_Constructor: {
		if (object_id == NoNode) panic("Constructor must be declared in an object.", pc.tc);
		ConstructorAST &constructor_ast = std::get<ConstructorAST>(node.ast);
		constructor_ast.object_data = object_id;
		constructor_ast.func_data = register_func_data(pc, constructor_ast.func_name,
			find_type(pc, "void"), constructor_ast.params, object_id, true);
		break;
	}
	case AST_VariableDeclarationBundle:
		for (Node &var_decl : std::get<VarDeclBundleAST>(node.ast).var_decls) register_data(pc, var_decl, object_id);
		break;
	case AST_VariableDeclaration: {
		VarDeclAST &var_decl = std::get<VarDeclAST>(node.ast);
		var_decl.var_data = register_non_local_var_data(pc, var_decl, object_id);
		break;
	}
	default:
		panic("Top level and object bodies require declarations.", pc.tc);
	}
}

void SemanticsContext::layout_object(ParserContext &pc, std::size_t object_id, std::vector<unsigned> &state) {
	if (state.at(object_id) == 2) return;
	if (state[object_id] == 1) panic("Cyclic object inheritance.", pc.tc);
	state[object_id] = 1;
	ObjectData &object_data = pc.object_data.at(object_id);
	unsigned offset = 0;
	if (!object_data.parent_type.type_str.empty()) {
		std::size_t parent_id = object_data.parent_type.data.object_data;
		layout_object(pc, parent_id, state);
		offset = pc.object_data.at(parent_id).instance_size;
	}
	// Symbol IDs follow declaration order; hash-table iteration does not define layout.
	for (VarData &var_data : pc.var_data) {
		auto member = object_data.member_vars.find(var_data.var_name);
		if (member == object_data.member_vars.end() || member->second.symbol_id != var_data.symbol_id) continue;
		var_data.offset = offset;
		offset = add_size(pc, offset, var_data.type.nbyte);
		member->second = var_data;
	}
	object_data.instance_size = offset;

	unsigned next_function_id = 1;
	Type parent = object_data.parent_type;
	while (!parent.type_str.empty()) {
		const ObjectData &base = pc.object_data.at(parent.data.object_data);
		for (const auto &[name, function] : base.member_funcs) {
			if (!function.is_constructor)
				next_function_id = std::max(next_function_id, function.id + 1);
		}
		parent = base.parent_type;
	}

	for (FuncData &function : pc.func_data) {
		if (function.scope_data != ScopeObject || function.object_data != object_id)
			continue;

		unsigned inherited_id = 0;
		parent = object_data.parent_type;
		while (!function.is_constructor && !parent.type_str.empty()) {
			const ObjectData &base = pc.object_data.at(parent.data.object_data);
			auto inherited = base.member_funcs.find(function.func_name);
			if (inherited != base.member_funcs.end() && !inherited->second.is_constructor) {
				inherited_id = inherited->second.id;
				break;
			}
			parent = base.parent_type;
		}
		if (function.is_constructor)
			function.id = 0;
		else if (inherited_id != 0)
			function.id = inherited_id;
		else
			function.id = next_function_id++;
		object_data.member_funcs[function.func_name] = function;
		if (function.is_constructor)
			object_data.constructor = function;
	}
	state[object_id] = 2;
}

void SemanticsContext::open_scope() {
	Scope scope = {};
	scope.prev_scope = current_scope;
	current_scope = scopes.size();
	scopes.push_back(std::move(scope));
}

void SemanticsContext::close_scope() {
	current_scope = scopes.at(current_scope).prev_scope;
}

void SemanticsContext::register_local_var_data(ParserContext &pc, VarDeclAST &var_decl) {
	Scope &scope = scopes.at(current_scope);
	const std::string &name = var_decl.var_name_tok.str;
	if (scope.local_var_smtb.contains(name)) panic("Duplicate local variable: " + name, pc.tc);
	VarData data = {};
	data.symbol_id = pc.var_data.size();
	data.id = static_cast<unsigned>(scope.local_var_smtb.size() + 1);
	data.var_name = name;
	data.type = find_type(pc, var_decl.var_type_tok.str);
	if (data.type.type_str == "void") panic("Variable cannot have void type.", pc.tc);
	data.scope_data = ScopeLocal;
	data.offset = stack_offset;
	stack_offset = add_size(pc, stack_offset, data.type.nbyte);
	var_decl.var_data = data.symbol_id;
	scope.local_var_smtb[name] = data;
	declared_local_vars.push_back(data.symbol_id);
	pc.var_data.push_back(std::move(data));
}

std::size_t SemanticsContext::find_member_var_data(const ParserContext &pc, std::size_t attr_of, const std::string &key) const {
	while (attr_of != NoNode) {
		const ObjectData &object_data = pc.object_data.at(attr_of);
		auto var_data = object_data.member_vars.find(key);
		if (var_data != object_data.member_vars.end()) return var_data->second.symbol_id;
		attr_of = object_data.parent_type.type_str.empty() ? NoNode : object_data.parent_type.data.object_data;
	}
	return NoNode;
}

std::size_t SemanticsContext::find_member_func_data(const ParserContext &pc, std::size_t attr_of, const std::string &key) const {
	while (attr_of != NoNode) {
		const ObjectData &object_data = pc.object_data.at(attr_of);
		auto func_data = object_data.member_funcs.find(key);
		if (func_data != object_data.member_funcs.end()) return func_data->second.symbol_id;
		attr_of = object_data.parent_type.type_str.empty() ? NoNode : object_data.parent_type.data.object_data;
	}
	return NoNode;
}

std::size_t SemanticsContext::find_var_data(const ParserContext &pc, const std::string &var_name) const {
	std::size_t scope_searcher = current_scope;
	while (scope_searcher != NoNode) {
		const Scope &scope = scopes.at(scope_searcher);
		auto result = scope.local_var_smtb.find(var_name);
		if (result != scope.local_var_smtb.end()) return result->second.symbol_id;
		scope_searcher = scope.prev_scope;
	}
	std::size_t member = find_member_var_data(pc, current_object, var_name);
	if (member != NoNode) return member;
	auto result = pc.glob_var_smtb.find(var_name);
	return result == pc.glob_var_smtb.end() ? NoNode : result->second.symbol_id;
}

std::size_t SemanticsContext::find_func_data(const ParserContext &pc, const std::string &func_name) const {
	std::size_t member = find_member_func_data(pc, current_object, func_name);

	if (member != NoNode) return member;
	auto result = pc.glob_func_smtb.find(func_name);

	if (result != pc.glob_func_smtb.end()) return result->second.symbol_id;
	return NoNode;
}

void SemanticsContext::check_child(ParserContext &pc, NodeId id) {
	if (id != NoNode) check_semantics(pc, pc.get_node(id));
}

void SemanticsContext::check_semantics_body(ParserContext &pc, std::vector<Node> &body) {
	open_scope();
	for (Node &node : body) check_semantics(pc, node);
	close_scope();
}

void SemanticsContext::check_function(ParserContext &pc, NodeId params, std::vector<Node> &body,
	std::size_t func_id, std::vector<std::size_t> &declared_vars) {
	if (current_func != NoNode) panic("Nested function declarations are not supported.", pc.tc);
	current_func = func_id;
	stack_offset = 0;
	declared_local_vars.clear();
	open_scope();
	VarDeclBundleAST &params_ast = std::get<VarDeclBundleAST>(pc.get_node(params).ast);
	for (Node &param : params_ast.var_decls) register_local_var_data(pc, std::get<VarDeclAST>(param.ast));
	for (Node &node : body) check_semantics(pc, node);
	close_scope();
	declared_vars = declared_local_vars;
	current_func = NoNode;
}

void SemanticsContext::resolve_attr(ParserContext &pc, const Type &type_of_node, Node &node) {
	if (node.attr == NoNode) return;
	if (type_of_node.type_kind == TK_Array) {
		bind_array_attr(pc, type_of_node, pc.get_node(node.attr));
		return;
	}
	if (type_of_node.type_kind != TK_Object) panic("Attribute access requires an object type.", pc.tc);
	register_attr_data(pc, type_of_node.data.object_data, pc.get_node(node.attr));
}

void SemanticsContext::register_attr_data(ParserContext &pc, std::size_t attr_of, Node &node) {
	Type type_of_node;
	if (node.type == AST_Identifier) {
		IdentifierAST &ident_ast = std::get<IdentifierAST>(node.ast);
		ident_ast.var_data = find_member_var_data(pc, attr_of, ident_ast.ident.str);
		if (ident_ast.var_data == NoNode) panic("Unknown member variable: " + ident_ast.ident.str, pc.tc);
		ident_ast.is_attr = true;
		type_of_node = pc.var_data.at(ident_ast.var_data).type;
	}
	else if (node.type == AST_FunctionCall) {
		FuncCallAST &func_call_ast = std::get<FuncCallAST>(node.ast);
		func_call_ast.func_data = find_member_func_data(pc, attr_of, func_call_ast.func_name_tok.str);
		if (func_call_ast.func_data == NoNode) panic("Unknown member function: " + func_call_ast.func_name_tok.str, pc.tc);
		func_call_ast.is_attr = true;
		func_call_ast.is_syscall = false;
		const FuncData &func_data = pc.func_data.at(func_call_ast.func_data);
		if (func_data.is_constructor) panic("Constructor requires new.", pc.tc);
		if (func_data.arg_count != func_call_ast.params.size()) panic("Wrong member argument count.", pc.tc);
		for (std::size_t i = 0; i < func_call_ast.params.size(); ++i)
			check_value(pc, func_call_ast.params[i], func_data.arg_types.at(i));
		type_of_node = func_data.return_type;
	}
	else panic("Unsupported attribute node.", pc.tc);
	resolve_attr(pc, type_of_node, node);
}

void SemanticsContext::check_semantics(ParserContext &pc, Node &node) {
	switch (node.type) {
	case AST_Object: {
		ObjectAST &object_ast = std::get<ObjectAST>(node.ast);
		if (current_func != NoNode || object_ast.object_data == NoNode) panic("Nested object declarations are not supported.", pc.tc);
		current_object = object_ast.object_data;
		for (Node &member : object_ast.body) check_semantics(pc, member);
		current_object = NoNode;
		break;
	}
	case AST_FunctionDeclaration: {
		FuncDeclAST &func_decl_ast = std::get<FuncDeclAST>(node.ast);
		check_function(pc, func_decl_ast.params, func_decl_ast.body, func_decl_ast.func_data, func_decl_ast.declared_vars);
		break;
	}
	case AST_Constructor: {
		ConstructorAST &constructor_ast = std::get<ConstructorAST>(node.ast);
		check_function(pc, constructor_ast.params, constructor_ast.body, constructor_ast.func_data, constructor_ast.declared_vars);
		break;
	}
	case AST_VariableDeclarationBundle:
		for (Node &var_decl : std::get<VarDeclBundleAST>(node.ast).var_decls) check_semantics(pc, var_decl);
		break;
	case AST_VariableDeclaration: {
		VarDeclAST &var_decl = std::get<VarDeclAST>(node.ast);
		if (var_decl.decl != NoNode)
			check_value(pc, pc.get_node(var_decl.decl), find_type(pc, var_decl.var_type_tok.str));
		if (current_func != NoNode) register_local_var_data(pc, var_decl);
		break;
	}
	case AST_Identifier: {
		IdentifierAST &ident_ast = std::get<IdentifierAST>(node.ast);
		ident_ast.var_data = find_var_data(pc, ident_ast.ident.str);
		if (ident_ast.var_data == NoNode) panic("Unknown variable: " + ident_ast.ident.str, pc.tc);
		resolve_attr(pc, pc.var_data.at(ident_ast.var_data).type, node);
		break;
	}
	case AST_FunctionCall: {
		FuncCallAST &func_call_ast = std::get<FuncCallAST>(node.ast);
		func_call_ast.func_data = find_func_data(pc, func_call_ast.func_name_tok.str);
		func_call_ast.is_syscall = func_call_ast.func_data == NoNode &&
			pc.syscall_smtb.contains(func_call_ast.func_name_tok.str);

		if (func_call_ast.func_data == NoNode && !func_call_ast.is_syscall)
			panic("Unknown function: " + func_call_ast.func_name_tok.str, pc.tc);

		const FuncData &func_data = pc.get_func_data(func_call_ast);
		if (func_data.is_constructor) panic("Constructor requires new.", pc.tc);
		if (!func_data.varargs && func_data.arg_count != func_call_ast.params.size()) panic("Wrong function argument count.", pc.tc);
		for (std::size_t i = 0; i < func_call_ast.params.size(); ++i) {
			if (func_data.varargs)
				check_semantics(pc, func_call_ast.params[i]);
			else
				check_value(pc, func_call_ast.params[i], func_data.arg_types.at(i));
		}

		if (func_call_ast.is_syscall)
			check_syscall(pc, func_call_ast);

		resolve_attr(pc, func_data.return_type, node);
		break;
	}
	case AST_BinExpr: {
		BinExprAST &bin_expr_ast = std::get<BinExprAST>(node.ast);
		check_child(pc, bin_expr_ast.left);
		if (bin_expr_ast.op_type == OpASSIGN) {
			const Node &left = pc.get_node(bin_expr_ast.left);
			if (left.type != AST_Identifier && left.type != AST_ArrayAccess)
				panic("Assignment requires a variable, member or array element.", pc.tc);
			check_value(pc, pc.get_node(bin_expr_ast.right), infer_type(pc, left),
				left.type == AST_ArrayAccess && left.attr == NoNode);
		} else
			check_child(pc, bin_expr_ast.right);
		break;
	}
	case AST_UnaryExpr: check_child(pc, std::get<UnaryExprAST>(node.ast).expr); break;
	case AST_Negative: check_child(pc, std::get<NegAST>(node.ast).ast); break;
	case AST_IdentIncrease: check_child(pc, std::get<IdentIncreAST>(node.ast).ident_node); break;
	case AST_IdentDecrease: check_child(pc, std::get<IdentDecreAST>(node.ast).ident_node); break;
	case AST_Return: {
		if (current_func == NoNode) panic("Return outside function.", pc.tc);
		NodeId expr = std::get<ReturnAST>(node.ast).expr;
		if (expr != NoNode)
			check_value(pc, pc.get_node(expr), pc.func_data.at(current_func).return_type);
		break;
	}
	case AST_IfStatement: {
		IfStmtAST &if_stmt_ast = std::get<IfStmtAST>(node.ast);
		check_child(pc, if_stmt_ast.cond);
		check_semantics_body(pc, if_stmt_ast.body);
		check_child(pc, if_stmt_ast.next_stmt);
		break;
	}
	case AST_ForStatement: {
		ForStmtAST &for_stmt_ast = std::get<ForStmtAST>(node.ast);
		open_scope();
		check_child(pc, for_stmt_ast.init);
		check_child(pc, for_stmt_ast.cond);
		check_child(pc, for_stmt_ast.step);
		check_semantics_body(pc, for_stmt_ast.body);
		close_scope();
		break;
	}
	case AST_New: {
		NewAST &new_ast = std::get<NewAST>(node.ast);
		Type type = find_type(pc, new_ast.name_tok.str);
		if (type.type_kind != TK_Object) panic("new requires an object.", pc.tc);
		new_ast.object_data = type.data.object_data;
		const FuncData &constructor = pc.object_data.at(new_ast.object_data).constructor;
		if (new_ast.params.size() != (constructor.is_constructor ? constructor.arg_count : 0))
			panic("Wrong constructor argument count.", pc.tc);
		for (std::size_t i = 0; i < new_ast.params.size(); ++i)
			check_value(pc, new_ast.params[i], constructor.arg_types.at(i));
		resolve_attr(pc, type, node);
		break;
	}
	case AST_ArrayDeclaration:
		check_array_literal(pc, std::get<ArrayDeclAST>(node.ast));
		resolve_attr(pc, find_type(pc, std::get<ArrayDeclAST>(node.ast).ele_type_tok.str), node);
		break;
	case AST_ArrayAccess:
		check_array_access(pc, node);
		break;
	case AST_None: case AST_NumberLiteral: case AST_StringLiteral: case AST_BoolLiteral: case AST_Null:
		break;
	default: panic("Unsupported AST in semantic preparation.", pc.tc);
	}
}

Type SemanticsContext::infer_type(const ParserContext &pc, const Node &node) const {
	if (node.attr != NoNode) return infer_type(pc, pc.get_node(node.attr));
	switch (node.type) {
	case AST_NumberLiteral: return std::get<NumberLiteralAST>(node.ast).type;
	case AST_StringLiteral: return find_type(pc, "string");
	case AST_BoolLiteral: return find_type(pc, "bool");
	case AST_Null: return find_type(pc, "null");
	case AST_Identifier: {
		const IdentifierAST &ident = std::get<IdentifierAST>(node.ast);
		return ident.is_array_length ? find_type(pc, "int") : pc.var_data.at(ident.var_data).type;
	}
	case AST_FunctionCall: {
		const FuncCallAST &call = std::get<FuncCallAST>(node.ast);
		return call.is_array ? find_type(pc, "void") : pc.get_func_data(call).return_type;
	}
	case AST_ArrayDeclaration:
		return find_type(pc, std::get<ArrayDeclAST>(node.ast).ele_type_tok.str);
	case AST_ArrayAccess: {
		const ArrayAccessAST &access = std::get<ArrayAccessAST>(node.ast);
		Type type = infer_type(pc, pc.get_node(access.target_array));
		for (std::size_t i = 0; i < access.indexes.size(); ++i)
			type = array_element_type(pc, type);
		return type;
	}
	case AST_New:
		return pc.object_data.at(std::get<NewAST>(node.ast).object_data).object_type;
	case AST_IdentIncrease: return infer_type(pc, pc.get_node(std::get<IdentIncreAST>(node.ast).ident_node));
	case AST_IdentDecrease: return infer_type(pc, pc.get_node(std::get<IdentDecreAST>(node.ast).ident_node));
	case AST_UnaryExpr: return find_type(pc, "bool");
	case AST_Negative: return infer_type(pc, pc.get_node(std::get<NegAST>(node.ast).ast));
	case AST_BinExpr: {
		const BinExprAST &bin_expr_ast = std::get<BinExprAST>(node.ast);
		if (bin_expr_ast.op_type >= OpEQUAL && bin_expr_ast.op_type != OpASSIGN) return find_type(pc, "bool");
		return infer_type(pc, pc.get_node(bin_expr_ast.left));
	}
	default: panic("Cannot infer expression type.", pc.tc);
	}
}

void SemanticsContext::resolve_semantics(ParserContext &pc) {
	scopes.clear(); current_scope = NoNode; current_object = NoNode; current_func = NoNode;
	stack_offset = 0; global_offset = 0;
	pc.glob_var_smtb.clear(); pc.glob_func_smtb.clear(); pc.object_type_smtb.clear();
	pc.object_data.clear(); pc.func_data.clear(); pc.var_data.clear();

	register_objects(pc);

	for (Node &node : pc.nodes) register_data(pc, node, NoNode);
	
	std::vector<unsigned> state(pc.object_data.size(), 0);
	
	for (std::size_t i = 0; i < pc.object_data.size(); ++i) layout_object(pc, i, state);
	
	for (Node &node : pc.nodes) check_semantics(pc, node);
}

