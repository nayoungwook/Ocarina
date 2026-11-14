#include "vm.h"
#include "vm_opcode.h"
#include "vm_numeric.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include "ovm_panic.h"

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
	assert(operand.op_type == VMOPType::OPRND_INT32);
	VMOperand result = {};
	result.op_type = VMOPType::OPRND_INT32;
	result.val.i32 = operand.val.i32 + (decrease ? -1 : 1);
	save(offset, size, result);
}

VMHierarchyObject &OVM::find_object(const VMOperand &operand) {
	if (operand.op_type != VMOPType::OPRND_ADDRESS || operand.val.i32 <= 0 ||
		static_cast<std::size_t>(operand.val.i32) >= hierarchy.size())
		throw OVMPanic("Invalid or null object reference.");

	return hierarchy.at(static_cast<unsigned>(operand.val.i32));
}

const VMFunctionData &OVM::vm_find_member_function_data(unsigned object_id, unsigned id) const {
	for (std::size_t depth = 0; depth < this->object_data.size(); ++depth) {
		const VMObjectData &object_data = this->object_data.at(object_id);
		const auto found = object_data.function_data.find(id);
		if (found != object_data.function_data.end())
			return found->second;

		if (object_data.parent_id == 0)
			break;
		object_id = object_data.parent_id;
	}
	throw OVMPanic("Member function not found: " + std::to_string(id));
}

void OVM::update_hierarchy() {
	exec_hierarchy("update");
}

void OVM::render_hierarchy() {
	exec_hierarchy("render");
}

void OVM::set_scene(const VMOperand &scene) {
	if (scene.op_type == VMOPType::OPRND_NULL)
		throw OVMPanic("set_scene requires a non-null scene reference.");
	const VMHierarchyObject &instance = find_object(scene);
	if (!object_data.at(instance.object_id).is_scene)
		throw OVMPanic("set_scene requires a scene reference.");
	current_scene = static_cast<unsigned>(scene.val.i32);
}

void OVM::exec_instance_method(unsigned hierarchy_index, std::string_view method) {
	unsigned object_id = hierarchy.at(hierarchy_index).object_id;
	for (std::size_t depth = 0; object_id != 0 && depth < object_data.size(); ++depth) {
		const VMObjectData &object = object_data.at(object_id);
		const auto function = std::find_if(object.function_data.begin(), object.function_data.end(),
			[method](const auto &entry) { return !entry.second.is_constructor && entry.second.name == method; });
		if (function != object.function_data.end()) {
			vm_exec_function(function->second, hierarchy_index);
			return;
		}
		object_id = object.parent_id;
	}
}

void OVM::exec_hierarchy(std::string_view method) {
	if (!syscalls || executing)
		return;

	const std::size_t count = hierarchy.size();
	executing = true;
	try {
		if (current_scene != 0)
			exec_instance_method(current_scene, method);
		for (unsigned index = 1; index < count; ++index) {
			if (!object_data.at(hierarchy.at(index).object_id).is_scene)
				exec_instance_method(index, method);
		}
		executing = false;
	}
	catch (...) {
		clear_execution();
		throw;
	}
}

void OVM::vm_exec_object_initializers(unsigned object_id, unsigned hierarchy_index) {
	std::vector<unsigned> parents;
	while (object_id != 0) {
		if (parents.size() >= this->object_data.size())
			throw OVMPanic("Cyclic object inheritance.");
		parents.push_back(object_id);
		object_id = this->object_data.at(object_id).parent_id;
	}

	for (auto parent = parents.rbegin(); parent != parents.rend(); ++parent) {
		for (const auto &[id, variable] : this->object_data.at(*parent).variable_data) {
			VMOperand operand;
			operand.op_type = variable.operand_type == VMOPType::OPRND_TEXTURE
				? VMOPType::OPRND_NULL : variable.operand_type;
			hierarchy.at(hierarchy_index).data.save(variable.offset, variable.size, operand);
		}
	}

	for (auto parent = parents.rbegin(); parent != parents.rend(); ++parent) {
		const VMFunctionData &initializer = this->object_data.at(*parent).initializer;
		if (initializer.code_loaded)
			vm_exec_function(initializer, hierarchy_index);
	}
}

void OVM::exec_object_instruction(const VMInstruction &instruction, unsigned hierarchy_index) {
	if (instruction.opcode == OP_NEW_OBJECT) {
		exec_new_object(instruction);
		return;
	}

	exec_object_member_instruction(instruction, hierarchy_index);
}

void OVM::exec_new_object(const VMInstruction &instruction) {
	const auto &arguments = instruction.operands.u32;
	const VMObjectData &object_data = this->object_data.at(arguments[0]);

	std::vector<VMOperand> operands = update_function_arguments(arguments[3]);
	VMHierarchyObject object;
	object.object_id = object_data.id;
	object.data.resize(object_data.size);

	const unsigned hierarchy_index = hierarchy.store(std::move(object));

	const VMFunctionData *constructor = nullptr;
	if (instruction.operands.i32[2] >= 0) {
		constructor = &object_data.function_data.at(arguments[2]);
		if (!constructor->is_constructor)
			throw OVMPanic("Object constructor expected.");
	}
	else if (!operands.empty())
		throw OVMPanic("Arguments supplied to an object without a constructor.");

	vm_exec_object_initializers(object_data.id, hierarchy_index);
	if (constructor != nullptr)
		vm_exec_function(*constructor, hierarchy_index, operands);

	VMOperand operand;
	operand.op_type = VMOPType::OPRND_ADDRESS;
	operand.val.i32 = static_cast<std::int32_t>(hierarchy_index);
	vm_stack_push(operand);
}

void OVM::exec_object_member_instruction(const VMInstruction &instruction, unsigned hierarchy_index) {
	const auto &arguments = instruction.operands.u32;
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
		throw OVMPanic("Object instruction requires a valid receiver.");

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
