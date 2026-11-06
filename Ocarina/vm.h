#pragma once

#include "code_data.h"
#include "vm_syscall.h"
#include "vm_memory.h"

#include <iosfwd>
#include <limits>
#include <unordered_map>

#include "ir.h"
#include "engine/engine.h"

class CVM {
public:
	void clear();
	void register_ir(std::string fname, const IRContext &irc);
	void register_bytes(std::string fname, std::vector<byte> bytes);
	void register_bytecode(const std::string &path);
	void load_files();
	std::optional<VMOperand> run(std::ostream &out);

	std::unordered_map<std::string, std::vector<byte>> byte_files;

	std::map<unsigned, VMObjectData> object_data;
	std::map<unsigned, VMFunctionData> function_data;
	std::map<unsigned, VMVariableData> variable_data;
	VMFunctionData initializer;
	VMStringPool vm_string_pool;

	unsigned main_func_id = std::numeric_limits<unsigned>::max();

	Engine engine;

private:
	std::vector<VMOperand> vm_stack;
	std::deque<VMFrame> frames;
	std::deque<VMHeapObject> heap_mapper;
	VMStorage globals;
	std::size_t stack_size = 0;
	std::optional<std::reference_wrapper<SyscallContext>> syscalls;
	std::optional<std::reference_wrapper<std::ostream>> output;

	void clear_execution();
	void vm_stack_push(VMOperand operand);
	VMOperand vm_stack_pop();
	std::vector<VMOperand> update_function_arguments(unsigned argument_count);
	void push_frame(const VMFunctionData &function_data,
		std::vector<VMOperand> arguments = {},
		unsigned heap_index = 0);
	std::optional<VMOperand> finish_frame();
	std::optional<VMOperand> vm_exec_function(const VMFunctionData &function_data);
	void exec_instruction(const VMInstruction &instruction,
		std::size_t &instruction_index,
		VMFrame &frame,
		unsigned heap_index);
	void call_function(const VMFunctionData &function_data,
		unsigned argument_count,
		unsigned heap_index = 0,
		bool receiver = false);
	VMHeapObject &find_heap_object(const VMOperand &operand);
	const VMFunctionData &vm_find_member_function_data(unsigned object_id, unsigned id) const;
	void vm_exec_object_initializers(unsigned object_id, unsigned heap_index);
	void exec_object_instruction(const VMInstruction &instruction, unsigned heap_index);
	void exec_array_instruction(const VMInstruction &instruction);
};
