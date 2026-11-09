#include "lexer.h"
#include "error.h"

#include <cctype>
#include <unordered_map>

std::unordered_map<std::string, struct KeywordEntry> TokenizerContext::keyword_table;

void TokenizerContext::insert_keyword(const std::string &keyword, TokenType tok_type) {
	keyword_table[keyword] = { keyword, tok_type };
}

void TokenizerContext::init_keyword() {
	if (!keyword_table.empty()) return;
	insert_keyword("var", TokenType::TokVar);
	insert_keyword("if", TokenType::TokIf);
	insert_keyword("for", TokenType::TokFor);
	insert_keyword("func", TokenType::TokFunc);
	insert_keyword("return", TokenType::TokReturn);
	insert_keyword("else", TokenType::TokElse);
	insert_keyword("object", TokenType::TokObject);
	insert_keyword("extends", TokenType::TokExtends);
	insert_keyword("private", TokenType::TokPrivate);
	insert_keyword("public", TokenType::TokPublic);
	insert_keyword("protected", TokenType::TokProtected);
	insert_keyword("constructor", TokenType::TokConstructor);
	insert_keyword("new", TokenType::TokNew);
	insert_keyword("true", TokenType::TokTrue);
	insert_keyword("false", TokenType::TokFalse);
	insert_keyword("null", TokenType::TokNull);
	insert_keyword("this", TokenType::TokThis);
}

TokenizerContext::TokenizerContext(std::string_view file, const std::string &source_name) {
	TokenizerContext &tc = *this;
	if (file.find('\0') != std::string_view::npos)
		throw FrontendError(source_name, {}, "Unexpected null character.");
	tc.file = file;
	tc.source_name = source_name;
	tc.begin_ch = 0;
	init_tc();
	init_keyword();
}

void TokenizerContext::init_tc() {
	TokenizerContext &tc = *this;
	tc.cur_ch = tc.begin_ch;

	// skipping BOM.
	if (tc.file.compare(0, 3, "\xEF\xBB\xBF") == 0) tc.cur_ch += 3;

	tc.line_begin = tc.cur_ch;
	tc.token_cache.reset();
	tc.line_num = 1;
}

SourceLocation TokenizerContext::get_source_location() const {
	const TokenizerContext &tc = *this;
	return { static_cast<std::size_t>(tc.cur_ch - tc.begin_ch), tc.line_num,
			static_cast<std::size_t>(tc.cur_ch - tc.line_begin) + 1 };
}

char TokenizerContext::next_ch() {
	char ch = current_ch();
	if (cur_ch >= file.size()) panic("Unexpected end of input.", *this);

	cur_ch++;
	if (ch == '\n') {
		line_num++;
		line_begin = cur_ch;
	}

	return ch;
}

char TokenizerContext::current_ch(std::size_t ahead) const {
	return cur_ch < file.size() && ahead < file.size() - cur_ch ? file[cur_ch + ahead] : '\0';
}

bool TokenizerContext::is_digit(char ch) {
	return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

bool TokenizerContext::is_ident(char ch) {
	return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool TokenizerContext::is_sc(char wc) {
	if (wc == '_') return false;
	return (wc >= '!' && wc <= '/') || (wc >= ':' && wc <= '@') ||
		(wc >= '[' && wc <= '`') || (wc >= '{' && wc <= '~');
}

struct Token TokenizerContext::gen_token(TokenType type, std::string str, SourceLocation location) {
	struct Token tok = {};
	tok.length = static_cast<unsigned>(str.size());
	tok.str = std::move(str);
	tok.type = type;
	tok.location = location;
	return tok;
}

struct Token TokenizerContext::gen_num_token() {
	TokenizerContext &tc = *this;
	SourceLocation location = get_source_location();
	std::string str;

	while (is_digit(current_ch())) str += next_ch();

	if (current_ch() == '.') {
		str += next_ch();
		while (is_digit(current_ch())) str += next_ch();
	}

	if (current_ch() == 'f') str += next_ch();
	if (is_ident(current_ch()) || current_ch() == '.') panic("Invalid numeric type.", tc);

	return gen_token(TokenType::TokNumberLiteral, std::move(str), location);
}

TokenType TokenizerContext::check_ident_type(const std::string &str) {
	auto ke = keyword_table.find(str);
	if (ke == keyword_table.end()) return TokenType::TokIdent;
	return ke->second.type;
}

struct Token TokenizerContext::gen_ident_token() {
	SourceLocation location = get_source_location();
	std::string str;
	while (is_ident(current_ch())) str += next_ch();

	TokenType type = check_ident_type(str);
	return gen_token(type, std::move(str), location);
}

void TokenizerContext::get_str_literal(std::string &str, char quote) {
	TokenizerContext &tc = *this;
	unsigned str_len = 0;
	while (current_ch() != quote) {
		char ch = current_ch();
		if (ch == '\0' || ch == '\n' || ch == '\r') panic("Unterminated literal.", tc);
		str += next_ch();

		if (ch == '\\') {
			ch = current_ch();
			if (ch != 'n' && ch != 'r' && ch != 't' && ch != '0' &&
				ch != '\\' && ch != '\'' && ch != '"') panic("Invalid escape sequence.", tc);
			str += next_ch();
		}
		str_len++;
	}
	str += next_ch();
	if (quote == '\'' && str_len != 1) panic("Invalid character literal.", tc);
}

struct Token TokenizerContext::gen_sc_token() {
	TokenizerContext &tc = *this;
	SourceLocation location = get_source_location();
	std::string str;
	TokenType type = TokenType::TokEOF;
	char ch = next_ch();
	str += ch;
	switch (ch) {
	case '"': type = TokenType::TokStringLiteral; get_str_literal(str, ch); break;
	case '\'': type = TokenType::TokCharacterLiteral; get_str_literal(str, ch); break;
	case '=': type = TokenType::TokAssign; if (current_ch() == '=') type = TokenType::TokEqual; break;
	case '!': type = TokenType::TokNot; if (current_ch() == '=') type = TokenType::TokNotEqual; break;
	case '#': type = TokenType::TokSharp; break;
	case '<': type = TokenType::TokLesser; if (current_ch() == '=') type = TokenType::TokEqualLesser; break;
	case '>': type = TokenType::TokGreater; if (current_ch() == '=') type = TokenType::TokEqualGreater; break;
	case '(': type = TokenType::TokLParen; break;
	case ')': type = TokenType::TokRParen; break;
	case '{': type = TokenType::TokLBracket; break;
	case '}': type = TokenType::TokRBracket; break;
	case ':': type = TokenType::TokColon; break;
	case ';': type = TokenType::TokSemiColon; break;
	case '.': type = TokenType::TokDot; break;
	case ',': type = TokenType::TokComma; break;
	case '|': type = current_ch() == '|' ? TokenType::TokOr : TokenType::TokBitOr; break;
	case '&': type = current_ch() == '&' ? TokenType::TokAnd : TokenType::TokBitAnd; break;
	case '+':
		type = TokenType::TokAdd;
		if (current_ch() == '=') type = TokenType::TokPlusAssign;
		else if (current_ch() == '+' && current_ch(1) != '+') type = TokenType::TokIncrease;
		break;
	case '-':
		type = TokenType::TokSub;
		if (current_ch() == '=') type = TokenType::TokMinusAssign;
		else if (current_ch() == '-' && current_ch(1) != '-') type = TokenType::TokDecrease;
		break;
	case '*':
		type = TokenType::TokMul;
		if (current_ch() == '=') type = TokenType::TokMultAssign;
		else if (current_ch() == '*') type = TokenType::TokPow;
		break;
	case '/': type = current_ch() == '=' ? TokenType::TokDivAssign : TokenType::TokDiv; break;
	case '%': type = current_ch() == '=' ? TokenType::TokModAssign : TokenType::TokMod; break;
	default: panic("Undefined special character.", tc);
	}
	switch (type) {
	case TokenType::TokEqual: case TokenType::TokNotEqual: case TokenType::TokEqualLesser: case TokenType::TokEqualGreater:
	case TokenType::TokOr: case TokenType::TokAnd: case TokenType::TokPlusAssign: case TokenType::TokMinusAssign:
	case TokenType::TokMultAssign: case TokenType::TokDivAssign: case TokenType::TokIncrease: case TokenType::TokDecrease: case TokenType::TokPow:
	case TokenType::TokModAssign:
		str += next_ch();
		break;
	default: break;
	}
	return gen_token(type, std::move(str), location);
}

struct Token TokenizerContext::peek() {
	if (!token_cache.has_value()) token_cache = pull();
	return token_cache.value();
}

struct Token TokenizerContext::pull() {
	TokenizerContext &tc = *this;
	if (tc.token_cache.has_value()) {
		struct Token result = std::move(tc.token_cache.value());
		tc.token_cache.reset();
		return result;
	}

	// comments and whitespaces.
	for (;;) {
		while (std::isspace(static_cast<unsigned char>(current_ch()))) next_ch(); // skipping white spaces.

		if (current_ch() == '/' && current_ch(1) == '/') { // comments
			while (current_ch() != '\0' && current_ch() != '\n') next_ch();
		}
		else if (current_ch() == '/' && current_ch(1) == '*') { // multi line comments.
			next_ch(); next_ch();
			while (!(current_ch() == '*' && current_ch(1) == '/')) {
				if (current_ch() == '\0') panic("Unterminated comment.", tc);
				next_ch();
			}
			next_ch(); next_ch();
		}
		else break;
	}

	// real token shits.
	if (std::isalpha(static_cast<unsigned char>(current_ch())) || current_ch() == '_') return gen_ident_token(); // identifier

	if (is_sc(current_ch())) return gen_sc_token(); // special character

	if (is_digit(current_ch())) return gen_num_token(); // digits

	if (current_ch() != '\0') panic("Unexpected character.", tc); // unexpected character

	struct Token eof_token = gen_token(TokenType::TokEOF, "EOF", get_source_location()); // eof

	return eof_token;
}

struct Token TokenizerContext::consume(TokenType tt) {
	TokenizerContext &tc = *this;
	struct Token tok = peek();

	if (tok.type != tt) panic("Wrong token type consumed.", tc);

	return pull();
}


