#include "vm.h"
#include <iostream>
#include <sstream>

std::vector<std::string> shell_tokenize(const std::string &command) {
	std::istringstream input(command);
	std::vector<std::string> result;
	std::string tok;

	while (input >> tok)
		result.push_back(tok);

	return result;
}

void run_shell(CVM &vm) {
	std::string command;
	while (std::cout << ">>" && std::getline(std::cin, command)) {
		const std::vector<std::string> command_tok = shell_tokenize(command);
		if (command_tok.empty())
			continue;
		if (command_tok[0] == "quit")
			break;

		try {
			if (command_tok[0] == "register" && command_tok.size() > 1) {
				vm.clear();
				ParserContext pc;
				IRContext irc;

				for (std::size_t i = 1; i < command_tok.size(); ++i)
					pc.register_file(command_tok[i]);

				pc.compile_files();
				irc.gen_ir(std::move(pc));
				vm.register_ir(command_tok[1], irc);
			} else if (command_tok[0] == "run") {
				vm.load_files();
				vm.run(std::cout);
				std::cout << '\n';
			} else if (command_tok[0] == "load") {
				for (std::size_t i = 1; i < command_tok.size(); ++i)
					vm.register_bytecode(command_tok[i]);
			}
		} catch (const std::exception &error) {
			std::cerr << error.what() << '\n';
		}
	}
}

int main() {
	CVM vm;
	run_shell(vm);
	return 0;
}
