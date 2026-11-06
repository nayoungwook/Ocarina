#include "parser.h"
#include "error.h"

Token ParserContext::parse_type_token() {
	Token type_tok = tc.consume(TokenType::TokIdent);
	if (type_tok.str != "array")
		return type_tok;

	type_tok.str += '<';
	tc.consume(TokenType::TokLesser);
	type_tok.str += parse_type_token().str;
	if (tc.peek().type == TokenType::TokEqualGreater) {
		Token assign = tc.pull();
		assign.type = TokenType::TokAssign;
		assign.str = "=";
		assign.length = 1;
		++assign.location.offset;
		++assign.location.column;
		tc.token_cache = std::move(assign);
	} else
		tc.consume(TokenType::TokGreater);
	type_tok.str += '>';
	type_tok.length = static_cast<unsigned>(type_tok.str.size());
	return type_tok;
}

Node ParserContext::gen_array_decl_node() {
	ArrayDeclAST array_ast;
	while (tc.peek().type != TokenType::TokRBracket) {
		array_ast.elements.push_back(parse_expression());
		if (tc.peek().type == TokenType::TokRBracket)
			break;
		tc.consume(TokenType::TokComma);
	}
	tc.consume(TokenType::TokRBracket);
	return pack(AST_ArrayDeclaration, std::move(array_ast));
}

Node ParserContext::gen_array_access_node(Node node) {
	ArrayAccessAST access;
	access.target_array = store_node(std::move(node));
	while (tc.peek().type == TokenType::TokLSquareBracket) {
		tc.consume(TokenType::TokLSquareBracket);
		access.indexes.push_back(parse_expression());
		tc.consume(TokenType::TokRSquareBracket);
	}
	return pack(AST_ArrayAccess, std::move(access));
}
