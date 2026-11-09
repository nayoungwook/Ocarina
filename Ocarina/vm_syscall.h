#pragma once

#include "code_data.h"

#include <iosfwd>
#include <optional>
#include <span>

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
		std::span<const VMOperand> arguments,
		const VMStringPool &vm_string_pool,
		std::ostream &out);

private:
	void print(std::span<const VMOperand> arguments,
		const VMStringPool &vm_string_pool,
		std::ostream &out) const;
	VMOperand vector(std::span<const VMOperand> arguments) const;
	VMOperand window(std::span<const VMOperand> arguments, const VMStringPool &vm_string_pool);
	OVM &vm;
};
