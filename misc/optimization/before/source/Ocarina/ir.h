#pragma once

#include "semantics.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#define BYTE_CHUNK 1024

// --------------------------
// ** MetaData **
// --------------------------

#define META_BEGIN 0x01 // begining of metadata section
#define META_END 0x02   // end of metadata section

#define META_TERM 0x03

// META_OBJECT id, name, parent_id, size, [contents], META_TERM
#define META_OBJECT 0x04

// Functions include argument types, stack size and local variable layouts.
#define META_FUNC 0x05

// META_VAR id, name, type, offset, size
#define META_VAR 0x06

// META_CONSTRUCTOR func_id(uint) func_str(char*)
#define META_CONSTRUCTOR 0X07
#define META_IMPORT 0x08 // required bytecode path, relative to this bytecode file
#define META_SCENE 0x09 // same layout as META_OBJECT, without spatial builtins

#define CODE_BEGIN 0x51 // begining of code section
#define CODE_END 0x52

#define CODE_TERM 0x53

#define CODE_OBJECT 0x54 // CODE_OBJECT object_id(uint)

#define CODE_FUNC 0x55 // CODE_FUNC func_id(uint)

#define RODATA_BEGIN 0x11
#define RODATA_END 0x12

#define RODATA_STR 0x13 // RODATA_STR id(uint) str(char*)

// --------------------------
// ** Operations **
// --------------------------

#define OP_SP_PUSH 0x56 // sp_push amount(uint)
#define OP_SP_POP 0x57  // sp_pop amount(uint)

#define OP_SP_LOAD 0x58 // sp_load offset(uint) size(uint)
#define OP_SP_SAVE 0x59 // sp_save offset(uint) size(uint)
#define OP_SP_INCRE 0x5a // sp_incre offset(uint) size(uint)
#define OP_SP_DECRE 0x5b // sp_decre offset(uint) size(uint)

#define OP_SYSCALL 0x5c // syscall id(uint) arg_cnt(uint)
#define OP_CALL 0x5d    // call id(uint) arg_cnt(uint)
#define OP_CALL_ATTR 0x5e // call_attr id(uint) arg_cnt(uint)
#define OP_CALL_OBJECT 0x5f // call_object id(uint) arg_cnt(uint)
#define OP_CALL_GLOBAL 0x61 // call_global id(uint) arg_cnt(uint)

#define OP_LOAD_STR 0x60 // load_str id(uint)

#define OP_RET 0x70 // ret
#define OP_RET_VAL 0x71 // ret with value. store return value from top of the stack into ret register 

#define OP_GOTO 0x73 // goto label(uint)
#define OP_LABEL 0x74 // label(uint)
#define OP_JE 0x75   // je label(uint)
#define OP_JNE 0x76  // jne label(uint)

// --------------------------
// ** Binary expression **
// --------------------------

#define OP_EXPR_OP 0x72

#define OP_ADD 0x01
#define OP_SUB 0x02
#define OP_MUL 0x03
#define OP_DIV 0x04
#define OP_EQUAL 0x05
#define OP_NOTEQUAL 0x06
#define OP_GREATER 0x07
#define OP_LESS 0x08
#define OP_EQUALGREATER 0x09
#define OP_EQUALLESS 0x0a
#define OP_ASSIGN 0x0b
#define OP_OR 0x0c
#define OP_AND 0x0d
#define OP_MOD 0x0e

// --------------------------
// ** Load and push data **
// --------------------------

#define OP_LDC_I4 0x81 // load const integer 4byte
#define OP_LDC_F4 0x82 // load const float 4byte
#define OP_LDC_F8 0x83 // load const float 8byte

#define OP_PUSH_NULL 0x84 // push null
#define OP_LOAD_OBJECT 0x85 // load_object offset(uint) size(uint) # load from current object and push into stack.
#define OP_INCRE_OBJECT 0x8a // incre_object offset(uint) size(uint) # increase from current object
#define OP_DECRE_OBJECT 0x8b // decre_object offset(uint) size(uint) # decrease from current object

#define OP_LOAD_GLOBAL 0x8c // load_global offset(uint) sizeof(uint) # load from global and push into stack.
#define OP_INCRE_GLOBAL 0x8d // incre_global offset(uint) sizeof(uint) # increase from global
#define OP_DECRE_GLOBAL 0x8e // decre_global offset(uint) sizeof(uint) # decrease from global

#define OP_LOAD_ATTR 0x8f // load_attr offset(uint) size(uint) # load from data from top of the stack.

#define OP_INCRE_ATTR 0x90 // incre_attr offset(uint) size(uint) # increase from data from top of the stack.
#define OP_DECRE_ATTR 0x91 // decre_attr offset(uint) size(uint) # decrease from data from top of the stack.


// Storage operations missing from the C emitter. Existing opcode values are unchanged.
#define OP_SAVE_OBJECT 0x92
#define OP_SAVE_GLOBAL 0x93
#define OP_SAVE_ATTR 0x94
#define OP_NEW_OBJECT 0x95 // object_id, size, constructor_id (-1 if absent), arg_count
#define OP_DUP 0x9c // duplicate the top count(uint) operands in order
#define OP_CONVERT 0x9d // convert top operand to compiler-selected VMOPType(uint)
#define OP_ADD_CHILD 0x9e // receiver(uint): 0=current object, 1=operand stack
#define OP_GET_CHILD 0x9f // receiver(uint): 0=current object, 1=operand stack
#define OP_CHILD_SIZE 0xa0 // receiver(uint): 0=current object, 1=operand stack
#define OP_LOAD_THIS 0xa1 // push the current receiver reference
#define OP_VECTOR_COMPONENT 0xa2 // extract component(uint) from a vector value
#define OP_SAVE_VECTOR_COMPONENT 0xa3 // storage(uint), vector offset(uint), component(uint)
#define OP_RENDER 0xa4 // texture, position, width, height, rotation byte offsets in the receiver

enum class VectorStorage : unsigned { Local, Global, Object, Attribute };

using byte = std::uint8_t;

struct RODATA_Str {
	std::string str;
	unsigned id = 0;
};

struct IRFile {
	std::string source;
	std::vector<byte> bytes;
};

class IRContext {
public:
	std::vector<byte> bytes;
	unsigned label_id = 0;
	std::unordered_map<std::string, RODATA_Str> str_rodata;
	std::size_t current_object = NoNode;
	std::size_t current_func = NoNode;

	void init_irc();
	void emit_byte(byte _b);
	void gen_ir(ParserContext input_pc);
	std::vector<IRFile> gen_files(ParserContext input_pc);
	const std::vector<byte> &get_bytes() const;

private:
	ParserContext pc;
	SemanticsContext semantics;
	std::size_t return_label = NoNode;
	unsigned next_string_id = 0;
	void emit_str(const std::string &str);
	void emit_int(std::uint32_t si);
	void emit_float(float f);
	void emit_double(double f);
	void gen_func_metadata(const FuncData &fd, const std::vector<std::size_t> &declared_vars);
	void gen_var_metadata(const VarData &vd);
	void gen_object_metadata(const ObjectAST &object_ast);
	void gen_metadata(const std::vector<Node> &nodes, const std::vector<std::string> &imports);
	void gen_unit(const std::vector<Node> &nodes, const std::vector<std::string> &imports);
	void gen_global_metadata(const Node &node);
	unsigned get_total_stack_size_of_func(const std::vector<std::size_t> &declared_vars) const;
	RODATA_Str add_str_rodata(const std::string &str);
	byte get_op_byte(OperatorType op_type) const;
	void gen_node_ir(const Node &node, bool value_required = false);
	void gen_child_ir(NodeId id, bool value_required = true);
	void gen_value_ir(const Node &node, const Type &target);
	void gen_conversion(const Type &source, const Type &target);
	void gen_ident_ir(const Node &node, bool value_required);
	void gen_ident_load_ir(const VarData &var_data, bool is_attr);
	void gen_ident_incre_ir(const VarData &var_data, bool is_attr);
	void gen_ident_decre_ir(const VarData &var_data, bool is_attr);
	void gen_ident_update_ir(const VarData &var_data, bool is_attr, bool decrease);
	void gen_ident_save_ir(const VarData &var_data, bool is_attr);
	IdentifierAST gen_assignment_target(const Node &node);
	void gen_target_load_ir(const IdentifierAST &target);
	void gen_target_save_ir(const IdentifierAST &target);
	void gen_number_ir(const NumberLiteralAST &num_lit_ast, bool negative = false);
	void gen_zero(const Type &type);
	void gen_function_ir(const FuncData &func_data, const std::vector<std::size_t> &declared_vars,
		const std::vector<Node> &body);
	void gen_if_ir(const IfStmtAST &if_stmt_ast);
	void gen_rodata();
	unsigned next_label();
	void emit_jump(byte op, unsigned label);
};
