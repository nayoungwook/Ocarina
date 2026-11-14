#pragma once

#include "diagnostic.h"

[[noreturn]] inline void panic(const std::string &msg, const TokenizerContext &tc) {
        SourceLocation location = tc.token_cache.has_value() ? tc.token_cache.value().location : tc.get_source_location();
	throw FrontendError(tc.source_name, location, msg);
}
