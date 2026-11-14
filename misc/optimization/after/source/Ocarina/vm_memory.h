#pragma once

#include "code_data.h"
#include "vm_syscall.h"
#include "vm_opcode.h"

#include <deque>
#include <cstring>
#include <functional>
#include <optional>

struct VMStorage {
	unsigned size = 0;
	std::vector<byte> values;
	std::vector<VMOPType> types;

	void resize(unsigned capacity);
	static void copy_value(void *destination, const void *source, unsigned count) {
		switch (count) {
		case 1: std::memcpy(destination, source, 1); break;
		case 2: std::memcpy(destination, source, 2); break;
		case 4: std::memcpy(destination, source, 4); break;
		case 8: std::memcpy(destination, source, 8); break;
		case 12: std::memcpy(destination, source, 12); break;
		default: std::memcpy(destination, source, count); break;
		}
	}
	VMOperand load(unsigned offset, unsigned count) const {
		VMOperand operand;
		operand.op_type = types[offset];
		copy_value(&operand.val, values.data() + offset, count);
		return operand;
	}
	void save(unsigned offset, unsigned count, VMOperand operand) {
		copy_value(values.data() + offset, &operand.val, count);
		types[offset] = operand.op_type;
	}
	void increment(unsigned offset, unsigned size, bool decrease);
};

struct VMHierarchyObject {
	unsigned object_id = 0;
	VMStorage data;
	unsigned parent = 0;
	std::vector<unsigned> children;
};

class VMHierarchy {
public:
	VMHierarchy() : objects(1) {}
	void reset();
	unsigned store(VMHierarchyObject object);
	void add_child(unsigned parent, unsigned child);
	unsigned get_child(unsigned parent, std::int32_t index) const;
	unsigned get_child_size(unsigned parent) const;
	VMHierarchyObject &at(unsigned index) { return objects.at(index); }
	const VMHierarchyObject &at(unsigned index) const { return objects.at(index); }
	std::size_t size() const { return objects.size(); }

private:
	// Index zero is the root container and is never a language object reference.
	std::deque<VMHierarchyObject> objects;
};

struct VMFrame {
	explicit VMFrame(const VMFunctionData &function_data);
	std::reference_wrapper<const VMFunctionData> function_data;
	VMStorage data;
	unsigned stack_pointer = 0;
	unsigned hierarchy_index = 0;
	std::size_t instruction_index = 0;
	std::size_t operand_base = 0;
	std::optional<VMOperand> return_value;
	std::optional<VMOperand> result_override;
};
