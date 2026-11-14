#pragma once

#include "token.h"

#include "ovm_panic.h"

class FrontendError : public OVMPanic {
public:
	FrontendError(const std::string &source, SourceLocation location,
		const std::string &message)
		: OVMPanic(source + ":" + std::to_string(location.line) + ":" +
			std::to_string(location.column) + ": " + message),
		location(location) {
	}

	SourceLocation location;
};
