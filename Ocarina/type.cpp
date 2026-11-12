#include "type.h"
#include "parser.h"
#include "error.h"

struct Type gen_primitive_type(const std::string &type_str, unsigned nbyte) {
	struct Type result = {};
	result.type_str = type_str;
	result.nbyte = nbyte;
	result.type_kind = TK_Primitive;
	return result;
}

struct NumericData gen_numeric_data(unsigned rank, bool is_signed, bool is_integer) {
	struct NumericData numeric_data = {};
	numeric_data.rank = rank;
	numeric_data.is_integer = is_integer;
	numeric_data.is_signed = is_signed;
	return numeric_data;
}

struct Type gen_numeric_type(const std::string &type_str, unsigned nbyte, struct NumericData numeric_data) {
	struct Type result = gen_primitive_type(type_str, nbyte);
	result.type_kind = TK_Numeric;
	result.data.numeric_data = numeric_data;
	return result;
}

struct Type gen_null_type() {
	struct Type type = {};
	type.type_kind = TK_Null;
	type.type_str = "null";
	return type;
}

struct Type gen_object_type(const std::string &type_str) {
	struct Type type = {};
	type.type_kind = TK_Object;
	type.type_str = type_str;
	type.data.object_data = static_cast<std::size_t>(-1);
	type.nbyte = 8;
	return type;
}

Type gen_scene_type(const std::string &type_str) {
	Type type = gen_object_type(type_str);
	type.type_kind = TK_Scene;
	return type;
}

struct Type find_type(const ParserContext &pc, const std::string &type_str) {
	auto primitive_type = pc.primitive_type_smtb.find(type_str);
	if (primitive_type != pc.primitive_type_smtb.end()) return primitive_type->second;
	auto object_type = pc.object_type_smtb.find(type_str);
	if (object_type != pc.object_type_smtb.end()) return object_type->second;
	panic("Failed to find type!", pc.tc);
}

static bool derives_from(const ParserContext &pc, Type derived, const Type &base) {
	while (derived.type_kind == TK_Object || derived.type_kind == TK_Scene) {
		if (derived.type_str == base.type_str)
			return true;
		derived = pc.object_data.at(derived.data.object_data).parent_type;
	}
	return false;
}

bool is_object_reference(const Type &type) {
	return type.type_kind == TK_Object || type.type_str == "object";
}

bool is_scene_reference(const Type &type) {
	return type.type_kind == TK_Scene || type.type_str == "scene";
}

bool is_instance_reference(const Type &type) {
	return is_object_reference(type) || is_scene_reference(type);
}

bool is_castable(const ParserContext &pc, const Type &source, const Type &target) {
	if (source.type_str == target.type_str)
		return true;
	if (source.type_kind == TK_Object && target.type_str == "object")
		return true;
	if (source.type_kind == TK_Scene && target.type_str == "scene")
		return true;
	if (source.type_kind != TK_Null && source.type_kind != target.type_kind)
		return false;
	if (source.type_kind == TK_Null)
		return is_instance_reference(target) || target.type_str == "string" || target.type_str == "window";
	if (source.type_kind == TK_Numeric)
		return source.data.numeric_data.rank <= target.data.numeric_data.rank;
	if (source.type_kind == TK_Object || source.type_kind == TK_Scene)
		return derives_from(pc, source, target);
	return false;
}
