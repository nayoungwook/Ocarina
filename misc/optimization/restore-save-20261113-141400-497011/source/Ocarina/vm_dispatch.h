#pragma once

#include "vm.h"
#include "vm_numeric.h"

inline void OVM::exec_instruction(const VMInstruction &instruction,
	std::size_t &instruction_index, unsigned hierarchy_index) {
	const auto &arguments = instruction.operands.u32;
	switch (instruction.opcode) {
	case OP_FAST_LOCAL_LOAD_I4:
		vm_stack_load_numeric<VMOPType::OPRND_INT32>(stack, stack_frame + arguments[0]); return;
	case OP_FAST_LOCAL_LOAD_F4:
		vm_stack_load_numeric<VMOPType::OPRND_FLOAT32>(stack, stack_frame + arguments[0]); return;
	case OP_FAST_LOCAL_LOAD_F8:
		vm_stack_load_numeric<VMOPType::OPRND_FLOAT64>(stack, stack_frame + arguments[0]); return;
	case OP_FAST_LOCAL_SAVE_4:
		vm_stack_save_numeric<4>(stack, stack_frame + arguments[0]); return;
	case OP_FAST_LOCAL_SAVE_8:
		vm_stack_save_numeric<8>(stack, stack_frame + arguments[0]); return;
	case OP_FAST_OBJECT_LOAD_I4:
	case OP_FAST_OBJECT_LOAD_F4:
	case OP_FAST_OBJECT_LOAD_F8:
	case OP_FAST_OBJECT_SAVE_4:
	case OP_FAST_OBJECT_SAVE_8: {
		if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
			throw OVMPanic("Object instruction requires a valid receiver.");
		VMStorage &storage = hierarchy.at(hierarchy_index).data;
		switch (instruction.opcode) {
		case OP_FAST_OBJECT_LOAD_I4: vm_stack_load_numeric<VMOPType::OPRND_INT32>(storage, arguments[0]); break;
		case OP_FAST_OBJECT_LOAD_F4: vm_stack_load_numeric<VMOPType::OPRND_FLOAT32>(storage, arguments[0]); break;
		case OP_FAST_OBJECT_LOAD_F8: vm_stack_load_numeric<VMOPType::OPRND_FLOAT64>(storage, arguments[0]); break;
		case OP_FAST_OBJECT_SAVE_4: vm_stack_save_numeric<4>(storage, arguments[0]); break;
		case OP_FAST_OBJECT_SAVE_8: vm_stack_save_numeric<8>(storage, arguments[0]); break;
		}
		return;
	}
	case OP_SP_LOAD:
		vm_stack_push(stack.load(stack_frame + arguments[0], arguments[1]));
		return;
	case OP_SP_SAVE:
		stack.save(stack_frame + arguments[0], arguments[1], vm_stack_pop());
		return;
	case OP_SP_INCRE:
	case OP_SP_DECRE:
		stack.increment(stack_frame + arguments[0], arguments[1], instruction.opcode == OP_SP_DECRE);
		return;
	case OP_EXPR_OP:
		if (stack_index < 2)
			throw OVMPanic("VM operand stack underflow.");
		vm_apply_numeric_operation(instruction.operands.u8,
			vm_stack[stack_index - 2], vm_stack[stack_index - 1]);
		--stack_index;
		return;
	case OP_CONVERT: {
		if (stack_index == 0)
			throw OVMPanic("VM operand stack underflow.");
		VMOperand &operand = vm_stack[stack_index - 1];
		const VMOPType target = static_cast<VMOPType>(arguments[0]);
		if (operand.op_type != target) operand = convert_argument(operand, target);
		return;
	}
	case OP_LDC_I4:
	case OP_LDC_F4:
	case OP_LDC_F8: {
		if (stack_index >= MAX_STACK_SIZE)
			throw OVMPanic("VM operand stack overflow.");
		VMOperand &operand = vm_stack[stack_index++];
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
		return;
	}
	case OP_GOTO:
		instruction_index = arguments[0];
		return;
	case OP_JE:
	case OP_JNE: {
		const VMOperand condition = vm_stack_pop();
		const bool value = condition.op_type == VMOPType::OPRND_BOOL
			? condition.val.i32 != 0 : vm_condition(condition);
		if (value == (instruction.opcode == OP_JE)) instruction_index = arguments[0];
		return;
	}
	case OP_LOAD_OBJECT:
	case OP_SAVE_OBJECT:
	case OP_INCRE_OBJECT:
	case OP_DECRE_OBJECT: {
		if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
			throw OVMPanic("Object instruction requires a valid receiver.");
		VMStorage &storage = hierarchy.at(hierarchy_index).data;
		if (instruction.opcode == OP_LOAD_OBJECT)
			vm_stack_push(storage.load(arguments[0], arguments[1]));
		else if (instruction.opcode == OP_SAVE_OBJECT)
			storage.save(arguments[0], arguments[1], vm_stack_pop());
		else storage.increment(arguments[0], arguments[1], instruction.opcode == OP_DECRE_OBJECT);
		return;
	}
	default:
		exec_complex_instruction(instruction, hierarchy_index);
	}
}
