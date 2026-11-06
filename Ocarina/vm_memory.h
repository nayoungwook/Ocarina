#pragma once

#include "code_data.h"
#include "vm_syscall.h"
#include "vm_opcode.h"

#include <deque>
#include <functional>
#include <optional>

struct VMStorage {
	unsigned size = 0;
	std::map<unsigned, VMOperand> values;
	std::map<unsigned, VMOPType> types;

	VMOperand load(unsigned offset, unsigned size) const;
	void save(unsigned offset, unsigned size, VMOperand operand);
	void increment(unsigned offset, unsigned size, bool decrease);
};

struct VMHeapObject {
	unsigned object_id = 0;
	bool is_array = false;
	VMStorage data;
	std::vector<VMOperand> elements;
};

struct VMFrame {
	explicit VMFrame(const VMFunctionData &function_data);
	std::reference_wrapper<const VMFunctionData> function_data;
	VMStorage data;
	unsigned stack_pointer = 0;
	unsigned heap_index = 0;
	std::size_t instruction_index = 0;
	std::size_t operand_base = 0;
	std::optional<VMOperand> return_value;
	std::optional<VMOperand> result_override;
};
