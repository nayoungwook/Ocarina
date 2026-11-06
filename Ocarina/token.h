#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

enum class TokenType {
	TokEOF,
	TokIdent,
	TokNumberLiteral,
	TokStringLiteral,
	TokCharacterLiteral,
	TokBoolLiteral,
	TokEqual,
	TokNotEqual,
	TokSharp,
	TokLesser,
	TokEqualLesser,
	TokGreater,
	TokEqualGreater,
	TokAdd,
	TokSub,
	TokMul,
	TokDiv,
	TokLParen,
	TokRParen,
	TokLBracket,
	TokRBracket,
	TokLSquareBracket,
	TokRSquareBracket,
	TokAssign,
	TokSemiColon,
	TokColon,
	TokComma,
	TokDot,

	TokPlusAssign,
	TokMinusAssign,
	TokMultAssign,
	TokDivAssign,

	TokIncrease,
	TokDecrease,
	TokPow,

	TokNot,

	TokVar,
	TokIf,
	TokFor,
	TokFunc,

	TokOr,
	TokAnd,

	TokBitOr,
	TokBitAnd,

	TokReturn,

	TokElse,
	TokObject,
	TokExtends,

	TokPrivate,
	TokProtected,
	TokPublic,

	TokConstructor,
	TokNew,

	TokTrue,
	TokFalse,

	TokNull,
};

struct SourceLocation {
	std::size_t offset = 0;
	std::size_t line = 1;
	std::size_t column = 1;
};

struct Token {
	std::string str;
	unsigned length = 0;
	TokenType type = TokenType::TokEOF;
	SourceLocation location;
};

struct KeywordEntry {
	std::string keyword;
	TokenType type;
};

class TokenizerContext {
public:
	explicit TokenizerContext(std::string_view file = {}, const std::string &source_name = "<input>");

	std::optional<struct Token> token_cache;
	std::string file;
	std::size_t cur_ch = 0;
	std::size_t begin_ch = 0;
	unsigned line_num = 1;

	std::size_t line_begin = 0;
	std::string source_name;

	void init_tc();
	SourceLocation get_source_location() const;
	struct Token peek();
	struct Token pull();
	struct Token consume(TokenType tt);

private:
	static std::unordered_map<std::string, struct KeywordEntry> keyword_table;
	static void insert_keyword(const std::string &keyword, TokenType tok_type);
	static void init_keyword();
	char current_ch(std::size_t ahead = 0) const;
	char next_ch();
	static bool is_digit(char ch);
	static bool is_ident(char ch);
	static bool is_sc(char wc);
	struct Token gen_token(TokenType type, std::string str, SourceLocation location);
	struct Token gen_num_token();
	static TokenType check_ident_type(const std::string &str);
	struct Token gen_ident_token();
	void get_str_literal(std::string &str, char quote);
	struct Token gen_sc_token();
};
