#pragma once

#include <exception>
#include <string>
#include <utility>

class OVMPanic : public std::exception {
public:
	explicit OVMPanic(std::string message) : message(std::move(message)) {}
	const char *what() const noexcept override { return message.c_str(); }

private:
	std::string message;
};
