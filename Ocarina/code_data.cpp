#include "code_data.h"
#include "vm_opcode.h"

int vm_instruction_operand_count(std::uint8_t opcode) {
	switch (opcode) {
	case OP_SP_PUSH:
	case OP_SP_POP:
	case OP_LOAD_STR:
	case OP_EXPR_OP:
	case OP_GOTO:
	case OP_LABEL:
	case OP_JE:
	case OP_JNE:
	case OP_LDC_I4:
	case OP_LDC_F4:
	case OP_LDC_F8:
	case OP_DUP:
	case OP_CONVERT:
	case OP_ADD_CHILD:
	case OP_GET_CHILD:
	case OP_CHILD_SIZE:
	case OP_VECTOR_COMPONENT:
		return 1;
	case OP_SP_LOAD:
	case OP_SP_SAVE:
	case OP_SP_INCRE:
	case OP_SP_DECRE:
	case OP_SYSCALL:
	case OP_CALL:
	case OP_CALL_ATTR:
	case OP_CALL_OBJECT:
	case OP_CALL_GLOBAL:
	case OP_CALL_SUPER:
	case OP_LOAD_GLOBAL:
	case OP_INCRE_GLOBAL:
	case OP_DECRE_GLOBAL:
	case OP_SAVE_GLOBAL:
	case OP_LOAD_OBJECT:
	case OP_INCRE_OBJECT:
	case OP_DECRE_OBJECT:
	case OP_LOAD_ATTR:
	case OP_INCRE_ATTR:
	case OP_DECRE_ATTR:
	case OP_SAVE_OBJECT:
	case OP_SAVE_ATTR:
		return 2;
	case OP_NEW_OBJECT:
		return 4;
	case OP_SAVE_VECTOR_COMPONENT:
	case OP_CALL_SUPER_METHOD:
		return 3;
	case OP_RET:
	case OP_RET_VAL:
	case OP_PUSH_NULL:
	case OP_LOAD_THIS:
	case OP_NEG:
		return 0;
	default:
		return -1;
	}
}

VMOPType vm_operand_type(std::string_view type) {
	if (type == "void" || type == "null")
		return VMOPType::OPRND_NULL;

	if (type == "string")
		return VMOPType::OPRND_String;

	if (type == "int")
		return VMOPType::OPRND_INT32;

	if (type == "float")
		return VMOPType::OPRND_FLOAT32;

	if (type == "double")
		return VMOPType::OPRND_FLOAT64;

	if (type == "bool")
		return VMOPType::OPRND_BOOL;

	if (type == "char")
		return VMOPType::OPRND_CHAR16;

	if (type == "vector")
		return VMOPType::OPRND_VEC;

	return VMOPType::OPRND_ADDRESS;
}
