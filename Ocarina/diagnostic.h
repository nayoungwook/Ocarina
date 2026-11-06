#pragma once

#include "token.h"

#include <stdexcept>

class FrontendError : public std::runtime_error {
public:
	FrontendError(const std::string &source, SourceLocation location,
		const std::string &message)
		: std::runtime_error(source + ":" + std::to_string(location.line) + ":" +
			std::to_string(location.column) + ": " + message),
		location(location) {
	}

	SourceLocation location;
};
