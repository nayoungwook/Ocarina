#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

inline constexpr unsigned VM_INSTRUCTION_MAX_OPERANDS = 5;
inline constexpr unsigned VM_LABEL_CAPACITY = 4096;

enum class VMOPType : std::uint8_t {
	OPRND_NULL,
	OPRND_String,
	OPRND_INT32,
	OPRND_FLOAT32,
	OPRND_FLOAT64,
	OPRND_BOOL,
	OPRND_ADDRESS,
	OPRND_CHAR16,
	OPRND_VEC,
};

struct VMInstructionOperands {
	// Separate fields avoid reading inactive C++ union members.
	std::uint8_t u8 = 0;
	std::array<std::uint32_t, VM_INSTRUCTION_MAX_OPERANDS> u32{};
	std::array<std::int32_t, VM_INSTRUCTION_MAX_OPERANDS> i32{};
	float f32 = 0;
	double f64 = 0;
};

struct VMInstruction {
	std::uint8_t opcode = 0;
	VMInstructionOperands operands;
};

struct VMFunctionData {
	unsigned id = 0, stack_size = 0;
	std::string name, return_type = "void";
	std::vector<std::string> argument_types;
	std::vector<unsigned> argument_sizes;
	std::map<unsigned, VMOPType> local_types;
	unsigned argument_count = 0;
	bool is_constructor = false, code_loaded = false;
	std::vector<VMInstruction> instructions;
};

struct VMVariableData {
	unsigned id = 0, offset = 0, size = 0;
	VMOPType operand_type = VMOPType::OPRND_NULL;
	std::string name, type;
};

struct VMObjectData {
	unsigned id = 0, parent_id = 0, size = 0;
	std::string name;
	bool parent_resolved = false;
	VMFunctionData initializer;
	std::map<unsigned, VMFunctionData> function_data;
	std::map<unsigned, VMVariableData> variable_data;
};

struct VMStringPool {
	std::map<unsigned, std::string> str_pool;
	unsigned size = 0;
};

int vm_instruction_operand_count(std::uint8_t opcode);
VMOPType vm_operand_type(std::string_view type);
