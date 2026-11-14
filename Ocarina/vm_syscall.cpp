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

void SyscallContext::print(const std::vector<VMOperand> &arguments,
	std::ostream &out) const {
	std::ostringstream text;

	for (const VMOperand &op : arguments) {
		switch (op.op_type) {
		case VMOPType::OPRND_String:
			text << vm.vm_string_pool.str_pool.at(static_cast<unsigned>(op.val.i32));
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

		case VMOPType::OPRND_TEXTURE:
			text << "Texture : " << vm.vm_string_pool.str_pool.at(op.val.i32);
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

VMOperand SyscallContext::vector(const std::vector<VMOperand> &arguments) const {
	VMOperand result;
	result.op_type = VMOPType::OPRND_VEC;
	result.val = VMNumericValue{};

	for (std::size_t i = 0; i < arguments.size(); ++i)
		result.val.v[i] = numeric_operand_to_float(arguments[i]);

	return result;
}

VMOperand SyscallContext::window(const std::vector<VMOperand> &arguments) {
	VMOperand result;
	result.op_type = VMOPType::OPRND_ADDRESS;

	const std::string &title =
		vm.vm_string_pool.str_pool.at(static_cast<unsigned>(arguments[0].val.i32));
	const int width = arguments[1].val.i32;
	const int height = arguments[2].val.i32;

	vm.engine.init_window(title, width, height);
	result.val.i32 = 1;

	return result;
}

VMOperand SyscallContext::texture(const std::vector<VMOperand> &arguments) {
	VMOperand result;

	result.op_type = VMOPType::OPRND_TEXTURE;

	const std::string &path = vm.vm_string_pool.str_pool.at(static_cast<unsigned>(arguments[0].val.i32));

	if (vm.texture_pool.find(path) == vm.texture_pool.end()) {
		vm.texture_pool.insert(std::make_pair(path, new Texture(path)));
	}

	result.val.i32 = arguments[0].val.i32;

	return result;
}

void SyscallContext::render_texture(const std::vector<VMOperand> &arguments) {
	const VMOperand &position = arguments[1];
	vm.render_texture(arguments[0], Vector(position.val.v[0], position.val.v[1], position.val.v[2]),
		arguments[2].val.f32, arguments[3].val.f32);
}

void SyscallContext::set_scene(const std::vector<VMOperand> &arguments) {
	vm.set_scene(arguments.at(0));
}

std::optional<VMOperand> SyscallContext::handle_syscall(unsigned id,
	const std::vector<VMOperand> &arguments,
	std::ostream &out) {
	switch (id) {
	case 0:
		print(arguments, out);
		return std::nullopt;

	case 1:
		return vector(arguments);

	case 2:
		return texture(arguments);

	case 3:
		return window(arguments);

	case 4:
		render_texture(arguments);
		return std::nullopt;
	case 5:
		set_scene(arguments);
		return std::nullopt;

	default:
		throw OVMPanic("Unknown syscall ID: " + std::to_string(id));
	}
}
