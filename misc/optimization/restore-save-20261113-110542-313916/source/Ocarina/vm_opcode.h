#pragma once

#include "ir.h"

inline constexpr byte OP_CALL_SUPER = 0x62;
inline constexpr byte OP_CALL_SUPER_METHOD = 0x63; // parent object_id, function_id, argument_count
inline constexpr byte OP_NEG = 0x77;

// Loader-only specializations; serialized bytecode keeps the original opcodes.
inline constexpr byte OP_FAST_LOCAL_LOAD_I4 = 0xc0;
inline constexpr byte OP_FAST_LOCAL_LOAD_F4 = 0xc1;
inline constexpr byte OP_FAST_LOCAL_LOAD_F8 = 0xc2;
inline constexpr byte OP_FAST_LOCAL_SAVE_4 = 0xc3;
inline constexpr byte OP_FAST_LOCAL_SAVE_8 = 0xc4;
inline constexpr byte OP_FAST_OBJECT_LOAD_I4 = 0xc5;
inline constexpr byte OP_FAST_OBJECT_LOAD_F4 = 0xc6;
inline constexpr byte OP_FAST_OBJECT_LOAD_F8 = 0xc7;
inline constexpr byte OP_FAST_OBJECT_SAVE_4 = 0xc8;
inline constexpr byte OP_FAST_OBJECT_SAVE_8 = 0xc9;
