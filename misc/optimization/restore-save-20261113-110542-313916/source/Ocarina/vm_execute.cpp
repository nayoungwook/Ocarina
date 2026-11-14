#include "vm.h"
#include "vm_numeric.h"
#include "vm_opcode.h"

#include <algorithm>
#include "ovm_panic.h"

static constexpr unsigned VM_STACK_SIZE = 1024 * 1024;
static constexpr unsigned VM_CALL_DEPTH = 512;
static constexpr std::size_t VM_FRAME_CACHE_SIZE = VM_STACK_SIZE * 4;

VMFrame::VMFrame(const VMFunctionData &function_data) : function_data(function_data) {
}

void OVM::trim_frame_cache(std::size_t first_unused) {
	for (std::size_t i = frames.size(); i > first_unused && frame_cache_bytes > VM_FRAME_CACHE_SIZE; --i) {
		VMStorage &data = frames[i - 1].data;
		frame_cache_bytes -= data.values.capacity() + data.types.capacity();
		data = {};
	}
}

void OVM::clear_execution() {
	frame_count = 0;
	if (frame_cache_bytes > VM_FRAME_CACHE_SIZE) trim_frame_cache(0);
	stack_index = 0;
	stack_base = 0;
	hierarchy.reset();
	current_scene = 0;
	globals = {};
	stack_size = 0;
	syscalls.reset();
	output.reset();
}

std::vector<VMOperand> OVM::update_function_arguments(unsigned argument_count) {
	if (argument_count > stack_index - stack_base)
		throw OVMPanic("Not enough function arguments on the operand stack.");

	std::vector<VMOperand> arguments(argument_count);
	const std::size_t first = stack_index - argument_count;
	for (unsigned i = 0; i < argument_count; ++i) {
		arguments[i] = vm_stack[first + i];
	}
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
	if (required > stack_index - stack_base)
		throw OVMPanic("Not enough function arguments on the operand stack.");
	const std::size_t first = stack_index - argument_count;
	if (receiver) {
		const VMOperand &object = vm_stack[first - 1];
		find_object(object);
		if (static_cast<unsigned>(object.val.i32) != hierarchy_index)
			throw OVMPanic("Invalid method receiver.");
	}

	const std::size_t base = first - (receiver ? 1 : 0);
	VMFrame &frame = prepare_frame(function_data, hierarchy_index, base);
	unsigned offset = 0;
	for (unsigned i = 0; i < argument_count; ++i) {
		const unsigned size = function_data.argument_sizes[i];
		frame.data.save(offset, size, vm_stack[first + i]);
		offset += size;
	}
	stack_index = base;
}

VMFrame &OVM::prepare_frame(const VMFunctionData &function_data,
	unsigned hierarchy_index, std::size_t operand_base) {
	if (!function_data.code_loaded)
		throw OVMPanic("Function code is not loaded: " + function_data.name);
	if (frame_count >= VM_CALL_DEPTH || function_data.stack_size > VM_STACK_SIZE - stack_size)
		throw OVMPanic("VM call stack overflow.");
	if (frame_count == frames.size())
		frames.emplace_back(function_data);
	VMFrame &frame = frames[frame_count];
	frame.function_data = std::cref(function_data);
	if (frame.data.size != function_data.stack_size) {
		const std::size_t previous = frame.data.values.capacity() + frame.data.types.capacity();
		frame.data.resize(function_data.stack_size);
		frame_cache_bytes += frame.data.values.capacity() + frame.data.types.capacity() - previous;
		if (frame_cache_bytes > VM_FRAME_CACHE_SIZE) trim_frame_cache(frame_count + 1);
	}
	std::fill(frame.data.values.begin(), frame.data.values.end(), byte{});
	std::fill(frame.data.types.begin(), frame.data.types.end(), VMOPType::OPRND_NULL);
	frame.stack_pointer = 0;
	frame.hierarchy_index = hierarchy_index;
	frame.instruction_index = 0;
	frame.operand_base = operand_base;
	frame.return_value.reset();
	frame.result_override.reset();
	++frame_count;
	stack_size += function_data.stack_size;
	stack_base = operand_base;
	return frame;
}

void OVM::push_frame(const VMFunctionData &function_data,
	std::vector<VMOperand> arguments, unsigned hierarchy_index) {
	if (arguments.size() != function_data.argument_count)
		throw OVMPanic("Wrong argument count for function: " + function_data.name);
	VMFrame &frame = prepare_frame(function_data, hierarchy_index, stack_index);

	// upload arguments into stack buffer.
	unsigned offset = 0;
	for (std::size_t i = 0; i < arguments.size(); ++i) {
		const unsigned size = function_data.argument_sizes[i];
		frame.data.save(offset, size, arguments[i]);
		offset += size;
	}

}

std::optional<VMOperand> OVM::finish_frame() {
	VMFrame &frame = frames[frame_count - 1];
	const VMFunctionData &function_data = frame.function_data.get();
	std::optional<VMOperand> result;

	bool should_return_value = function_data.return_type != "void" && !function_data.is_constructor;
	if (should_return_value) {
		if (!frame.return_value) { // Should return value!!!
			if (stack_index <= frame.operand_base)
				throw OVMPanic("Function did not return a value.");
			frame.return_value = vm_stack_pop();
		}

		result = *frame.return_value;
	}

	if (frame.result_override)
		result = frame.result_override;

	stack_index = frame.operand_base;
	stack_size -= function_data.stack_size;
	--frame_count;
	stack_base = frame_count == 0 ? 0 : frames[frame_count - 1].operand_base;
	return result;
}

std::optional<VMOperand> OVM::vm_exec_function(const VMFunctionData &function_data,
	unsigned hierarchy_index) {
	push_frame(function_data, {}, hierarchy_index);
	try {
		while (frame_count != 0) {
			VMFrame &frame = frames[frame_count - 1];
			const auto &instructions = frame.function_data.get().instructions;
			bool finished = frame.instruction_index >= instructions.size();

			if (!finished) {
				const VMInstruction &instruction = instructions[frame.instruction_index++];
				exec_instruction(instruction, frame.instruction_index, frame, frame.hierarchy_index);
				finished = instruction.opcode == OP_RET;
			}
			if (!finished)
				continue;

			const auto result = finish_frame();
			if (frame_count == 0)
				return result;
			if (result)
				vm_stack_push(*result);
		}
	}
	catch (const std::exception &error) {
		std::string message = error.what();
		for (std::size_t depth = frame_count; depth > 0; --depth) {
			const VMFrame &frame = frames[depth - 1];
			const auto &function = frame.function_data.get();
			const std::size_t index =
				frame.instruction_index == 0 ? 0 : frame.instruction_index - 1;
			message += "\n  at " +
				(function.name.empty() ? std::string("<initializer>") : function.name) +
				" instruction " + std::to_string(index);
		}
		throw OVMPanic(message);
	}
	return std::nullopt;
}

std::optional<VMOperand> OVM::run(std::ostream &out, RunMode mode) {
	if (frame_count != 0)
		throw OVMPanic("This OVM is already running.");
	clear_execution();

	syscalls.emplace(*this);
	output = std::ref(out);

	try {
		if (!vm_stack)
			vm_stack = std::make_unique<VMOperand[]>(MAX_STACK_SIZE);
		if (frames.capacity() < VM_CALL_DEPTH)
			frames.reserve(VM_CALL_DEPTH);
		unsigned global_size = 0;
		for (const auto &[id, variable] : variable_data)
			global_size = std::max(global_size, variable.offset + variable.size);
		globals.resize(global_size);
		const auto entry = function_data.find(main_func_id);
		if (entry == function_data.end())
			throw OVMPanic("Bytecode has no main function.");
		if (entry->second.argument_count != 0)
			throw OVMPanic("main must not require arguments.");

		if (initializer.code_loaded)
			vm_exec_function(initializer);

		const auto result = vm_exec_function(entry->second);

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
