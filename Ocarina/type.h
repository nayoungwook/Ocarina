#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct NumericData {
	unsigned rank;
	bool is_signed, is_integer;
};

union TypeData {
	struct NumericData numeric_data;
	std::size_t object_data;
};

enum TypeKind {
	TK_Numeric, TK_Object, TK_Primitive, TK_Null
};

struct Type {
	std::string type_str;
	union TypeData data = {};
	enum TypeKind type_kind = TK_Primitive;
	uint32_t nbyte = 0;
};

class ParserContext;

struct NumericData gen_numeric_data(unsigned rank, bool is_signed, bool is_integer);
struct Type gen_primitive_type(const std::string &type_str, unsigned nbyte);
struct Type gen_numeric_type(const std::string &type_str, unsigned nbyte, struct NumericData numeric_data);
struct Type gen_object_type(const std::string &type_str);
struct Type gen_null_type();
struct Type find_type(const ParserContext &pc, const std::string &type_str);
bool is_castable(const ParserContext &pc, const Type &source, const Type &target);
bool is_object_reference(const Type &type);
