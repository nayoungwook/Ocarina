#include "parser.h"
#include "error.h"

#include <algorithm>
#include <utility>

void ParserContext::add_primitive_numeric(const std::string &name, unsigned nbyte, unsigned rank, bool is_signed, bool is_integer) {
	struct Type numeric_type = gen_numeric_type(name, nbyte, gen_numeric_data(rank, is_signed, is_integer));
	primitive_type_smtb[name] = numeric_type;
	numeric_types.push_back(std::move(numeric_type));
}

void ParserContext::init_primitive() {
	add_primitive_numeric("int", 4, 5, true, true);
	add_primitive_numeric("float", 4, 6, true, false);
	add_primitive_numeric("double", 8, 7, true, false);

	primitive_type_smtb["char"] = gen_primitive_type("char", 2);
	primitive_type_smtb["bool"] = gen_primitive_type("bool", 1);
	primitive_type_smtb["void"] = gen_primitive_type("void", 0);
	primitive_type_smtb["null"] = gen_null_type();
	primitive_type_smtb["string"] = gen_primitive_type("string", 8);
	primitive_type_smtb["vector"] = gen_primitive_type("vector", 4 * 3);
	primitive_type_smtb["window"] = gen_primitive_type("window", 8);
}

ParserContext::ParserContext() {
	init_primitive();
	init_syscall();
}

const FuncData &ParserContext::get_func_data(const FuncCallAST &func_call_ast) const {
	if (func_call_ast.is_syscall)
		return syscall_smtb.at(func_call_ast.func_name_tok.str);

	return func_data.at(func_call_ast.func_data);
}

void ParserContext::compile_file(TokenizerContext input_tc) {
	tc = std::move(input_tc);
	const std::size_t first = nodes.size();
	while (tc.peek().type != TokenType::TokEOF) {
		struct Node node = parse_stmt();
		nodes.push_back(std::move(node));
	}
	source_files.push_back({tc.source_name, first, nodes.size() - first, imports[tc.source_name]});
}

NodeId ParserContext::store_node(struct Node node) {
	if (node.type == AST_None) return NoNode;
	NodeId id = node_pool.size();
	node_pool.push_back(std::move(node));
	return id;
}

Token ParserContext::parse_type_token() {
	return tc.consume(TokenType::TokIdent);
}

bool ParserContext::is_variable_target(const Node &node) const {
	const Node *target = &node;
	while (target->attr != NoNode) target = &get_node(target->attr);
	return target->type == AST_Identifier;
}

const struct Node &ParserContext::get_node(NodeId id) const {
	return node_pool.at(id);
}

struct Node &ParserContext::get_node(NodeId id) {
	return node_pool.at(id);
}

struct ParamData ParserContext::parse_func_call_params() {
	struct ParamData param_data = {};
	tc.consume(TokenType::TokLParen);
	
	while (tc.peek().type != TokenType::TokRParen) {
		struct Node expr = parse_expression();
		param_data.params.push_back(std::move(expr));
		if (tc.peek().type == TokenType::TokRParen) break;
		tc.consume(TokenType::TokComma);
		if (tc.peek().type == TokenType::TokRParen) panic("Expected argument after comma.", tc);
	}

	tc.consume(TokenType::TokRParen);
	return param_data;
}

struct Node ParserContext::gen_func_call_node(const struct Token &first) {
	struct FuncCallAST func_call = {};

	struct ParamData param_data = parse_func_call_params();

	func_call.params = std::move(param_data.params);
	func_call.func_name_tok = first;

	return pack(AST_FunctionCall, std::move(func_call));
}

void ParserContext::parse_attribute(struct Node &ident_node) {
	if (tc.peek().type != TokenType::TokIdent) panic("Expected attribute name.", tc);
	
	struct Node attr = parse_expr_node();
	if (attr.type == AST_Identifier) std::get<IdentifierAST>(attr.ast).is_attr = true;
	if (attr.type == AST_FunctionCall) std::get<FuncCallAST>(attr.ast).is_attr = true;
	
	NodeId id = store_node(std::move(attr));
	
	if (ident_node.attr == NoNode) {
		ident_node.attr = id;
	}
	else {
		NodeId next = ident_node.attr;
		while (node_pool[next].attr != NoNode) next = node_pool[next].attr;
		node_pool[next].attr = id;
	}
}

struct Node ParserContext::gen_ident_node(const struct Token &first) {
	struct Node result = {};

	struct IdentifierAST ident_ast = {};
	ident_ast.ident = first;

	result = pack(AST_Identifier, std::move(ident_ast));

	return result;
}

// create node of function parameters. (Variable Declaration Bundle AST)
struct Node ParserContext::gen_func_param_node() {
	struct VarDeclBundleAST result = {};
	tc.consume(TokenType::TokLParen);
	
	while (tc.peek().type != TokenType::TokRParen) {
		struct Token name_tok = tc.consume(TokenType::TokIdent);
		tc.consume(TokenType::TokColon);
		struct Token type_tok = parse_type_token();
		struct VarDeclAST var_decl = {};
		var_decl.var_name_tok = name_tok;
		var_decl.var_type_tok = type_tok;
		var_decl.ac_mod = ACMOD_DEFAULT;
		result.var_decls.push_back(pack(AST_VariableDeclaration, std::move(var_decl)));
		if (tc.peek().type == TokenType::TokRParen) break;
		tc.consume(TokenType::TokComma);
		if (tc.peek().type == TokenType::TokRParen) panic("Expected parameter after comma.", tc);
	}
	
	tc.consume(TokenType::TokRParen);

	return pack(AST_VariableDeclarationBundle, std::move(result));
}

std::vector<struct Node> ParserContext::gen_body() {
	tc.consume(TokenType::TokLBracket);
	std::vector<struct Node> result;
	while (tc.peek().type != TokenType::TokRBracket) {
		if (tc.peek().type == TokenType::TokEOF) panic("Expected closing brace.", tc);
			struct Node element = parse_stmt();
		result.push_back(std::move(element));
	}
	tc.consume(TokenType::TokRBracket);
	return result;
}

struct Node ParserContext::gen_constructor_node(const struct Token &first) {
	ParserContext &pc = *this;
	struct ConstructorAST constructor = {};

	struct Token func_name = pc.tc.consume(TokenType::TokIdent);

	struct Node params = gen_func_param_node();

	constructor.params = store_node(std::move(params));

	constructor.func_name = func_name;

	constructor.body = gen_body();

	return pack(AST_Constructor, std::move(constructor));
}

struct Node ParserContext::gen_func_decl_node(const struct Token &first) {
	struct FuncDeclAST func_decl = {};

	struct Token func_name_tok = tc.consume(TokenType::TokIdent);

	struct Node params = gen_func_param_node();

	tc.consume(TokenType::TokColon);

	struct Token ret_type_tok = parse_type_token();

	func_decl.func_name_tok = func_name_tok;
	func_decl.ret_type_tok = ret_type_tok;
	func_decl.params = store_node(std::move(params));

	func_decl.body = gen_body();

	return pack(AST_FunctionDeclaration, std::move(func_decl));
}

struct Node ParserContext::gen_var_decl_node(const struct Token &first) {
	struct VarDeclBundleAST result = {};
	bool comp = false;
	while (!comp) {
		struct Token var_name_tok = tc.consume(TokenType::TokIdent);
		tc.consume(TokenType::TokColon);
		struct Token var_type_tok = parse_type_token();
		struct Token cont_tok = tc.peek();
		struct Node decl = {};
		if (cont_tok.type == TokenType::TokAssign) {
			tc.consume(TokenType::TokAssign);
			decl = parse_expression();
			cont_tok = tc.peek();
		}
		switch (cont_tok.type) {
		case TokenType::TokComma: tc.consume(TokenType::TokComma); break;
		case TokenType::TokSemiColon: tc.consume(TokenType::TokSemiColon); comp = true; break;
		default: panic("Unexpected token in variable declaration.", tc);
		}
		struct VarDeclAST var_decl = {};
		var_decl.var_name_tok = var_name_tok;
		var_decl.var_type_tok = var_type_tok;
		var_decl.decl = store_node(std::move(decl));
		var_decl.ac_mod = ACMOD_DEFAULT;
		result.var_decls.push_back(pack(AST_VariableDeclaration, std::move(var_decl)));
	}
	return pack(AST_VariableDeclarationBundle, std::move(result));
}

enum IfStmtType ParserContext::get_if_stmt_type(const struct Token &first) {
	struct Token nt = tc.peek();
	enum IfStmtType stmt_type = StmtNone;

	if (first.type == TokenType::TokIf) {
		stmt_type = StmtIf;
	}

	if (first.type == TokenType::TokElse) {
		stmt_type = StmtElse;

		if (nt.type == TokenType::TokIf) {
			stmt_type = StmtElseIf;
		}
	}

	return stmt_type;
}

struct Node ParserContext::gen_next_if_stmt_node(enum IfStmtType stmt_type) {
	struct Token nt = tc.peek();
	struct Node next_stmt = {};

	if (nt.type == TokenType::TokElse) {
		tc.consume(TokenType::TokElse);
		next_stmt = gen_if_stmt_node(nt);
	}

	if (stmt_type == StmtElse && next_stmt.type != AST_None) {
		panic("Wrong statement. statement after else statement.", tc);
	}

	return next_stmt;
}

struct Node ParserContext::gen_if_stmt_node(const struct Token &first) {
	struct IfStmtAST result = {};

	enum IfStmtType stmt_type = get_if_stmt_type(first);

	if (stmt_type == StmtElseIf) {
		tc.consume(TokenType::TokIf);
	}
	if (stmt_type == StmtNone) {
		panic("Wrong Statement type", tc);
	}

	struct Node cond = {};
	if (stmt_type == StmtElseIf || stmt_type == StmtIf) {
		tc.consume(TokenType::TokLParen);
		cond = parse_expression();
		tc.consume(TokenType::TokRParen);
	}
	std::vector<struct Node> body = gen_body();

	result.cond = store_node(std::move(cond));
	result.body = std::move(body);
	result.stmt_type = stmt_type;

	// gen next if stmt
	result.next_stmt = store_node(gen_next_if_stmt_node(stmt_type));

	return pack(AST_IfStatement, std::move(result));
}

struct Node ParserContext::gen_for_stmt_node(const struct Token &first) {
	struct ForStmtAST result = {};

	tc.consume(TokenType::TokLParen);

	struct Node init;
	if (tc.peek().type == TokenType::TokVar) {
		const Token var = tc.pull();
		init = gen_var_decl_node(var);
	} else {
		init = parse_expression();
		tc.consume(TokenType::TokSemiColon);
	}

	struct Node cond = parse_expression();
	tc.consume(TokenType::TokSemiColon);

	struct Node step = parse_expression();

	tc.consume(TokenType::TokRParen);
	std::vector<struct Node> body = gen_body();

	result.init = store_node(std::move(init));
	result.cond = store_node(std::move(cond));
	result.step = store_node(std::move(step));
	result.body = std::move(body);

	return pack(AST_ForStatement, std::move(result));
}

struct Node ParserContext::gen_ret_node(const struct Token &first) {
	ParserContext &pc = *this;
	struct Node expr = pc.tc.peek().type == TokenType::TokSemiColon ? Node{} : parse_expression();
	struct ReturnAST ret_ast = {};

	ret_ast.expr = store_node(std::move(expr));

	pc.tc.consume(TokenType::TokSemiColon);

	return pack(AST_Return, std::move(ret_ast));
}

struct Node ParserContext::gen_object_decl_node(const struct Token &first) {
	struct ObjectAST object_ast = {};

	struct Token name_tok = tc.consume(TokenType::TokIdent);
	object_ast.name_tok = name_tok;
	object_ast.parent_name_tok = {};

	if (tc.peek().type == TokenType::TokExtends) {
		tc.consume(TokenType::TokExtends);

		struct Token parent_name_tok = tc.consume(TokenType::TokIdent);
		object_ast.parent_name_tok = parent_name_tok;
	}

	object_ast.body = gen_body();

	return pack(AST_Object, std::move(object_ast));
}

struct Node ParserContext::gen_new_node(const struct Token &first) {
	struct Token object_name_tok = tc.consume(TokenType::TokIdent);
	struct ParamData param_data = parse_func_call_params();

	struct NewAST new_ast = {};

	new_ast.name_tok = object_name_tok;
	new_ast.params = std::move(param_data.params);

	return pack(AST_New, std::move(new_ast));
}

bool ParserContext::is_number_literal_integer(const std::string &num_lit_str) {
	for (char c : num_lit_str) {
		if (c == '.' || c == 'f') return false;
	}
	return true;
}

short ParserContext::get_number_literal_byte(const std::string &num_lit_str) {
	short result = 0;

	size_t len = num_lit_str.size();

	bool is_float = num_lit_str[len - 1] == 'f';
	bool is_integer = is_number_literal_integer(num_lit_str);

	if (is_float || is_integer) { // int and float
		result = 4;
	}

	if (!is_integer && !is_float) { // double
		result = 8;
	}

	return result;
}

struct Type ParserContext::find_numeric_type(unsigned nbyte, bool is_integer, bool is_signed) {
	for (const Type &numeric_type : numeric_types) {
		struct NumericData numeric_data = numeric_type.data.numeric_data;

		if (numeric_data.is_integer == is_integer && numeric_data.is_signed == is_signed && numeric_type.nbyte == nbyte) {
			return numeric_type;
		}
	}

	return {};
}

struct Node ParserContext::parse_expr_node() {
	ParserContext &pc = *this;

	struct Token first = tc.pull();

	switch (first.type) {
	case TokenType::TokNumberLiteral: {
		struct NumberLiteralAST num = {};

		num.num_tok = first;

		bool is_integer = is_number_literal_integer(num.num_tok.str);
		bool is_signed = true;
		unsigned nbyte = get_number_literal_byte(num.num_tok.str);

		struct Type numeric_type = find_numeric_type(nbyte, is_integer, is_signed);

		if (numeric_type.type_str.empty()) {
			panic("This number literal is not supported in this compiler.", pc.tc);
		}

		num.type = numeric_type;

		return pack(AST_NumberLiteral, std::move(num));
	}

	case TokenType::TokTrue:
	case TokenType::TokFalse: {
		struct BoolLiteralAST bool_lit = {};
		bool_lit.bool_tok = first;
		return pack(AST_BoolLiteral, std::move(bool_lit));
	}

	case TokenType::TokStringLiteral: {
		struct StringLiteralAST str = {};

		str.str_tok = first;

		return pack(AST_StringLiteral, std::move(str));
	}

	case TokenType::TokNull: {
		struct NullAST null = {};
		null.null_tok = first;

		return pack(AST_Null, std::move(null));
	}

	case TokenType::TokThis: {
		ThisAST self;
		self.this_tok = first;
		return pack(AST_This, std::move(self));
	}

	case TokenType::TokNew: {
		return gen_new_node(first);
	}

	case TokenType::TokIdent: {
		struct Token nt = tc.peek();
		if (nt.type == TokenType::TokLParen) {
			return gen_func_call_node(first);
		}

		return gen_ident_node(first);
	}

	case TokenType::TokLParen: {
		struct Node expr = parse_expression();

		tc.consume(TokenType::TokRParen);

		return expr;
	}

	case TokenType::TokEOF: {
		panic("Expected expression before end of file.", tc);
	}

	default:
		panic("Unexpected Token type\n", tc);
	}
}

struct Node ParserContext::parse_stmt() {

	struct Token first = tc.peek();

	switch (first.type) {
	case TokenType::TokIf: {
		tc.pull();
		return gen_if_stmt_node(first);
	}

	case TokenType::TokFor: {
		tc.pull();
		return gen_for_stmt_node(first);
	}

	case TokenType::TokVar: {
		tc.pull();
		return gen_var_decl_node(first);
	}

	case TokenType::TokFunc: {
		tc.pull();
		return gen_func_decl_node(first);
	}

	case TokenType::TokConstructor: {
		tc.pull();
		return gen_constructor_node(first);
	}

	case TokenType::TokObject: {
		tc.pull();
		return gen_object_decl_node(first);
	}

	case TokenType::TokReturn: {
		tc.pull();
		return gen_ret_node(first);
	}

	case TokenType::TokEOF: {
		tc.pull();
		return {};
	}

	default: {
		struct Node expr = parse_expression();
		tc.consume(TokenType::TokSemiColon);
		return expr;
	}
	}
}

struct Node ParserContext::parse_term() {
	struct Node node = parse_unary_expression();
	struct Token tok = {};

	while ((tok = tc.peek()).type != TokenType::TokEOF &&
		(tok.type == TokenType::TokMul || tok.type == TokenType::TokDiv || tok.type == TokenType::TokMod)) {
		TokenType op = tc.pull().type;
		struct Node right = parse_unary_expression();

		enum OperatorType op_type = op == TokenType::TokMul ? OpMUL :
			op == TokenType::TokDiv ? OpDIV : OpMOD;

		struct BinExprAST bin_expr = {};

		bin_expr.left = store_node(std::move(node));
		bin_expr.right = store_node(std::move(right));
		bin_expr.op_type = op_type;

		node = pack(AST_BinExpr, std::move(bin_expr));
	}
	return node;
}

struct Node ParserContext::parse_simple_expression() {
	struct Node node = parse_term();
	struct Token tok = {};

	while ((tok = tc.peek()).type != TokenType::TokEOF &&
		(tok.type == TokenType::TokAdd || tok.type == TokenType::TokSub)) {
		TokenType op = tc.pull().type;
		struct Node right = parse_term();

		enum OperatorType op_type = (op == TokenType::TokAdd) ? OpADD : OpSUB;

		struct BinExprAST bin_expr = {};

		bin_expr.left = store_node(std::move(node));
		bin_expr.right = store_node(std::move(right));
		bin_expr.op_type = op_type;

		node = pack(AST_BinExpr, std::move(bin_expr));
	}
	return node;
}

struct Node ParserContext::parse_prefix() {
	ParserContext &pc = *this;
	struct Token tok = {};

	tok = tc.pull();

	switch (tok.type) {
	case TokenType::TokIncrease: {
		struct IdentIncreAST incre = {};

		struct Node node = parse_unary_expression();

		if (!is_variable_target(node)) {
			panic("Increasement (++) must be used with identifier!", pc.tc);
		}

		incre.ident_node = store_node(std::move(node));

		return pack(AST_IdentIncrease, std::move(incre));
	}

	case TokenType::TokDecrease: {
		struct IdentDecreAST decre = {};

		struct Node node = parse_unary_expression();

		if (!is_variable_target(node)) {
			panic("Decreasement (--) must be used with identifier!", pc.tc);
		}

		decre.ident_node = store_node(std::move(node));

		return pack(AST_IdentDecrease, std::move(decre));
	}

	case TokenType::TokSub: {
		struct NegAST negative = {};
		negative.ast = store_node(parse_unary_expression());
		return pack(AST_Negative, std::move(negative));
	}
	case TokenType::TokAdd: {
		return parse_unary_expression();
	}
	case TokenType::TokNot: {
		struct UnaryExprAST unary_expr = {};

		unary_expr.expr = store_node(parse_unary_expression());
		return pack(AST_UnaryExpr, std::move(unary_expr));
	}

	default: {
		panic("Unknown operator type.", tc);
	}
	}

}

struct Node ParserContext::parse_postfix() {
	ParserContext &pc = *this;

	struct Node node = parse_expr_node();
	struct Token tok = {};

	while ((tok = tc.peek()).type != TokenType::TokEOF) {
		if (tok.type == TokenType::TokDot) {
			tc.consume(TokenType::TokDot);
			parse_attribute(node);
			continue;
		}

		if (tok.type == TokenType::TokIncrease) {
			tc.consume(TokenType::TokIncrease);

			if (!is_variable_target(node)) {
				panic("Increasement (++) must be used with identifier!", pc.tc);
			}

			struct IdentIncreAST incre = {};

			incre.ident_node = store_node(std::move(node));
			node = pack(AST_IdentIncrease, std::move(incre));
			continue;
		}

		if (tok.type == TokenType::TokDecrease) {
			tc.consume(TokenType::TokDecrease);

			if (!is_variable_target(node)) {
				panic("Decreasement (--) must be used with identifier!", pc.tc);
			}

			struct IdentDecreAST decre = {};

			decre.ident_node = store_node(std::move(node));
			node = pack(AST_IdentDecrease, std::move(decre));
			continue;
		}

		break;
	}

	return node;
}

struct Node ParserContext::parse_unary_expression() {
	struct Token tok = tc.peek();

	if (tok.type != TokenType::TokEOF && (tok.type == TokenType::TokNot || tok.type == TokenType::TokSub || tok.type == TokenType::TokAdd || tok.type == TokenType::TokIncrease || tok.type == TokenType::TokDecrease)) {
		return parse_prefix();
	}

	return parse_postfix();
}

struct Node ParserContext::parse_compare_expression() {
	struct Node node = parse_simple_expression();
	struct Token tok = {};

	while (((tok = tc.peek()).type != TokenType::TokEOF) &&
		(tok.type == TokenType::TokEqual || tok.type == TokenType::TokNotEqual ||
			tok.type == TokenType::TokGreater || tok.type == TokenType::TokLesser ||
			tok.type == TokenType::TokEqualGreater || tok.type == TokenType::TokEqualLesser)) {

		struct Token operator_token = tc.pull();
		TokenType op = operator_token.type;
		struct Node right = parse_simple_expression();

		enum OperatorType op_type = OpNone;
		switch (op) {
		case TokenType::TokEqual:
			op_type = OpEQUAL;
			break;
		case TokenType::TokNotEqual:
			op_type = OpNOTEQUAL;
			break;
		case TokenType::TokGreater:
			op_type = OpGREATER;
			break;
		case TokenType::TokLesser:
			op_type = OpLESS;
			break;
		case TokenType::TokEqualGreater:
			op_type = OpEQUALGREATER;
			break;
		case TokenType::TokEqualLesser:
			op_type = OpEQUALLESS;
			break;
		default:
			panic("Unknown operator type.", tc);
		}

		struct BinExprAST bin_expr = {};

		bin_expr.left = store_node(std::move(node));
		bin_expr.right = store_node(std::move(right));

		bin_expr.op_type = op_type;

		node = pack(AST_BinExpr, std::move(bin_expr));
	}
	return node;
}

struct Node ParserContext::parse_logical_expression() {
	struct Node node = parse_compare_expression();
	struct Token tok = {};

	while (((tok = tc.peek()).type != TokenType::TokEOF) &&
		(tc.peek().type == TokenType::TokOr || tc.peek().type == TokenType::TokAnd)) {
		TokenType op = tc.pull().type;
		struct Node right = parse_compare_expression();

		enum OperatorType op_type = OpNone;
		switch (op) {
		case TokenType::TokOr:
			op_type = OpOR;
			break;
		case TokenType::TokAnd:
			op_type = OpAND;
			break;
		default:
			panic("Unknown operator type.", tc);
		}

		struct BinExprAST bin_expr = {};

		bin_expr.left = store_node(std::move(node));
		bin_expr.right = store_node(std::move(right));
		bin_expr.op_type = op_type;

		node = pack(AST_BinExpr, std::move(bin_expr));
	}

	return node;
}

struct Node ParserContext::parse_expression() {
	struct Node node = parse_logical_expression();
	struct Token tok = {};

	if (((tok = tc.peek()).type != TokenType::TokEOF) &&
		(tok.type == TokenType::TokAssign || tok.type == TokenType::TokPlusAssign ||
		 tok.type == TokenType::TokMinusAssign || tok.type == TokenType::TokMultAssign ||
		 tok.type == TokenType::TokDivAssign || tok.type == TokenType::TokModAssign)) {
		TokenType op = tc.pull().type;
		struct Node right = parse_expression();

		enum OperatorType op_type = OpNone;
		switch (op) {
		case TokenType::TokAssign:
			op_type = OpASSIGN;
			break;
		case TokenType::TokPlusAssign: op_type = OpPLUSASSIGN; break;
		case TokenType::TokMinusAssign: op_type = OpMINUSASSIGN; break;
		case TokenType::TokMultAssign: op_type = OpMULTASSIGN; break;
		case TokenType::TokDivAssign: op_type = OpDIVASSIGN; break;
		case TokenType::TokModAssign: op_type = OpMODASSIGN; break;
		default:
			panic("Unknown operator type.", tc);
		}

		struct BinExprAST bin_expr = {};

		bin_expr.left = store_node(std::move(node));
		bin_expr.right = store_node(std::move(right));
		bin_expr.op_type = op_type;

		node = pack(AST_BinExpr, std::move(bin_expr));
	}

	return node;
}

//=======================================================================
//      FIRST PASS OF COMPILER FOR CODE STRUCTURES AND BLOCKS.
//=======================================================================

void ParserContext::pass_body() {
	struct Token tok;

	tc.consume(TokenType::TokLBracket);
	int bracket_counter = 1;

	while ((tok = tc.pull()).type != TokenType::TokEOF) {
		if (tok.type == TokenType::TokRBracket) {
			bracket_counter--;
		}
		if (tok.type == TokenType::TokLBracket) {
			bracket_counter++;
		}

		if (bracket_counter == 0)
			break;
	}
	if (bracket_counter != 0) panic("Expected closing brace.", tc);
}

void ParserContext::pass_func_param() {
	struct Token tok;

	tc.consume(TokenType::TokLParen);
	int paren_counter = 1;

	while ((tok = tc.pull()).type != TokenType::TokEOF) {
		if (tok.type == TokenType::TokRParen) {
			paren_counter--;
		}
		if (tok.type == TokenType::TokLParen) {
			paren_counter++;
		}

		if (paren_counter == 0)
			break;
	}
	if (paren_counter != 0) panic("Expected closing parenthesis.", tc);
}

void ParserContext::parse_object_structure() {
	ParserContext &pc = *this;
	struct Token object_name_tok = tc.consume(TokenType::TokIdent);
	const std::string &object_name = object_name_tok.str;
	struct Type object_type = gen_object_type(object_name);
	pc.object_type_smtb[object_name] = object_type;
	if (tc.peek().type == TokenType::TokExtends) {
		tc.consume(TokenType::TokExtends);
		tc.consume(TokenType::TokIdent);
	}
	pass_body();
}

void ParserContext::parse_func_structure() {

	tc.consume(TokenType::TokIdent);

	pass_func_param();

	tc.consume(TokenType::TokColon);

	parse_type_token();

	// We will not parse content of function declaration.
	pass_body();
}

void ParserContext::parse_constructor_structure() {

	tc.consume(TokenType::TokIdent);

	pass_func_param();
	// We will not parse content of function declaration.
	pass_body();
}

void ParserContext::parse_var_structure() {
	bool comp = false;

	while (!comp) {
		tc.consume(TokenType::TokIdent);

		tc.consume(TokenType::TokColon);

		parse_type_token();

		struct Token tok = {};
		unsigned parentheses = 0;

		while ((tok = tc.pull()).type != TokenType::TokEOF) {
			if (tok.type == TokenType::TokLParen) ++parentheses;
			if (tok.type == TokenType::TokRParen && parentheses != 0) --parentheses;
			if (tok.type == TokenType::TokSemiColon && parentheses == 0) {
				comp = true;
				break;
			}

			if (tok.type == TokenType::TokComma && parentheses == 0) {
				break;
			}
		}
		if (tok.type == TokenType::TokEOF) panic("Expected semicolon after variable declaration.", tc);
	}
}

void ParserContext::parse_preprocessor() {
	struct Token first = tc.pull();
	if (first.type != TokenType::TokSharp) return;

	struct Token directive = tc.consume(TokenType::TokIdent);
	struct Token file = tc.consume(TokenType::TokStringLiteral);

	if (directive.location.line != first.location.line || file.location.line != first.location.line)
		panic("Expected #import and its file path on the same line.", tc);

	const std::string path = file.str.substr(1, file.str.size() - 2);

	if (path.empty()) panic("Import file path is empty.", tc);
	register_import(path);

	std::fill(tc.file.begin() + first.location.offset, tc.file.begin() + tc.cur_ch, ' ');
}

// First pass of compiler
// This function will parse the structure of source code roughly
// This function will register global object data (with member of object) , global
// function data, global variable data
void ParserContext::parse_structure() {

	struct Token first = tc.pull();

	switch (first.type) {
	case TokenType::TokObject: {
		parse_object_structure();
		break;
	}

	case TokenType::TokFunc: {
		parse_func_structure();
		break;
	}

	case TokenType::TokVar: {
		parse_var_structure();
		break;
	}

	case TokenType::TokConstructor: {
		parse_constructor_structure();
		break;
	}

	default: {
		panic("Unexpected token, block of source code must begin with "
			"object or "
			"function.",
			tc);
	}
	}
}

