#include "vm.h"
#include "vm_opcode.h"
#include "vm_numeric.h"

#include <cstring>
#include <stdexcept>

void VMStorage::resize(unsigned capacity) {
	size = capacity;
	values.resize(capacity);
	types.resize(capacity);
}

VMOperand VMStorage::load(unsigned offset, unsigned size) const {
	VMOperand operand;
	operand.op_type = types[offset];
	std::memcpy(&operand.val, values.data() + offset, size);
	return operand;
}

void VMStorage::save(unsigned offset, unsigned size, VMOperand operand) {
	std::memcpy(values.data() + offset, &operand.val, size);
	types[offset] = operand.op_type;
}

void VMStorage::increment(unsigned offset, unsigned size, bool decrease) {
	VMOperand operand = load(offset, size);
	VMOperand one;
	one.op_type = VMOPType::OPRND_INT32;
	one.val.i32 = 1;
	const VMOPType target = operand.op_type;
	operand = vm_numeric_operation(decrease ? OP_SUB : OP_ADD, operand, one);
	save(offset, size, convert_argument(operand, target));
}

VMHierarchyObject &CVM::find_object(const VMOperand &operand) {
	if (operand.op_type != VMOPType::OPRND_ADDRESS || operand.val.i32 <= 0 ||
		static_cast<std::size_t>(operand.val.i32) >= hierarchy.size())
		throw std::runtime_error("Invalid or null object reference.");

	return hierarchy.at(static_cast<unsigned>(operand.val.i32));
}

const VMFunctionData &CVM::vm_find_member_function_data(unsigned object_id, unsigned id) const {
	for (std::size_t depth = 0; depth < this->object_data.size(); ++depth) {
		const VMObjectData &object_data = this->object_data.at(object_id);
		const auto found = object_data.function_data.find(id);
		if (found != object_data.function_data.end())
			return found->second;

		if (object_data.parent_id == 0)
			break;
		object_id = object_data.parent_id;
	}
	throw std::runtime_error("Member function not found: " + std::to_string(id));
}

void CVM::vm_exec_object_initializers(unsigned object_id, unsigned hierarchy_index) {
	std::vector<unsigned> parents;
	while (object_id != 0) {
		if (parents.size() >= this->object_data.size())
			throw std::runtime_error("Cyclic object inheritance.");
		parents.push_back(object_id);
		object_id = this->object_data.at(object_id).parent_id;
	}

	for (auto parent = parents.rbegin(); parent != parents.rend(); ++parent) {
		for (const auto &[id, variable] : this->object_data.at(*parent).variable_data) {
			VMOperand operand;
			operand.op_type = variable.operand_type;
			hierarchy.at(hierarchy_index).data.save(variable.offset, variable.size, operand);
		}
	}

	// Frames execute from the back: parent initializers run before child initializers.
	for (unsigned parent : parents) {
		const VMFunctionData &initializer = this->object_data.at(parent).initializer;
		if (initializer.code_loaded)
			push_frame(initializer, {}, hierarchy_index);
	}
}

void CVM::exec_object_instruction(const VMInstruction &instruction, unsigned hierarchy_index) {
	const auto &arguments = instruction.operands.u32;

	if (instruction.opcode == OP_NEW_OBJECT) {
		const VMObjectData &object_data = this->object_data.at(arguments[0]);

		std::vector<VMOperand> operands = update_function_arguments(arguments[3]);
		VMHierarchyObject object;
		object.object_id = object_data.id;
		object.data.resize(object_data.size);

		hierarchy_index = hierarchy.store(std::move(object));
		const std::size_t first_frame = frames.size();

		if (instruction.operands.i32[2] >= 0) {
			const VMFunctionData &constructor = object_data.function_data.at(arguments[2]);
			if (!constructor.is_constructor)
				throw std::runtime_error("Object constructor expected.");
			push_frame(constructor, std::move(operands), hierarchy_index);
		}
		else if (!operands.empty())
			throw std::runtime_error("Arguments supplied to an object without a constructor.");

		VMOperand operand;
		operand.op_type = VMOPType::OPRND_ADDRESS;
		operand.val.i32 = static_cast<std::int32_t>(hierarchy_index);
		vm_exec_object_initializers(object_data.id, hierarchy_index);
		if (frames.size() > first_frame)
			frames.at(first_frame).result_override = operand;
		else
			vm_stack_push(operand);

		return;
	}

	const bool attr = instruction.opcode == OP_LOAD_ATTR || instruction.opcode == OP_INCRE_ATTR ||
		instruction.opcode == OP_DECRE_ATTR || instruction.opcode == OP_SAVE_ATTR;
	const bool save = instruction.opcode == OP_SAVE_OBJECT || instruction.opcode == OP_SAVE_ATTR;
	VMOperand value;
	if (save)
		value = vm_stack_pop();

	if (attr) {
		const VMOperand object = vm_stack_pop();
		find_object(object);
		hierarchy_index = static_cast<unsigned>(object.val.i32);
	}
	if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
		throw std::runtime_error("Object instruction requires a valid receiver.");

	const unsigned offset = arguments[0];
	const unsigned size = arguments[1];

	VMStorage &storage = hierarchy.at(hierarchy_index).data;
	if (save) {
		storage.save(offset, size, value);
	}
	else if (instruction.opcode == OP_LOAD_OBJECT || instruction.opcode == OP_LOAD_ATTR) {
		vm_stack_push(storage.load(offset, size));
	}
	else
		storage.increment(offset,
			size,
			instruction.opcode == OP_DECRE_OBJECT ||
			instruction.opcode == OP_DECRE_ATTR);
}
