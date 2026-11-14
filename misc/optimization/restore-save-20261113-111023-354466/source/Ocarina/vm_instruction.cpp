#include "vm.h"
#include "vm_opcode.h"
#include "vm_numeric.h"

#include "ovm_panic.h"

void OVM::exec_complex_instruction(const VMInstruction &instruction,
	std::size_t &instruction_index,
	VMFrame &frame,
	unsigned hierarchy_index) {
	const auto &arguments = instruction.operands.u32;
	const auto resolve = [&](unsigned object_id, unsigned function_id) -> const VMFunctionData & {
		if (instruction.cached_function == nullptr || instruction.cached_object != object_id) {
			instruction.cached_function = object_id == 0 ? &function_data.at(function_id)
				: &vm_find_member_function_data(object_id, function_id);
			instruction.cached_object = object_id;
		}
		return *instruction.cached_function;
	};

	switch (instruction.opcode) {
	case OP_DUP: {
		const unsigned count = arguments[0];
		if (stack_index < frame.operand_base || count == 0 || count > stack_index - frame.operand_base)
			throw OVMPanic("Invalid operand count for duplication.");
		if (count > MAX_STACK_SIZE - stack_index)
			throw OVMPanic("VM operand stack overflow.");
		const std::size_t begin = stack_index - count;
		for (unsigned i = 0; i < count; ++i)
			vm_stack[stack_index + i] = vm_stack[begin + i];
		stack_index += count;
		break;
	}
	case OP_SP_PUSH:
		if (frame.stack_pointer > frame.data.size ||
			arguments[0] > frame.data.size - frame.stack_pointer)
			throw OVMPanic("VM stack frame overflow.");
		frame.stack_pointer += arguments[0];
		break;
	case OP_SP_POP:
		if (arguments[0] > frame.stack_pointer)
			throw OVMPanic("VM stack frame underflow.");
		frame.stack_pointer -= arguments[0];
		break;
	case OP_SYSCALL: {
		const auto operands = update_function_arguments(arguments[1]);

		const auto result = syscalls->handle_syscall(arguments[0],
			operands,
			output->get());

		if (result)
			vm_stack_push(*result);

		break;
	}
	case OP_RENDER:
		exec_render(instruction, hierarchy_index);
		break;
	case OP_CALL:
	case OP_CALL_ATTR:
	case OP_CALL_OBJECT:
	case OP_CALL_GLOBAL: {
		if (instruction.opcode == OP_CALL_ATTR) {
			if (stack_index < frame.operand_base || arguments[1] >= stack_index - frame.operand_base)
				throw OVMPanic("Method receiver is missing.");
			const VMOperand receiver = vm_stack[stack_index - arguments[1] - 1];
			const VMHierarchyObject &object = find_object(receiver);
			call_function(resolve(object.object_id, arguments[0]),
				arguments[1],
				static_cast<unsigned>(receiver.val.i32),
				true);
		}
		else if (instruction.opcode == OP_CALL_OBJECT) {
			if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
				throw OVMPanic("Member call requires an object.");
			call_function(
				resolve(hierarchy.at(hierarchy_index).object_id, arguments[0]),
				arguments[1],
				hierarchy_index);
		}
		else
			call_function(resolve(0, arguments[0]), arguments[1]);
		break;
	}
	case OP_CALL_SUPER: {
		if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
			throw OVMPanic("super requires an object.");
		const VMObjectData &parent = this->object_data.at(arguments[0]);
		const VMFunctionData &constructor = parent.function_data.at(0);
		if (!constructor.is_constructor)
			throw OVMPanic("Parent constructor not found.");
		call_function(constructor, arguments[1], hierarchy_index);
		break;
	}
	case OP_CALL_SUPER_METHOD: {
		if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
			throw OVMPanic("super method call requires an object.");
		const VMFunctionData &method = resolve(arguments[0], arguments[1]);
		if (method.is_constructor)
			throw OVMPanic("super method call requires a method.");
		call_function(method, arguments[2], hierarchy_index);
		break;
	}
	case OP_LOAD_STR: {
		if (!this->vm_string_pool.str_pool.contains(arguments[0]))
			throw OVMPanic("Invalid string pool index.");
		VMOperand operand;
		operand.op_type = VMOPType::OPRND_String;
		operand.val.i32 = static_cast<std::int32_t>(arguments[0]);
		vm_stack_push(operand);
		break;
	}
	case OP_VECTOR_COMPONENT: {
		const VMOperand vector = vm_stack_pop();
		if (vector.op_type != VMOPType::OPRND_VEC || arguments[0] > 2)
			throw OVMPanic("Invalid vector component access.");
		VMOperand component;
		component.op_type = VMOPType::OPRND_FLOAT32;
		component.val.f32 = vector.val.v[arguments[0]];
		vm_stack_push(component);
		break;
	}
	case OP_SAVE_VECTOR_COMPONENT: {
		const VMOperand value = vm_stack_pop();
		VMStorage *storage = nullptr;
		switch (static_cast<VectorStorage>(arguments[0])) {
		case VectorStorage::Local: storage = &frame.data; break;
		case VectorStorage::Global: storage = &globals; break;
		case VectorStorage::Object:
			if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
				throw OVMPanic("Vector member requires an object receiver.");
			storage = &hierarchy.at(hierarchy_index).data;
			break;
		case VectorStorage::Attribute:
			storage = &find_object(vm_stack_pop()).data;
			break;
		default: throw OVMPanic("Invalid vector storage.");
		}
		if (arguments[1] > storage->size || storage->size - arguments[1] < 12 || arguments[2] > 2)
			throw OVMPanic("Invalid vector component store.");
		VMOperand vector = storage->load(arguments[1], 12);
		if (vector.op_type != VMOPType::OPRND_VEC || value.op_type != VMOPType::OPRND_FLOAT32)
			throw OVMPanic("Vector component requires a float value.");
		vector.val.v[arguments[2]] = value.val.f32;
		storage->save(arguments[1], 12, vector);
		break;
	}
	case OP_NEG:
		vm_stack_push(vm_negate(vm_stack_pop()));
		break;
	case OP_LOAD_THIS: {
		if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
			throw OVMPanic("this requires an object receiver.");
		VMOperand receiver;
		receiver.op_type = VMOPType::OPRND_ADDRESS;
		receiver.val.i32 = static_cast<std::int32_t>(hierarchy_index);
		vm_stack_push(receiver);
		break;
	}
	case OP_PUSH_NULL:
		vm_stack_push({});
		break;
	case OP_LOAD_ATTR:
	case OP_INCRE_ATTR:
	case OP_DECRE_ATTR:
	case OP_SAVE_ATTR:
	case OP_NEW_OBJECT:
		exec_object_instruction(instruction, hierarchy_index);
		break;
	case OP_LOAD_GLOBAL:
		vm_stack_push(globals.load(arguments[0], arguments[1]));
		break;
	case OP_INCRE_GLOBAL:
	case OP_DECRE_GLOBAL:
		globals.increment(arguments[0], arguments[1], instruction.opcode == OP_DECRE_GLOBAL);
		break;
	case OP_SAVE_GLOBAL:
		globals.save(arguments[0], arguments[1], vm_stack_pop());
		break;
	case OP_ADD_CHILD: {
		const VMOperand child = vm_stack_pop();
		find_object(child);
		unsigned parent = hierarchy_index;
		if (arguments[0] != 0) {
			const VMOperand receiver = vm_stack_pop();
			find_object(receiver);
			parent = static_cast<unsigned>(receiver.val.i32);
		}
		hierarchy.add_child(parent, static_cast<unsigned>(child.val.i32));
		break;
	}
	case OP_GET_CHILD: {
		const VMOperand index = vm_stack_pop();
		if (index.op_type != VMOPType::OPRND_INT32)
			throw OVMPanic("get_child requires an integer index.");
		unsigned parent = hierarchy_index;
		if (arguments[0] != 0) {
			const VMOperand receiver = vm_stack_pop();
			find_object(receiver);
			parent = static_cast<unsigned>(receiver.val.i32);
		}
		VMOperand child;
		child.op_type = VMOPType::OPRND_ADDRESS;
		child.val.i32 = static_cast<std::int32_t>(hierarchy.get_child(parent, index.val.i32));
		vm_stack_push(child);
		break;
	}
	case OP_CHILD_SIZE: {
		unsigned parent = hierarchy_index;
		if (arguments[0] != 0) {
			const VMOperand receiver = vm_stack_pop();
			find_object(receiver);
			parent = static_cast<unsigned>(receiver.val.i32);
		}

		VMOperand child;
		child.op_type = VMOPType::OPRND_INT32;
		child.val.i32 = static_cast<std::int32_t>(hierarchy.get_child_size(parent));
		vm_stack_push(child);
		break;
	}
	default:
		throw OVMPanic("Unknown VM opcode: " + std::to_string(instruction.opcode));
	}
}
