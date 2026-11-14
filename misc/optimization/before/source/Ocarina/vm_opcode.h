#pragma once

#include "ir.h"

inline constexpr byte OP_CALL_SUPER = 0x62;
inline constexpr byte OP_CALL_SUPER_METHOD = 0x63; // parent object_id, function_id, argument_count
inline constexpr byte OP_NEG = 0x77;
