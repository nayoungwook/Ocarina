#pragma once

#include "code_data.h"
#include "ovm_panic.h"
#include "vm_syscall.h"
#include "vm_memory.h"

#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <unordered_map>

#include "ir.h"
#include "engine/engine.h"

inline constexpr std::size_t MAX_STACK_SIZE = 1024 * 256;

class OVM {
public:
	enum class RunMode { WaitForWindow, ReturnAfterMain };

	void clear();
	void register_ir(std::string fname, const IRContext &irc);
	void register_bytes(std::string fname, std::vector<byte> bytes);
	void register_bytecode(const std::string &path);
	void load_files();
	std::optional<VMOperand> run(std::ostream &out, RunMode mode = RunMode::WaitForWindow);
	void update_hierarchy();
	void render_hierarchy();
	void set_scene(const VMOperand &scene);
	void render_texture(const VMOperand &texture, const Vector &position,
		float width, float height, float rotation = 0.0f);

	std::unordered_map<std::string, std::vector<byte>> byte_files;

	std::map<unsigned, VMObjectData> object_data;
	std::map<unsigned, VMFunctionData> function_data;
	std::map<unsigned, VMVariableData> variable_data;
	VMFunctionData initializer;
	VMStringPool vm_string_pool;

	unsigned main_func_id = std::numeric_limits<unsigned>::max();

	Engine engine{ this };

	std::unordered_map<std::string, Texture *> texture_pool;

private:
	std::unique_ptr<VMOperand[]> vm_stack;
	std::size_t stack_index = 0;

	VMStorage stack;
	unsigned stack_pointer = 0;
	unsigned stack_frame = 0;
	bool executing = false;
	VMHierarchy hierarchy;
	unsigned current_scene = 0;
	VMStorage globals;
	std::optional<SyscallContext> syscalls;
	std::optional<std::reference_wrapper<std::ostream>> output;


	void clear_execution();

	inline void vm_stack_push(VMOperand operand) {
		if (stack_index >= MAX_STACK_SIZE)
			throw OVMPanic("VM operand stack overflow.");
		vm_stack[stack_index++] = operand;
	}

	inline VMOperand vm_stack_pop() {
		if (stack_index == 0)
			throw OVMPanic("VM operand stack underflow.");
		return vm_stack[--stack_index];
	}

	std::vector<VMOperand> update_function_arguments(unsigned argument_count);
	std::optional<VMOperand> vm_exec_function(const VMFunctionData &function_data,
		unsigned hierarchy_index = 0,
		const std::vector<VMOperand> &arguments = {});
	void exec_hierarchy(std::string_view method);
	void exec_instance_method(unsigned hierarchy_index, std::string_view method);
	void exec_render(const VMInstruction &instruction, unsigned hierarchy_index);
	void exec_instruction(const VMInstruction &instruction,
		std::size_t &instruction_index,
		unsigned hierarchy_index);
	void call_function(const VMFunctionData &function_data,
		unsigned argument_count,
		unsigned hierarchy_index = 0,
		bool receiver = false);
	VMHierarchyObject &find_object(const VMOperand &operand);
	const VMFunctionData &vm_find_member_function_data(unsigned object_id, unsigned id) const;
	void vm_exec_object_initializers(unsigned object_id, unsigned hierarchy_index);
	void exec_object_instruction(const VMInstruction &instruction, unsigned hierarchy_index);
	void exec_new_object(const VMInstruction &instruction);
	void exec_object_member_instruction(const VMInstruction &instruction, unsigned hierarchy_index);
};
