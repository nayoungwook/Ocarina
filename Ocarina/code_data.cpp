#include "code_data.h"

int vm_instruction_operand_count(std::uint8_t opcode, IRFormat format) {
	const bool latest = format == IRFormat::Chestnut;

	switch (opcode) {
	case 0x56:
	case 0x57:
	case 0x60:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x97:
	case 0x98:
		return 1;
	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f:
	case 0x61:
	case 0x8c:
	case 0x8d:
	case 0x8e:
	case 0x93:
	case 0x96:
	case 0x9a:
	case 0x9b:
		return 2;
	case 0x85:
	case 0x8a:
	case 0x8b:
	case 0x8f:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x94:
		return latest ? 3 : 2;
	case 0x95:
		return 4;
	case 0x70:
	case 0x84:
	case 0x99:
		return 0;
	case 0x62:
		return latest ? 2 : -1;
	case 0x71:
		return latest ? -1 : 0;
	case 0x77:
		return latest ? 0 : -1;
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

	if (type == "window")
		return VMOPType::OPRND_WINDOW;

	return VMOPType::OPRND_ADDRESS;
}
