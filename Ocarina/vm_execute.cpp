#include "vm.h"
#include "vm_numeric.h"
#include "vm_opcode.h"

#include <algorithm>
#include <stdexcept>

static constexpr unsigned VM_STACK_SIZE = 1024 * 1024;
static constexpr unsigned VM_OPERAND_STACK_SIZE = 1024 * 256;
static constexpr unsigned VM_CALL_DEPTH = 512;

VMFrame::VMFrame(const VMFunctionData &function_data) : function_data(function_data) {
}

void CVM::clear_execution() {
	frames.clear();
	vm_stack.clear();
	hierarchy.reset();
	globals = {};
	stack_size = 0;
	syscalls.reset();
	output.reset();
}

void CVM::vm_stack_push(VMOperand operand) {
	if (vm_stack.size() >= VM_OPERAND_STACK_SIZE)
		throw std::runtime_error("VM operand stack overflow.");
	vm_stack.push_back(operand);
}

VMOperand CVM::vm_stack_pop() {
	if (vm_stack.empty() || (!frames.empty() && vm_stack.size() <= frames.back().operand_base))
		throw std::runtime_error("VM operand stack underflow.");

	VMOperand operand = vm_stack.back();
	vm_stack.pop_back();
	return operand;
}

std::vector<VMOperand> CVM::update_function_arguments(unsigned argument_count) {
	const std::size_t base = frames.empty() ? 0 : frames.back().operand_base;
	if (argument_count > vm_stack.size() - base)
		throw std::runtime_error("Not enough function arguments on the operand stack.");

	std::vector<VMOperand> arguments(argument_count);
	for (unsigned i = 0; i < argument_count; ++i) {
		const unsigned index = argument_count - i - 1;
		arguments[index] = vm_stack_pop();
	}
	return arguments;
}

void CVM::call_function(const VMFunctionData &function_data,
						unsigned argument_count,
						unsigned hierarchy_index,
						bool receiver) {
	std::vector<VMOperand> arguments = update_function_arguments(argument_count);
	if (receiver) {
		const VMOperand object = vm_stack_pop();
		find_object(object);
		if (static_cast<unsigned>(object.val.i32) != hierarchy_index)
			throw std::runtime_error("Invalid method receiver.");
	}
	push_frame(function_data, std::move(arguments), hierarchy_index);
}

void CVM::push_frame(const VMFunctionData &function_data,
					 std::vector<VMOperand> arguments,
					 unsigned hierarchy_index) {
	if (!function_data.code_loaded)
		throw std::runtime_error("Function code is not loaded: " + function_data.name);
	if (arguments.size() != function_data.argument_count)
		throw std::runtime_error("Wrong argument count for function: " + function_data.name);
	if (frames.size() >= VM_CALL_DEPTH || function_data.stack_size > VM_STACK_SIZE - stack_size)
		throw std::runtime_error("VM call stack overflow.");

	VMFrame frame(function_data);
	frame.data.resize(function_data.stack_size);
	frame.hierarchy_index = hierarchy_index;
	frame.operand_base = vm_stack.size();

	unsigned offset = 0;
	for (std::size_t i = 0; i < arguments.size(); ++i) {
		const unsigned size = function_data.argument_sizes[i];
		frame.data.save(offset, size, arguments[i]);
		offset += size;
	}

	frames.push_back(std::move(frame));
	stack_size += function_data.stack_size;
}

std::optional<VMOperand> CVM::finish_frame() {
	VMFrame &frame = frames.back();
	const VMFunctionData &function_data = frame.function_data.get();
	std::optional<VMOperand> result;

	if (function_data.return_type != "void" && !function_data.is_constructor) {
		if (!frame.return_value) {
			if (vm_stack.size() <= frame.operand_base)
				throw std::runtime_error("Function did not return a value.");
			frame.return_value = vm_stack_pop();
		}
		result = *frame.return_value;
	}
	if (frame.result_override)
		result = frame.result_override;

	vm_stack.resize(frame.operand_base);
	stack_size -= function_data.stack_size;
	frames.pop_back();
	return result;
}

std::optional<VMOperand> CVM::vm_exec_function(const VMFunctionData &function_data) {
	push_frame(function_data);

	try {
		while (!frames.empty()) {
			VMFrame &frame = frames.back();
			const auto &instructions = frame.function_data.get().instructions;
			bool finished = frame.instruction_index >= instructions.size();

			if (!finished) {
				const VMInstruction &instruction = instructions.at(frame.instruction_index++);
				exec_instruction(instruction, frame.instruction_index, frame, frame.hierarchy_index);
				finished = instruction.opcode == OP_RET;
			}
			if (!finished)
				continue;

			const auto result = finish_frame();
			if (frames.empty())
				return result;
			if (result)
				vm_stack_push(*result);
		}
	} catch (const std::exception &error) {
		std::string message = error.what();
		for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame) {
			const auto &function = frame->function_data.get();
			const std::size_t index =
				frame->instruction_index == 0 ? 0 : frame->instruction_index - 1;
			message += "\n  at " +
					   (function.name.empty() ? std::string("<initializer>") : function.name) +
					   " instruction " + std::to_string(index);
		}
		throw std::runtime_error(message);
	}
	return std::nullopt;
}

std::optional<VMOperand> CVM::run(std::ostream &out, RunMode mode) {
	if (syscalls)
		throw std::runtime_error("This CVM is already running.");
	clear_execution();

	SyscallContext context(*this);
	syscalls = std::ref(context);
	output = std::ref(out);

	try {
		unsigned global_size = 0;
		for (const auto &[id, variable] : variable_data)
			global_size = std::max(global_size, variable.offset + variable.size);
		globals.resize(global_size);
		const auto entry = function_data.find(main_func_id);
		if (entry == function_data.end())
			throw std::runtime_error("Bytecode has no main function.");
		if (entry->second.argument_count != 0)
			throw std::runtime_error("main must not require arguments.");

		if (initializer.code_loaded)
			vm_exec_function(initializer);
		const auto result = vm_exec_function(entry->second);
		if (mode == RunMode::WaitForWindow) {
			if (engine.get_display() != nullptr)
				engine.start();
			engine.shutdown();
		}
		clear_execution();
		return result;
	} catch (...) {
		engine.shutdown();
		clear_execution();
		throw;
	}
}
