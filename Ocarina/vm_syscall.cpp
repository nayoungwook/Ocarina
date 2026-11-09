#include "vm_syscall.h"
#include "vm.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <sstream>
#include "ovm_panic.h"

static float numeric_operand_to_float(const VMOperand &operand) {
	switch (operand.op_type) {
	case VMOPType::OPRND_INT32:
	case VMOPType::OPRND_CHAR16:
	case VMOPType::OPRND_BOOL:
		return static_cast<float>(operand.val.i32);

	case VMOPType::OPRND_FLOAT32:
		return operand.val.f32;

	case VMOPType::OPRND_FLOAT64:
		return static_cast<float>(operand.val.f64);

	default:
		throw OVMPanic("vector components must be numeric.");
	}
}

SyscallContext::SyscallContext(OVM &vm) : vm(vm) {
}

void SyscallContext::print(std::span<const VMOperand> arguments,
	const VMStringPool &vm_string_pool,
	std::ostream &out) const {
	std::ostringstream text;

	for (const VMOperand &op : arguments) {
		switch (op.op_type) {
		case VMOPType::OPRND_String:
			text << vm_string_pool.str_pool.at(static_cast<unsigned>(op.val.i32));
			break;

		case VMOPType::OPRND_INT32:
		case VMOPType::OPRND_ADDRESS:
		case VMOPType::OPRND_BOOL:
			text << op.val.i32;
			break;

		case VMOPType::OPRND_CHAR16:
			text << static_cast<char>(static_cast<unsigned char>(op.val.i32));
			break;

		case VMOPType::OPRND_FLOAT32:
			text << op.val.f32;
			break;

		case VMOPType::OPRND_FLOAT64:
			text << op.val.f64;
			break;

		case VMOPType::OPRND_VEC:
			text << '(';

			for (unsigned i = 0; i < 3; ++i) {
				if (i != 0)
					text << ", ";

				text << op.val.v[i];
			}

			text << ')';
			break;

		default:
			text << "print format not supported. : " << static_cast<unsigned>(op.op_type) << '\n';
			break;
		}
	}

	out << text.str();
}

VMOperand SyscallContext::vector(std::span<const VMOperand> arguments) const {
	if (arguments.size() > 3)
		throw OVMPanic("vector accepts at most three components.");

	VMOperand result;
	result.op_type = VMOPType::OPRND_VEC;
	result.val = VMNumericValue{};

	for (std::size_t i = 0; i < arguments.size(); ++i)
		result.val.v[i] = numeric_operand_to_float(arguments[i]);

	return result;
}

VMOperand SyscallContext::window(std::span<const VMOperand> arguments,
	const VMStringPool &vm_string_pool) {
	if (arguments.size() != 3 || arguments[0].op_type != VMOPType::OPRND_String ||
		arguments[1].op_type != VMOPType::OPRND_INT32 ||
		arguments[2].op_type != VMOPType::OPRND_INT32)
		throw OVMPanic("window expects (string, int, int).");

	VMOperand result;
	result.op_type = VMOPType::OPRND_ADDRESS;

	const std::string &title =
		vm_string_pool.str_pool.at(static_cast<unsigned>(arguments[0].val.i32));
	const int width = arguments[1].val.i32;
	const int height = arguments[2].val.i32;

	vm.engine.init_window(title, width, height);
	result.val.i32 = 1;

	return result;
}

std::optional<VMOperand> SyscallContext::handle_syscall(unsigned id,
	std::span<const VMOperand> arguments,
	const VMStringPool &vm_string_pool,
	std::ostream &out) {
	switch (id) {
	case 0:
		print(arguments, vm_string_pool, out);
		return std::nullopt;

	case 1:
		return vector(arguments);

	case 3:
		return window(arguments, vm_string_pool);

	default:
		throw OVMPanic("Unknown syscall ID: " + std::to_string(id));
	}
}
