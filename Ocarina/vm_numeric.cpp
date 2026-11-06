#include "vm_numeric.h"
#include "vm_opcode.h"

#include <bit>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

unsigned get_operand_size(VMOPType op_type) {
	switch (op_type) {
	case VMOPType::OPRND_NULL:
		return 0;
	case VMOPType::OPRND_BOOL:
		return 1;
	case VMOPType::OPRND_CHAR16:
		return 2;
	case VMOPType::OPRND_INT32:
	case VMOPType::OPRND_FLOAT32:
		return 4;
	case VMOPType::OPRND_VEC:
		return 12;
	default:
		return 8;
	}
}

int get_operand_level(VMOPType op_type) {
	switch (op_type) {
	case VMOPType::OPRND_INT32:
	case VMOPType::OPRND_BOOL:
	case VMOPType::OPRND_CHAR16:
		return 0;
	case VMOPType::OPRND_FLOAT32:
		return 1;
	case VMOPType::OPRND_FLOAT64:
		return 2;
	case VMOPType::OPRND_VEC:
		return 3;
	default:
		return -1;
	}
}

static double unpack_numeric_operand(const VMOperand &operand) {
	switch (operand.op_type) {
	case VMOPType::OPRND_INT32:
	case VMOPType::OPRND_BOOL:
	case VMOPType::OPRND_CHAR16:
		return operand.val.i32;
	case VMOPType::OPRND_FLOAT32:
		return operand.val.f32;
	case VMOPType::OPRND_FLOAT64:
		return operand.val.f64;
	default:
		throw std::runtime_error("Expected a numeric operand.");
	}
}

VMOperand convert_argument(VMOperand operand, VMOPType target) {
	if (operand.op_type == target)
		return operand;

	if (operand.op_type == VMOPType::OPRND_NULL &&
		(target == VMOPType::OPRND_ADDRESS || target == VMOPType::OPRND_WINDOW)) {
		operand.op_type = target;
		return operand;
	}
	if (operand.op_type == VMOPType::OPRND_NULL && target == VMOPType::OPRND_String)
		return operand;

	if (get_operand_level(operand.op_type) < 0 || get_operand_level(target) < 0)
		throw std::runtime_error("Incompatible operand types.");

	VMOperand result;
	result.op_type = target;

	if (target == VMOPType::OPRND_VEC) {
		result.val.v.fill(static_cast<float>(unpack_numeric_operand(operand)));
		return result;
	}

	const double value = unpack_numeric_operand(operand);
	if (target == VMOPType::OPRND_FLOAT64)
		result.val.f64 = value;
	else if (target == VMOPType::OPRND_FLOAT32)
		result.val.f32 = static_cast<float>(value);
	else if (target == VMOPType::OPRND_BOOL)
		result.val.i32 = value != 0;
	else {
		if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() ||
			value > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("Integer conversion overflow.");

		result.val.i32 = static_cast<std::int32_t>(value);
		if (target == VMOPType::OPRND_CHAR16)
			result.val.i32 = static_cast<std::uint16_t>(result.val.i32);
	}

	return result;
}

bool vm_condition(const VMOperand &operand) {
	if (operand.op_type == VMOPType::OPRND_NULL)
		return false;
	if (operand.op_type == VMOPType::OPRND_ADDRESS || operand.op_type == VMOPType::OPRND_String)
		return operand.val.i32 != 0;

	return unpack_numeric_operand(operand) != 0;
}

template <typename T> static T numeric_operation(std::uint8_t opcode, T lhs, T rhs) {
	switch (opcode) {
	case OP_ADD:
		return lhs + rhs;
	case OP_SUB:
		return lhs - rhs;
	case OP_MUL:
		return lhs * rhs;
	case OP_DIV:
		if (rhs == 0)
			throw std::runtime_error("Division by zero.");
		return lhs / rhs;
	case OP_EQUAL:
		return lhs == rhs;
	case OP_NOTEQUAL:
		return lhs != rhs;
	case OP_GREATER:
		return lhs > rhs;
	case OP_LESS:
		return lhs < rhs;
	case OP_EQUALGREATER:
		return lhs >= rhs;
	case OP_EQUALLESS:
		return lhs <= rhs;
	case OP_OR:
		return lhs != 0 || rhs != 0;
	case OP_AND:
		return lhs != 0 && rhs != 0;
	case OP_MOD:
		if (rhs == 0)
			throw std::runtime_error("Remainder by zero.");
		return static_cast<T>(std::fmod(lhs, rhs));
	default:
		throw std::runtime_error("Unknown numeric operator.");
	}
}

VMOperand vm_numeric_operation(std::uint8_t opcode, const VMOperand &lhs, const VMOperand &rhs) {
	const bool boolean =
		(opcode >= OP_EQUAL && opcode <= OP_EQUALLESS) || opcode == OP_OR || opcode == OP_AND;
	const int lhs_level = get_operand_level(lhs.op_type);
	const int rhs_level = get_operand_level(rhs.op_type);
	VMOperand result;

	if (lhs_level < 0 || rhs_level < 0) {
		if ((opcode != OP_EQUAL && opcode != OP_NOTEQUAL) ||
			(lhs.op_type != rhs.op_type && lhs.op_type != VMOPType::OPRND_NULL &&
				rhs.op_type != VMOPType::OPRND_NULL))
			throw std::runtime_error("Unsupported operand types for numeric operator.");

		const std::uint64_t lhs_value = lhs.op_type == VMOPType::OPRND_WINDOW ? lhs.val.u64 :
			static_cast<std::uint32_t>(lhs.val.i32);
		const std::uint64_t rhs_value = rhs.op_type == VMOPType::OPRND_WINDOW ? rhs.val.u64 :
			static_cast<std::uint32_t>(rhs.val.i32);
		const bool equal = lhs_value == rhs_value;
		result.op_type = VMOPType::OPRND_BOOL;
		result.val.i32 = opcode == OP_EQUAL ? equal : !equal;
		return result;
	}

	const int level = std::max(lhs_level, rhs_level);
	if (level == 3) {
		if (opcode == OP_OR || opcode == OP_AND)
			throw std::runtime_error("Logical operators do not support vectors.");

		const VMOperand a = convert_argument(lhs, VMOPType::OPRND_VEC);
		const VMOperand b = convert_argument(rhs, VMOPType::OPRND_VEC);
		result.op_type = boolean ? VMOPType::OPRND_BOOL : VMOPType::OPRND_VEC;
		result.val.i32 = opcode == OP_NOTEQUAL ? false : true;

		for (unsigned i = 0; i < 3; ++i) {
			const float value = numeric_operation(opcode, a.val.v[i], b.val.v[i]);
			if (boolean) {
				if (opcode == OP_NOTEQUAL)
					result.val.i32 |= value != 0;
				else
					result.val.i32 &= value != 0;
			} else
				result.val.v[i] = value;
		}
		return result;
	}

	if (boolean)
		result.op_type = VMOPType::OPRND_BOOL;
	else if (level == 2)
		result.op_type = VMOPType::OPRND_FLOAT64;
	else if (level == 1)
		result.op_type = VMOPType::OPRND_FLOAT32;
	else
		result.op_type = VMOPType::OPRND_INT32;

	if (level == 0 && !boolean) {
		const std::int64_t a = lhs.val.i32, b = rhs.val.i32;
		std::int64_t value;
		if (opcode == OP_MOD) {
			if (b == 0)
				throw std::runtime_error("Remainder by zero.");
			value = a % b;
		} else
			value = numeric_operation(opcode, a, b);

		result.val.i32 = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	} else if (level == 1) {
		const float value = numeric_operation(opcode,
			static_cast<float>(unpack_numeric_operand(lhs)),
			static_cast<float>(unpack_numeric_operand(rhs)));
		if (boolean)
			result.val.i32 = value != 0;
		else
			result.val.f32 = value;
	} else {
		const double value =
			numeric_operation(opcode, unpack_numeric_operand(lhs), unpack_numeric_operand(rhs));
		if (boolean)
			result.val.i32 = value != 0;
		else
			result.val.f64 = value;
	}

	return result;
}

VMOperand vm_negate(VMOperand operand) {
	switch (operand.op_type) {
	case VMOPType::OPRND_INT32:
		operand.val.i32 =
			std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(operand.val.i32));
		break;
	case VMOPType::OPRND_FLOAT32:
		operand.val.f32 = -operand.val.f32;
		break;
	case VMOPType::OPRND_FLOAT64:
		operand.val.f64 = -operand.val.f64;
		break;
	case VMOPType::OPRND_VEC:
		for (float &value : operand.val.v)
			value = -value;
		break;
	default:
		throw std::runtime_error("Invalid operand for negation.");
	}
	return operand;
}
