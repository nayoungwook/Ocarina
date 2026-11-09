#include "vm.h"
#include "vm_opcode.h"
#include "vm_numeric.h"

#include "ovm_panic.h"

void OVM::exec_instruction(const VMInstruction &instruction,
	std::size_t &instruction_index,
	VMFrame &frame,
	unsigned hierarchy_index) {
	const auto &arguments = instruction.operands.u32;

	switch (instruction.opcode) {
	case OP_DUP: {
		const unsigned count = arguments[0];
		if (count == 0 || count > vm_stack.size() - frame.operand_base)
			throw OVMPanic("Invalid operand count for duplication.");
		const std::size_t begin = vm_stack.size() - count;
		for (unsigned i = 0; i < count; ++i)
			vm_stack_push(vm_stack.at(begin + i));
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
	case OP_SP_LOAD:
		vm_stack_push(frame.data.load(arguments[0], arguments[1]));
		break;
	case OP_SP_SAVE:
		frame.data.save(arguments[0], arguments[1], vm_stack_pop());
		break;
	case OP_SP_INCRE:
	case OP_SP_DECRE:
		frame.data.increment(arguments[0], arguments[1], instruction.opcode == OP_SP_DECRE);
		break;
	case OP_SYSCALL: {
		const auto operands = update_function_arguments(arguments[1]);
		const auto result = syscalls->get().handle_syscall(arguments[0],
			operands,
			this->vm_string_pool,
			output->get());
		if (result)
			vm_stack_push(*result);
		break;
	}
	case OP_CALL:
	case OP_CALL_ATTR:
	case OP_CALL_OBJECT:
	case OP_CALL_GLOBAL: {
		if (instruction.opcode == OP_CALL_ATTR) {
			if (arguments[1] >= vm_stack.size())
				throw OVMPanic("Method receiver is missing.");
			const VMOperand receiver = vm_stack.at(vm_stack.size() - arguments[1] - 1);
			const VMHierarchyObject &object = find_object(receiver);
			call_function(vm_find_member_function_data(object.object_id, arguments[0]),
				arguments[1],
				static_cast<unsigned>(receiver.val.i32),
				true);
		} else if (instruction.opcode == OP_CALL_OBJECT) {
			if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
				throw OVMPanic("Member call requires an object.");
			call_function(
				vm_find_member_function_data(hierarchy.at(hierarchy_index).object_id, arguments[0]),
				arguments[1],
				hierarchy_index);
		} else
			call_function(this->function_data.at(arguments[0]), arguments[1]);
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
	case OP_LOAD_STR: {
		if (!this->vm_string_pool.str_pool.contains(arguments[0]))
			throw OVMPanic("Invalid string pool index.");
		VMOperand operand;
		operand.op_type = VMOPType::OPRND_String;
		operand.val.i32 = static_cast<std::int32_t>(arguments[0]);
		vm_stack_push(operand);
		break;
	}
	case OP_RET:
		break;
	case OP_RET_VAL:
		frame.return_value = vm_stack_pop();
		break;
	case OP_EXPR_OP: {
		const VMOperand rhs = vm_stack_pop();
		const VMOperand lhs = vm_stack_pop();
		vm_stack_push(vm_numeric_operation(instruction.operands.u8, lhs, rhs));
		break;
	}
	case OP_CONVERT:
		vm_stack_push(convert_argument(vm_stack_pop(), static_cast<VMOPType>(arguments[0])));
		break;
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
	case OP_GOTO:
		instruction_index = arguments[0];
		break;
	case OP_JE:
	case OP_JNE: {
		const bool cond = vm_condition(vm_stack_pop());
		if (cond == (instruction.opcode == OP_JE))
			instruction_index = arguments[0];
		break;
	}
	case OP_NEG:
		vm_stack_push(vm_negate(vm_stack_pop()));
		break;
	case OP_LDC_I4:
	case OP_LDC_F4:
	case OP_LDC_F8: {
		VMOperand operand;
		if (instruction.opcode == OP_LDC_I4) {
			operand.op_type = VMOPType::OPRND_INT32;
			operand.val.i32 = instruction.operands.i32[0];
		} else if (instruction.opcode == OP_LDC_F4) {
			operand.op_type = VMOPType::OPRND_FLOAT32;
			operand.val.f32 = instruction.operands.f32;
		} else {
			operand.op_type = VMOPType::OPRND_FLOAT64;
			operand.val.f64 = instruction.operands.f64;
		}
		vm_stack_push(operand);
		break;
	}
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
	case OP_LOAD_OBJECT:
	case OP_INCRE_OBJECT:
	case OP_DECRE_OBJECT:
	case OP_LOAD_ATTR:
	case OP_INCRE_ATTR:
	case OP_DECRE_ATTR:
	case OP_SAVE_OBJECT:
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
	default:
		throw OVMPanic("Unknown VM opcode: " + std::to_string(instruction.opcode));
	}
}
