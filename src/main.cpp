#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyzer.hpp"
#include "IRGenerator.hpp"
#include "Optimizer.hpp"
#include "PTXEmitter.hpp"
#include "StackVM.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

void print_usage(const char* prog) {
    std::cout << "ptxcc - Modern C++20 Compiler with NVIDIA PTX Target\n"
              << "Usage: " << prog << " [options] <input_file.nv>\n\n"
              << "Options:\n"
              << "  --emit-tokens   Dump lexical tokens\n"
              << "  --emit-ir       Dump unoptimized 3AC Intermediate Representation\n"
              << "  --emit-opt-ir   Dump optimized 3AC Intermediate Representation\n"
              << "  --emit-ptx      Emit NVIDIA PTX Virtual Assembly (default)\n"
              << "  --run           Execute on internal StackVM interpreter\n"
              << "  --args <v1> ... Pass numerical arguments to kernel\n"
              << "  --no-opt        Disable optimization passes\n"
              << "  -o <file>       Write output to file instead of stdout\n"
              << "  -h, --help      Display this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_path;
    std::string output_path;
    bool emit_tokens = false;
    bool emit_ir = false;
    bool emit_opt_ir = false;
    bool emit_ptx = true;
    bool run_vm = false;
    bool optimize = true;
    std::vector<int> vm_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--emit-tokens") {
            emit_tokens = true;
            emit_ptx = false;
        } else if (arg == "--emit-ir") {
            emit_ir = true;
            emit_ptx = false;
        } else if (arg == "--emit-opt-ir") {
            emit_opt_ir = true;
            emit_ptx = false;
        } else if (arg == "--emit-ptx") {
            emit_ptx = true;
        } else if (arg == "--no-opt") {
            optimize = false;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                std::cerr << "Error: -o requires a file argument\n";
                return 1;
            }
        } else if (arg == "--run") {
            run_vm = true;
            emit_ptx = false;
        } else if (arg == "--args") {
            while (i + 1 < argc) {
                std::string next_arg = argv[i + 1];
                if (next_arg.empty() || (next_arg[0] == '-' && next_arg.size() > 1 && !std::isdigit(static_cast<unsigned char>(next_arg[1])))) {
                    break;
                }
                try {
                    size_t pos = 0;
                    int val = std::stoi(next_arg, &pos);
                    if (pos == next_arg.size()) {
                        vm_args.push_back(val);
                        i++;
                    } else {
                        break;
                    }
                } catch (...) {
                    break;
                }
            }
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return 1;
        } else {
            input_path = arg;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    std::ifstream file(input_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file: " << input_path << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        // 1. Lexing
        ptxcc::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        if (emit_tokens) {
            for (const auto& tok : tokens) {
                std::cout << "[" << ptxcc::Token::token_type_name(tok.type)
                          << " '" << tok.lexeme << "' at "
                          << tok.location.line << ":" << tok.location.column << "]\n";
            }
            return 0;
        }

        // 2. Parsing
        ptxcc::Parser parser(std::move(tokens));
        auto ast = parser.parse();

        // 3. Semantic Analysis
        ptxcc::SemanticAnalyzer sema;
        sema.analyze(*ast);
        if (sema.has_errors()) {
            std::cerr << "Compilation failed with semantic errors:\n";
            for (const auto& err : sema.errors()) {
                std::cerr << "  " << err << "\n";
            }
            return 1;
        }

        // 4. IR Generation
        ptxcc::IRGenerator ir_gen;
        auto ir_program = ir_gen.generate(*ast);

        if (emit_ir) {
            std::cout << ir_program.to_string();
            return 0;
        }

        // 5. Optimization
        ptxcc::IRProgram final_ir = ir_program;
        if (optimize) {
            ptxcc::Optimizer opt;
            final_ir = opt.optimize(ir_program);
        }

        if (emit_opt_ir) {
            std::cout << final_ir.to_string();
            return 0;
        }

        // 6. StackVM Execution
        if (run_vm) {
            if (final_ir.kernels.empty()) {
                std::cerr << "Error: No kernels defined in source file.\n";
                return 1;
            }
            ptxcc::StackVM vm;
            int result = vm.execute(final_ir.kernels[0], vm_args);
            std::cout << "Kernel '" << final_ir.kernels[0].name << "' returned: " << result << "\n";
            return 0;
        }

        // 7. PTX Emission
        if (emit_ptx) {
            ptxcc::PTXEmitter ptx_emitter;
            std::string ptx_code = ptx_emitter.emit(final_ir);

            if (!output_path.empty()) {
                std::ofstream out(output_path);
                if (!out.is_open()) {
                    std::cerr << "Error: Could not open output file: " << output_path << "\n";
                    return 1;
                }
                out << ptx_code;
                std::cout << "PTX written to " << output_path << "\n";
            } else {
                std::cout << ptx_code;
            }
        }

    } catch (const std::exception& ex) {
        std::cerr << "Fatal Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
