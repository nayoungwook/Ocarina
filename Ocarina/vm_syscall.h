#pragma once

#include "code_data.h"

#include <iosfwd>
#include <optional>
#include <vector>

class OVM;

union VMNumericValue {
	std::int32_t i32;
	std::uint64_t u64;
	float f32;
	double f64;
	float v[4]{};
};

struct VMOperand {
	VMOperand() { val.i32 = 0; }

	VMOPType op_type = VMOPType::OPRND_NULL;
	VMNumericValue val;
};

class SyscallContext {
public:
	SyscallContext(OVM &vm);
	~SyscallContext() = default;
	SyscallContext(const SyscallContext &) = delete;
	SyscallContext &operator=(const SyscallContext &) = delete;

	std::optional<VMOperand> handle_syscall(unsigned id,
		const std::vector<VMOperand> &arguments,
		std::ostream &out);

private:
	void print(const std::vector<VMOperand> &arguments,
		std::ostream &out) const;
	
	VMOperand vector(const std::vector<VMOperand> &arguments) const;
	VMOperand window(const std::vector<VMOperand> &arguments);
	VMOperand texture(const std::vector<VMOperand> &arguments);
	void render_texture(const std::vector<VMOperand> &arguments);
	void set_scene(const std::vector<VMOperand> &arguments);
	
	OVM &vm;
};
