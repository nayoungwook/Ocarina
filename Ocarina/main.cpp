#include "vm.h"
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

std::vector<std::string> shell_tokenize(const std::string &command) {
	std::istringstream input(command);
	std::vector<std::string> result;
	std::string tok;

	while (input >> tok)
		result.push_back(tok);

	return result;
}

void run_shell(CVM &vm) {
	std::mutex input_mutex;
	std::condition_variable input_ready;
	std::queue<std::vector<std::string>> commands;
	bool input_done = false;
	std::string input_error;

	// Only this thread reads stdin. The SDL/main thread owns all VM and engine state.
	std::thread shell_input([&] {
		try {
			std::string command;
			while (std::getline(std::cin, command)) {
				auto tokens = shell_tokenize(command);
				const bool quit = !tokens.empty() && tokens[0] == "quit";
				{
					std::lock_guard lock(input_mutex);
					commands.push(std::move(tokens));
				}
				input_ready.notify_one();
				if (quit)
					break;
			}
		} catch (const std::exception &error) {
			std::lock_guard lock(input_mutex);
			input_error = error.what();
		}
		{
			std::lock_guard lock(input_mutex);
			input_done = true;
		}
		input_ready.notify_one();
	});

	std::cout << ">>" << std::flush;
	while (true) {
		std::vector<std::string> command_tok;
		bool have_command = false;
		{
			std::unique_lock lock(input_mutex);
			if (vm.engine.get_display() == nullptr)
				input_ready.wait(lock, [&] { return input_done || !commands.empty(); });
			if (!commands.empty()) {
				command_tok = std::move(commands.front());
				commands.pop();
				have_command = true;
			} else if (input_done)
				break;
		}
		if (have_command && !command_tok.empty() && command_tok[0] == "quit")
			break;

		try {
			if (!have_command || command_tok.empty()) {
				// Keep pumping frames while the input thread waits for another line.
			} else if (command_tok[0] == "register" && command_tok.size() > 1) {
				vm.clear();
				ParserContext pc;
				IRContext irc;

				for (std::size_t i = 1; i < command_tok.size(); ++i)
					pc.register_file(command_tok[i]);

				pc.compile_files();
				irc.gen_ir(std::move(pc));
				vm.register_ir(command_tok[1], irc);
			} else if (command_tok[0] == "run") {
				vm.engine.shutdown();
				vm.load_files();
				vm.run(std::cout, CVM::RunMode::ReturnAfterMain);
				std::cout << '\n';
			} else if (command_tok[0] == "load") {
				for (std::size_t i = 1; i < command_tok.size(); ++i)
					vm.register_bytecode(command_tok[i]);
			} else if (command_tok[0] == "stop") {
				vm.engine.shutdown();
			}
		} catch (const std::exception &error) {
			std::cerr << error.what() << '\n';
		}
		try {
			if (vm.engine.get_display() != nullptr && !vm.engine.run_frame())
				vm.engine.shutdown();
		} catch (const std::exception &error) {
			std::cerr << error.what() << '\n';
			vm.engine.shutdown();
		}
		if (have_command)
			std::cout << ">>" << std::flush;
	}
	shell_input.join();
	vm.engine.shutdown();
	if (!input_error.empty())
		std::cerr << input_error << '\n';
}

int main() {
	CVM vm;
	run_shell(vm);
	return 0;
}
