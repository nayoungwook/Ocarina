#pragma once

#include "code_data.h"
#include "vm_syscall.h"
#include "vm_opcode.h"

#include <deque>
#include <functional>
#include <optional>

struct VMStorage {
	unsigned size = 0;
	std::vector<byte> values;
	std::vector<VMOPType> types;

	void resize(unsigned capacity);
	VMOperand load(unsigned offset, unsigned size) const;
	void save(unsigned offset, unsigned size, VMOperand operand);
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
