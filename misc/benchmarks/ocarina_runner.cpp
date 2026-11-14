#include "vm.h"
#include "measure.h"

#include <sstream>

int main(int argc, char **argv) {
    try {
        if (argc != 4) throw std::runtime_error("Usage: ocarina_bench source.oc workload size");
        ParserContext parser;
        parser.register_file(argv[1]);
        parser.compile_files();
        IRContext ir;
        ir.gen_ir(std::move(parser));
        OVM vm;
        vm.register_ir(argv[1], ir);
        vm.load_files();
        std::ostringstream output;
        measure("Ocarina", argv[2], std::stoi(argv[3]), [&] {
            const auto result = vm.run(output);
            if (!result || result->op_type != VMOPType::OPRND_INT32)
                throw std::runtime_error("Benchmark must return an int checksum");
            return result->val.i32;
        });
        if (!output.str().empty()) throw std::runtime_error("Unexpected script output");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
