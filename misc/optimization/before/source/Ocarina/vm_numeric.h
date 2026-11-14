#pragma once

#include "vm_syscall.h"

unsigned get_operand_size(VMOPType op_type);
int get_operand_level(VMOPType op_type);
VMOperand convert_argument(VMOperand operand, VMOPType target);
VMOperand vm_numeric_operation(std::uint8_t opcode, const VMOperand &lhs, const VMOperand &rhs);
VMOperand vm_negate(VMOperand operand);
bool vm_condition(const VMOperand &operand);
