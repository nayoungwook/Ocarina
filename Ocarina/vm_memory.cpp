#include "vm.h"
#include "vm_opcode.h"
#include "vm_numeric.h"

#include <stdexcept>

static void check_storage_range(unsigned capacity, unsigned offset, unsigned size) {
	if (size == 0 || size > 16 || offset > capacity || size > capacity - offset)
		throw std::runtime_error("VM storage access is out of bounds.");
}

VMOperand VMStorage::load(unsigned offset, unsigned size) const {
	check_storage_range(this->size, offset, size);
	const auto found = values.find(offset);
	if (found == values.end())
		throw std::runtime_error("Read of an uninitialized VM variable.");
	return found->second;
}

void VMStorage::save(unsigned offset, unsigned size, VMOperand operand) {
	check_storage_range(this->size, offset, size);
	if (types.contains(offset))
		operand = convert_argument(operand, types.at(offset));
	values[offset] = operand;
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

VMHeapObject &CVM::find_heap_object(const VMOperand &operand) {
	if (operand.op_type != VMOPType::OPRND_ADDRESS || operand.val.i32 <= 0 ||
		static_cast<std::size_t>(operand.val.i32) >= heap_mapper.size())
		throw std::runtime_error("Invalid or null object/array reference.");

	return heap_mapper.at(static_cast<unsigned>(operand.val.i32));
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

const VMVariableData &CVM::vm_find_heap_variable_data(unsigned heap_index, unsigned id) const {
	const VMHeapObject &object = heap_mapper.at(heap_index);
	if (object.is_array)
		throw std::runtime_error("Array used as an object.");

	unsigned object_id = object.object_id;
	for (std::size_t depth = 0; depth < this->object_data.size(); ++depth) {
		const VMObjectData &object_data = this->object_data.at(object_id);
		const auto found = object_data.variable_data.find(id);
		if (found != object_data.variable_data.end())
			return found->second;

		if (object_data.parent_id == 0)
			break;
		object_id = object_data.parent_id;
	}
	throw std::runtime_error("Object variable not found: " + std::to_string(id));
}

void CVM::vm_exec_object_initializers(unsigned object_id, unsigned heap_index) {
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
			heap_mapper.at(heap_index).data.save(variable.offset, variable.size, operand);
		}
	}

	// Frames execute from the back: parent initializers run before child initializers.
	for (unsigned parent : parents) {
		const VMFunctionData &initializer = this->object_data.at(parent).initializer;
		if (initializer.code_loaded)
			push_frame(initializer, {}, heap_index);
	}
}

void CVM::exec_object_instruction(const VMInstruction &instruction, unsigned heap_index) {
	const auto &arguments = instruction.operands.u32;
	if (instruction.opcode == OP_NEW_OBJECT) {
		const VMObjectData &object_data = this->object_data.at(arguments[0]);
		if (!object_data.layout_known || object_data.size != arguments[1])
			throw std::runtime_error("Object allocation size does not match its metadata.");

		std::vector<VMOperand> operands = update_function_arguments(arguments[3]);
		VMHeapObject object;
		object.object_id = object_data.id;
		object.data.size = object_data.size;
		if (heap_mapper.size() >= static_cast<std::size_t>(INT32_MAX))
			throw std::runtime_error("Too many VM heap objects.");
		heap_index = static_cast<unsigned>(heap_mapper.size());
		heap_mapper.push_back(std::move(object));
		const std::size_t first_frame = frames.size();

		if (instruction.operands.i32[2] >= 0) {
			const VMFunctionData &constructor = object_data.function_data.at(arguments[2]);
			if (!constructor.is_constructor)
				throw std::runtime_error("Object constructor expected.");
			push_frame(constructor, std::move(operands), heap_index);
		} else if (!operands.empty())
			throw std::runtime_error("Arguments supplied to an object without a constructor.");

		VMOperand operand;
		operand.op_type = VMOPType::OPRND_ADDRESS;
		operand.val.i32 = static_cast<std::int32_t>(heap_index);
		vm_exec_object_initializers(object_data.id, heap_index);
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
		find_heap_object(object);
		heap_index = static_cast<unsigned>(object.val.i32);
	}
	if (heap_index == 0 || heap_index >= heap_mapper.size() || heap_mapper.at(heap_index).is_array)
		throw std::runtime_error("Object instruction requires a valid receiver.");

	const bool latest = this->loaded_format == IRFormat::Chestnut;
	const unsigned offset = arguments[latest ? 1 : 0];
	const unsigned size = arguments[latest ? 2 : 1];
	VMOPType type = VMOPType::OPRND_NULL;
	if (latest) {
		const VMVariableData &variable_data = vm_find_heap_variable_data(heap_index, arguments[0]);
		if (variable_data.offset != offset || variable_data.size != size)
			throw std::runtime_error("Object variable layout does not match its metadata.");
		type = variable_data.operand_type;
	}

	VMStorage &storage = heap_mapper.at(heap_index).data;
	if (save) {
		if (latest)
			value = convert_argument(value, type);
		storage.save(offset, size, value);
	} else if (instruction.opcode == OP_LOAD_OBJECT || instruction.opcode == OP_LOAD_ATTR) {
		value = storage.load(offset, size);
		if (value.op_type == VMOPType::OPRND_ADDRESS && value.val.i32 == 0)
			value.op_type = VMOPType::OPRND_NULL;
		vm_stack_push(value);
	} else
		storage.increment(offset,
			size,
			instruction.opcode == OP_DECRE_OBJECT || instruction.opcode == OP_DECRE_ATTR);
}

void CVM::exec_array_instruction(const VMInstruction &instruction) {
	if (instruction.opcode == OP_NEW_ARRAY) {
		const unsigned count = instruction.operands.u32[1];
		if (count > vm_stack.size())
			throw std::runtime_error("Not enough array initializer values.");

		VMHeapObject object;
		object.is_array = true;
		object.elements.resize(count);
		for (unsigned i = count; i > 0; --i)
			object.elements[i - 1] = vm_stack_pop();

		if (heap_mapper.size() >= static_cast<std::size_t>(INT32_MAX))
			throw std::runtime_error("Too many VM heap objects.");
		VMOperand operand;
		operand.op_type = VMOPType::OPRND_ADDRESS;
		operand.val.i32 = static_cast<std::int32_t>(heap_mapper.size());
		heap_mapper.push_back(std::move(object));
		vm_stack_push(operand);
		return;
	}

	VMOperand value;
	if (instruction.opcode == OP_ARRAY_SAVE || instruction.opcode == OP_ARRAY_PUSH)
		value = vm_stack_pop();

	VMOperand index;
	if (instruction.opcode == OP_ARRAY_LOAD || instruction.opcode == OP_ARRAY_SAVE ||
		instruction.opcode == OP_ARRAY_REMOVE)
		index = vm_stack_pop();

	VMHeapObject &array = find_heap_object(vm_stack_pop());
	if (!array.is_array)
		throw std::runtime_error("Array instruction requires an array.");

	if (instruction.opcode == OP_ARRAY_LENGTH) {
		value.op_type = VMOPType::OPRND_INT32;
		value.val.i32 = static_cast<std::int32_t>(array.elements.size());
		vm_stack_push(value);
	} else if (instruction.opcode == OP_ARRAY_PUSH) {
		if (array.elements.size() >= static_cast<std::size_t>(INT32_MAX))
			throw std::runtime_error("Array length overflow.");
		array.elements.push_back(value);
	} else {
		if (index.op_type != VMOPType::OPRND_INT32 || index.val.i32 < 0 ||
			static_cast<std::size_t>(index.val.i32) >= array.elements.size())
			throw std::runtime_error("Array index is out of bounds.");

		const auto position = static_cast<unsigned>(index.val.i32);
		if (instruction.opcode == OP_ARRAY_LOAD)
			vm_stack_push(array.elements.at(position));
		else if (instruction.opcode == OP_ARRAY_SAVE)
			array.elements.at(position) = value;
		else
			array.elements.erase(array.elements.begin() + position);
	}
}
