#include "parser.h"
#include "diagnostic.h"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message) {
	if (!condition) throw std::logic_error(message);
}

static Node parse(ParserContext &pc, std::string_view source) {
	pc.tc = TokenizerContext(source, "<cast-parser>");
	Node node = pc.parse_stmt();
	require(pc.tc.peek().type == TokenType::TokEOF, "Unexpected trailing tokens.");
	return node;
}

int main() {
	ParserContext pc;
	const Node arithmetic = parse(pc, "cast<int>(1 + 2 * 3);");
	require(arithmetic.type == AST_Cast, "Cast node missing.");
	const CastAST &cast = std::get<CastAST>(arithmetic.ast);
	require(cast.target_type_tok.str == "int", "Wrong cast target.");
	const BinExprAST &sum = std::get<BinExprAST>(pc.get_node(cast.expr).ast);
	require(sum.op_type == OpADD &&
		std::get<BinExprAST>(pc.get_node(sum.right).ast).op_type == OpMUL,
		"Cast operand lost expression precedence.");

	const Node nested = parse(pc, "cast<Player>(cast<Other>(get_child(0)));");
	const CastAST &outer = std::get<CastAST>(nested.ast);
	const CastAST &inner = std::get<CastAST>(pc.get_node(outer.expr).ast);
	require(outer.target_type_tok.str == "Player" && inner.target_type_tok.str == "Other" &&
		pc.get_node(inner.expr).type == AST_FunctionCall, "Nested object casts failed.");

	const Node member = parse(pc, "cast<Player>(parent.get_child(0)).update();");
	const CastAST &member_cast = std::get<CastAST>(member.ast);
	require(pc.get_node(member_cast.expr).type == AST_Identifier && member.attr != NoNode &&
		std::get<FuncCallAST>(pc.get_node(member.attr).ast).func_name_tok.str == "update",
		"Cast operand or result member access failed.");

	const Node precedence = parse(pc, "cast<float>(value) * 2 + 1;");
	const BinExprAST &addition = std::get<BinExprAST>(precedence.ast);
	const BinExprAST &product = std::get<BinExprAST>(pc.get_node(addition.left).ast);
	require(addition.op_type == OpADD && product.op_type == OpMUL &&
		pc.get_node(product.left).type == AST_Cast, "Cast operator precedence failed.");

	const Node negative = parse(pc, "-cast<int>(value);");
	require(pc.get_node(std::get<NegAST>(negative.ast).ast).type == AST_Cast,
		"Unary expression around cast failed.");
	const Node arguments = parse(pc, "print(cast<int>(value), cast<Player>(get_child(0)));");
	const auto &params = std::get<FuncCallAST>(arguments.ast).params;
	require(params.size() == 2 && params[0].type == AST_Cast && params[1].type == AST_Cast,
		"Casts in call arguments failed.");

	const Node grouping = parse(pc, "(value + 2) * 3;");
	require(std::get<BinExprAST>(grouping.ast).op_type == OpMUL, "Grouping changed.");
	const Node comparison = parse(pc, "(value) < other;");
	require(std::get<BinExprAST>(comparison.ast).op_type == OpLESS, "Comparison changed.");
	const Node ordinary_call = parse(pc, "cast(value);");
	require(ordinary_call.type == AST_FunctionCall, "Ordinary cast-named call changed.");

	const Node function = parse(pc, "func main():void { var value:Player = cast<Player>(get_child(0)); }");
	const auto &body = std::get<FuncDeclAST>(function.ast).body;
	const Node &decl = std::get<VarDeclBundleAST>(body.front().ast).var_decls.front();
	require(pc.get_node(std::get<VarDeclAST>(decl.ast).decl).type == AST_Cast,
		"Cast in variable initializer failed.");

	for (const char *invalid : {"cast<>(x);", "cast<int>();", "cast<int>(a, b);",
		"cast<int>(x;", "cast<int>x;", "cast<int(x);", "cast<5>(x);",
		"cast<int>(value)++;", "(int) (value);"}) {
		try { parse(pc, invalid); }
		catch (const FrontendError &) { continue; }
		throw std::logic_error(std::string("Invalid cast syntax accepted: ") + invalid);
	}
	std::cout << "PASS: cast AST, nested casts, precedence, member access, initializers, grouping, comparisons and invalid syntax\n";
}
