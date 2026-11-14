#pragma once

#include <string>
#include <vector>

class OVM;

void execute_shell_command(OVM &vm, const std::vector<std::string> &command);
