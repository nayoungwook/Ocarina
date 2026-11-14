#pragma once

#include "vm_syscall.h"
#include "vm_opcode.h"
#include "ovm_panic.h"

#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>

unsigned get_operand_size(VMOPType op_type);
int get_operand_level(VMOPType op_type);
VMOperand convert_argument(VMOperand operand, VMOPType target);
VMOperand vm_numeric_operation(std::uint8_t opcode, const VMOperand &lhs, const VMOperand &rhs);
VMOperand vm_negate(VMOperand operand);
bool vm_condition(const VMOperand &operand);

template<class T>
inline bool vm_scalar_operation(std::uint8_t opcode, T a, T b, VMOperand &result) {
	const auto save = [&](T value) {
		if constexpr (std::is_same_v<T, std::int32_t>) result.val.i32 = value;
		else if constexpr (std::is_same_v<T, float>) result.val.f32 = value;
		else result.val.f64 = value;
	};
	const auto boolean = [&](bool value) {
		result.op_type = VMOPType::OPRND_BOOL;
		result.val.i32 = value;
	};
	switch (opcode) {
	case OP_ADD:
		if constexpr (std::is_integral_v<T>)
			save(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)));
		else save(a + b);
		break;
	case OP_SUB:
		if constexpr (std::is_integral_v<T>)
			save(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b)));
		else save(a - b);
		break;
	case OP_MUL:
		if constexpr (std::is_integral_v<T>)
			save(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)));
		else save(a * b);
		break;
	case OP_DIV:
		if (b == 0) throw OVMPanic("Division by zero.");
		if constexpr (std::is_integral_v<T>) {
			if (a == std::numeric_limits<T>::min() && b == -1) save(a);
			else save(a / b);
		} else save(a / b);
		break;
	case OP_MOD:
		if (b == 0) throw OVMPanic("Remainder by zero.");
		if constexpr (std::is_integral_v<T>) {
			if (a == std::numeric_limits<T>::min() && b == -1) save(0);
			else save(a % b);
		} else save(std::fmod(a, b));
		break;
	case OP_EQUAL: boolean(a == b); break;
	case OP_NOTEQUAL: boolean(a != b); break;
	case OP_GREATER: boolean(a > b); break;
	case OP_LESS: boolean(a < b); break;
	case OP_EQUALGREATER: boolean(a >= b); break;
	case OP_EQUALLESS: boolean(a <= b); break;
	case OP_OR: boolean(a != 0 || b != 0); break;
	case OP_AND: boolean(a != 0 && b != 0); break;
	default: return false;
	}
	return true;
}

inline void vm_apply_numeric_operation(std::uint8_t opcode, VMOperand &lhs, const VMOperand &rhs) {
	if (lhs.op_type == rhs.op_type) {
		switch (lhs.op_type) {
		case VMOPType::OPRND_INT32:
			if (vm_scalar_operation(opcode, lhs.val.i32, rhs.val.i32, lhs)) return;
			break;
		case VMOPType::OPRND_FLOAT32:
			if (vm_scalar_operation(opcode, lhs.val.f32, rhs.val.f32, lhs)) return;
			break;
		case VMOPType::OPRND_FLOAT64:
			if (vm_scalar_operation(opcode, lhs.val.f64, rhs.val.f64, lhs)) return;
			break;
		default: break;
		}
	}
	lhs = vm_numeric_operation(opcode, lhs, rhs);
}
