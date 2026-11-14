#include "vm_numeric.h"

#include <array>
#include <iostream>
#include <random>

static bool equal(const VMOperand &a, const VMOperand &b) {
    if (a.op_type != b.op_type) return false;
    if (a.op_type == VMOPType::OPRND_FLOAT32)
        return (std::isnan(a.val.f32) && std::isnan(b.val.f32)) ||
            std::bit_cast<std::uint32_t>(a.val.f32) == std::bit_cast<std::uint32_t>(b.val.f32);
    if (a.op_type == VMOPType::OPRND_FLOAT64)
        return (std::isnan(a.val.f64) && std::isnan(b.val.f64)) ||
            std::bit_cast<std::uint64_t>(a.val.f64) == std::bit_cast<std::uint64_t>(b.val.f64);
    return a.val.i32 == b.val.i32;
}

static void check(std::uint8_t opcode, const VMOperand &a, const VMOperand &b) {
    VMOperand reference, actual = a;
    bool reference_error = false, actual_error = false;
    try { reference = vm_numeric_operation(opcode, a, b); }
    catch (const OVMPanic &) { reference_error = true; }
    try { vm_apply_numeric_operation(opcode, actual, b); }
    catch (const OVMPanic &) { actual_error = true; }
    if (reference_error != actual_error || (!reference_error && !equal(reference, actual)))
        throw std::runtime_error("Scalar fast path differs from generic numeric operation");
}

int main() {
    try {
        constexpr std::array<std::uint8_t, 14> operations = { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
            OP_EQUAL, OP_NOTEQUAL, OP_GREATER, OP_LESS, OP_EQUALGREATER, OP_EQUALLESS, OP_OR, OP_AND, OP_ADD };
        constexpr std::array<std::int32_t, 8> integers = { 0, 1, -1, 2147483647, -2147483647 - 1, 17, -19, 1000003 };
        const std::array<double, 10> floats = { 0.0, -0.0, 1.0, -1.0, 1.25, -2.75,
            1e30, 1e-30, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() };
        std::mt19937 random(20261113);
        std::size_t checks = 0;
        for (unsigned i = 0; i < 10000; ++i) {
            VMOperand a, b;
            a.op_type = b.op_type = VMOPType::OPRND_INT32;
            a.val.i32 = i < 64 ? integers[i / 8] : std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random()));
            b.val.i32 = i < 64 ? integers[i % 8] : std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random()));
            for (auto opcode : operations) { check(opcode, a, b); ++checks; }
        }
        for (auto type : { VMOPType::OPRND_FLOAT32, VMOPType::OPRND_FLOAT64 }) {
            for (double left : floats) for (double right : floats) {
                VMOperand a, b;
                a.op_type = b.op_type = type;
                if (type == VMOPType::OPRND_FLOAT32) { a.val.f32 = static_cast<float>(left); b.val.f32 = static_cast<float>(right); }
                else { a.val.f64 = left; b.val.f64 = right; }
                for (auto opcode : operations) { check(opcode, a, b); ++checks; }
            }
        }
        std::cout << checks << " numeric differential checks passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
