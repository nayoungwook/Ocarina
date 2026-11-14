#include "vm.h"
#include "vm_dispatch.h"
#include "vm_opcode.h"

#include <algorithm>

static constexpr unsigned VM_STACK_SIZE = 1024 * 1024;

void OVM::clear_execution() {
	stack_index = 0;
	stack_pointer = 0;
	stack_frame = 0;
	executing = false;
	hierarchy.reset();
	current_scene = 0;
	globals = {};
	syscalls.reset();
	output.reset();
}

std::vector<VMOperand> OVM::update_function_arguments(unsigned argument_count) {
	if (argument_count > stack_index)
		throw OVMPanic("Not enough function arguments on the operand stack.");

	std::vector<VMOperand> arguments(argument_count);
	const std::size_t first = stack_index - argument_count;
	std::copy_n(vm_stack.get() + first, argument_count, arguments.begin());
	stack_index = first;
	return arguments;
}

// hierarchy_index identifies the receiver object in the hierarchy.
// For a.foo(), hierarchy_index is a's instance index.
// receiver is true when the receiver must be popped from the operand stack.
void OVM::call_function(const VMFunctionData &function_data,
	unsigned argument_count,
	unsigned hierarchy_index,
	bool receiver) {
	if (argument_count != function_data.argument_count)
		throw OVMPanic("Wrong argument count for function: " + function_data.name);
	const std::size_t required = static_cast<std::size_t>(argument_count) + (receiver ? 1 : 0);
	if (required > stack_index)
		throw OVMPanic("Not enough function arguments on the operand stack.");
	if (function_data.stack_size > stack.size - stack_pointer)
		throw OVMPanic("VM call stack overflow.");
	const std::size_t first = stack_index - argument_count;

	if (receiver) {
		const VMOperand &object = vm_stack[first - 1];
		find_object(object);
		if (static_cast<unsigned>(object.val.i32) != hierarchy_index)
			throw OVMPanic("Invalid method receiver.");
	}

	unsigned offset = stack_pointer;
	for (unsigned i = 0; i < argument_count; ++i) {
		const unsigned size = function_data.argument_sizes[i];
		stack.save(offset, size, vm_stack[first + i]);
		offset += size;
	}
	stack_index = first - (receiver ? 1 : 0);
	const auto result = vm_exec_function(function_data, hierarchy_index);
	if (result)
		vm_stack_push(*result);
}

std::optional<VMOperand> OVM::vm_exec_function(const VMFunctionData &function_data,
	unsigned hierarchy_index) {
	if (!function_data.code_loaded)
		throw OVMPanic("Function code is not loaded: " + function_data.name);
	if (function_data.stack_size > stack.size - stack_pointer)
		throw OVMPanic("VM call stack overflow.");

	const unsigned caller_frame = stack_frame;
	const unsigned base = stack_pointer;
	const std::size_t operand_base = stack_index;
	stack_frame = base;
	const unsigned locals = base + function_data.argument_bytes;
	const unsigned end = base + function_data.stack_size;
	std::fill(stack.values.begin() + locals, stack.values.begin() + end, byte{});
	std::fill(stack.types.begin() + locals, stack.types.begin() + end, VMOPType::OPRND_NULL);

	std::optional<VMOperand> result;
	std::size_t instruction_index = 0;
	while (instruction_index < function_data.instructions.size()) {
		const VMInstruction &instruction = function_data.instructions[instruction_index++];
		if (instruction.opcode == OP_RET)
			break;
		if (instruction.opcode == OP_RET_VAL)
			result = vm_stack_pop();
		else
			exec_instruction(instruction, instruction_index, hierarchy_index);
	}

	const bool should_return_value = function_data.return_type != "void" && !function_data.is_constructor;
	if (should_return_value) {
		if (!result) {
			if (stack_index <= operand_base)
				throw OVMPanic("Function did not return a value.");
			result = vm_stack_pop();
		}
	}
	else
		result.reset();

	stack_index = operand_base;
	stack_pointer = base;
	stack_frame = caller_frame;
	return result;
}

std::optional<VMOperand> OVM::run(std::ostream &out, RunMode mode) {
	if (executing)
		throw OVMPanic("This OVM is already running.");
	clear_execution();

	syscalls.emplace(*this);
	output = std::ref(out);

	try {
		if (!vm_stack)
			vm_stack = std::make_unique<VMOperand[]>(MAX_STACK_SIZE);
		stack.resize(VM_STACK_SIZE);
		unsigned global_size = 0;
		for (const auto &[id, variable] : variable_data)
			global_size = std::max(global_size, variable.offset + variable.size);
		globals.resize(global_size);
		const auto entry = function_data.find(main_func_id);
		if (entry == function_data.end())
			throw OVMPanic("Bytecode has no main function.");
		if (entry->second.argument_count != 0)
			throw OVMPanic("main must not require arguments.");

		executing = true;
		if (initializer.code_loaded)
			vm_exec_function(initializer);

		const auto result = vm_exec_function(entry->second);
		executing = false;

		if (mode == RunMode::WaitForWindow) {
			if (engine.get_display() != nullptr)
				engine.start();
			engine.shutdown();
		}

		if (mode == RunMode::WaitForWindow || engine.get_display() == nullptr)
			clear_execution();
		return result;
	}
	catch (...) {
		engine.shutdown();
		clear_execution();
		throw;
	}
}
