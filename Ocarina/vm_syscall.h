#pragma once

#include "code_data.h"

#include <deque>
#include <iosfwd>
#include <optional>
#include <span>

struct SDL_Window;

struct VMNumericValue {
	std::int32_t i32 = 0;
	std::uint64_t u64 = 0;
	float f32 = 0;
	double f64 = 0;
	std::array<float, 4> v{};
};

struct VMOperand {
	VMOPType op_type = VMOPType::OPRND_NULL;
	VMNumericValue val;
};

class SyscallContext {
public:
	SyscallContext() = default;
	~SyscallContext();
	SyscallContext(const SyscallContext &) = delete;
	SyscallContext &operator=(const SyscallContext &) = delete;

	std::optional<VMOperand> handle_syscall(unsigned id,
		std::span<const VMOperand> arguments,
		const VMStringPool &vm_string_pool,
		std::ostream &out);
	void run_engine();

private:
	class Window {
	public:
		Window(const std::string &title, int width, int height);
		~Window();
		Window(const Window &) = delete;
		Window &operator=(const Window &) = delete;

	private:
		SDL_Window *window = nullptr;
	};

	std::deque<Window> windows;
	bool video_initialized = false;

	void print(std::span<const VMOperand> arguments,
		const VMStringPool &vm_string_pool,
		std::ostream &out) const;
	VMOperand vector(std::span<const VMOperand> arguments) const;
	VMOperand window(std::span<const VMOperand> arguments, const VMStringPool &vm_string_pool);
};
