#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyzer.hpp"
#include "IRGenerator.hpp"
#include "Optimizer.hpp"
#include "PTXEmitter.hpp"
#include "StackVM.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[*] Running " << #fn << "... "; \
        if (fn()) { \
            std::cout << "PASSED\n"; \
            passed_count++; \
        } else { \
            std::cout << "FAILED\n"; \
            failed_count++; \
        } \
    } while (0)

using namespace ptxcc;

// 1. Lexer Tests
bool test_lexer_tokens() {
    std::string src = "kernel saxpy(int a, int x) { // calculate\n int res = a * x + 42; return res; }";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();

    TEST_ASSERT(!tokens.empty(), "Tokens should not be empty");
    TEST_ASSERT(tokens[0].type == TokenType::KW_KERNEL, "First token should be KW_KERNEL");
    TEST_ASSERT(tokens[1].type == TokenType::IDENTIFIER && tokens[1].lexeme == "saxpy", "Second token should be saxpy");
    TEST_ASSERT(tokens[2].type == TokenType::LPAREN, "Token should be LPAREN");
    TEST_ASSERT(tokens[3].type == TokenType::KW_INT, "Token should be KW_INT");
    TEST_ASSERT(tokens[4].type == TokenType::IDENTIFIER && tokens[4].lexeme == "a", "Token should be 'a'");
    TEST_ASSERT(tokens.back().type == TokenType::END_OF_FILE, "Last token should be END_OF_FILE");
    return true;
}

bool test_lexer_malformed_input() {
    bool caught = false;
    try {
        Lexer lexer("int a = @;");
        lexer.tokenize();
    } catch (const std::exception& e) {
        caught = true;
    }
    TEST_ASSERT(caught, "Lexer should throw on unrecognized character '@'");
    return true;
}

// 2. Parser Precedence Tests
bool test_parser_precedence() {
    std::string src = "kernel test_math() { int x = 2 + 3 * 4; return x; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    TEST_ASSERT(ast != nullptr, "AST should not be null");
    TEST_ASSERT(ast->kernels.size() == 1, "Should have 1 kernel");
    TEST_ASSERT(ast->kernels[0]->name == "test_math", "Kernel name should match");

    // Semantic check
    SemanticAnalyzer sema;
    sema.analyze(*ast);
    TEST_ASSERT(!sema.has_errors(), "Semantic check should pass");

    // Lower to IR and execute in VM
    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);
    StackVM vm;
    int res = vm.execute(ir.kernels[0]);
    TEST_ASSERT(res == 14, "Operator precedence 2 + 3 * 4 should evaluate to 14, got " + std::to_string(res));
    return true;
}

bool test_parser_malformed_syntax() {
    bool caught = false;
    try {
        std::string src = "kernel bad( { int x = 1; }";
        Lexer lexer(src);
        Parser parser(lexer.tokenize());
        parser.parse();
    } catch (const std::exception& e) {
        caught = true;
    }
    TEST_ASSERT(caught, "Parser should throw on missing parameter type/closing parenthesis");
    return true;
}

// 3. Semantic Analysis Tests
bool test_semantic_undeclared_variable() {
    std::string src = "kernel bad() { x = 10; return x; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    SemanticAnalyzer sema;
    sema.analyze(*ast);
    TEST_ASSERT(sema.has_errors(), "Should report undeclared variable 'x'");
    return true;
}

bool test_semantic_duplicate_variable() {
    std::string src = "kernel bad() { int x = 1; int x = 2; return x; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    SemanticAnalyzer sema;
    sema.analyze(*ast);
    TEST_ASSERT(sema.has_errors(), "Should report duplicate variable 'x' in same scope");
    return true;
}

// 4. Optimization Tests
bool test_optimizer_constant_folding() {
    std::string src = "kernel fold() { int a = 10 + 20 * 3; return a; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    TEST_ASSERT(opt.stats().constant_folds > 0, "Constant folding should have fired");

    StackVM vm;
    int res = vm.execute(opt_ir.kernels[0]);
    TEST_ASSERT(res == 70, "Folded kernel should evaluate to 70");
    return true;
}

bool test_optimizer_algebraic_simplification() {
    std::string src = "kernel alg(int x) { int a = x + 0; int b = a * 1; int c = b * 0; return c; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    TEST_ASSERT(opt.stats().algebraic_simplifications > 0, "Algebraic simplification should have fired");

    StackVM vm;
    int res = vm.execute(opt_ir.kernels[0], {99});
    TEST_ASSERT(res == 0, "Result should be 0");
    return true;
}

bool test_optimizer_dead_code_elimination() {
    std::string src = "kernel dce() { int unused = 12345; int a = 5; return a; int dead = 999; }";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    TEST_ASSERT(opt.stats().dead_instructions_eliminated > 0, "Dead code elimination should have removed unused/dead instructions");

    StackVM vm;
    int res = vm.execute(opt_ir.kernels[0]);
    TEST_ASSERT(res == 5, "Return value should be 5");
    return true;
}

// 5. StackVM End-to-End Execution Tests
bool test_stackvm_control_flow_factorial() {
    std::string src = R"(
        kernel factorial(int n) {
            int result = 1;
            while (n > 1) {
                result = result * n;
                n = n - 1;
            }
            return result;
        }
    )";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    SemanticAnalyzer sema;
    sema.analyze(*ast);
    TEST_ASSERT(!sema.has_errors(), "Factorial kernel semantic check failed");

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    StackVM vm;
    int fact5 = vm.execute(opt_ir.kernels[0], {5});
    TEST_ASSERT(fact5 == 120, "factorial(5) should be 120, got " + std::to_string(fact5));

    int fact6 = vm.execute(opt_ir.kernels[0], {6});
    TEST_ASSERT(fact6 == 720, "factorial(6) should be 720, got " + std::to_string(fact6));
    return true;
}

bool test_stackvm_fibonacci() {
    std::string src = R"(
        kernel fib(int n) {
            if (n <= 0) {
                return 0;
            }
            if (n == 1) {
                return 1;
            }
            int a = 0;
            int b = 1;
            int i = 2;
            while (i <= n) {
                int c = a + b;
                a = b;
                b = c;
                i = i + 1;
            }
            return b;
        }
    )";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    SemanticAnalyzer sema;
    sema.analyze(*ast);
    TEST_ASSERT(!sema.has_errors(), "Fibonacci kernel semantic check failed");

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    StackVM vm;
    int fib0 = vm.execute(opt_ir.kernels[0], {0});
    int fib1 = vm.execute(opt_ir.kernels[0], {1});
    int fib7 = vm.execute(opt_ir.kernels[0], {7}); // 0, 1, 1, 2, 3, 5, 8, 13

    TEST_ASSERT(fib0 == 0, "fib(0) should be 0");
    TEST_ASSERT(fib1 == 1, "fib(1) should be 1");
    TEST_ASSERT(fib7 == 13, "fib(7) should be 13, got " + std::to_string(fib7));
    return true;
}

// 6. PTX Emission Test
bool test_ptx_emission() {
    std::string src = R"(
        kernel saxpy(int a, int x, int y) {
            int prod = a * x;
            int res = prod + y;
            return res;
        }
    )";
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    IRGenerator ir_gen;
    auto ir = ir_gen.generate(*ast);

    Optimizer opt;
    auto opt_ir = opt.optimize(ir);

    PTXEmitter ptx_emitter;
    std::string ptx = ptx_emitter.emit(opt_ir);

    TEST_ASSERT(ptx.find(".version 7.0") != std::string::npos, "PTX should declare .version 7.0");
    TEST_ASSERT(ptx.find(".target sm_75") != std::string::npos, "PTX should declare .target sm_75");
    TEST_ASSERT(ptx.find(".visible .entry saxpy") != std::string::npos, "PTX should declare .entry saxpy");
    TEST_ASSERT(ptx.find("ld.param.s32") != std::string::npos, "PTX should load parameters");
    TEST_ASSERT(ptx.find("add.s32") != std::string::npos, "PTX should emit add.s32");
    TEST_ASSERT(ptx.find("ret;") != std::string::npos, "PTX should emit ret;");
    return true;
}

int main() {
    std::cout << "===================================================\n";
    std::cout << " ptxcc Compiler Test Suite (C++20)\n";
    std::cout << "===================================================\n";

    size_t passed_count = 0;
    size_t failed_count = 0;

    RUN_TEST(test_lexer_tokens);
    RUN_TEST(test_lexer_malformed_input);
    RUN_TEST(test_parser_precedence);
    RUN_TEST(test_parser_malformed_syntax);
    RUN_TEST(test_semantic_undeclared_variable);
    RUN_TEST(test_semantic_duplicate_variable);
    RUN_TEST(test_optimizer_constant_folding);
    RUN_TEST(test_optimizer_algebraic_simplification);
    RUN_TEST(test_optimizer_dead_code_elimination);
    RUN_TEST(test_stackvm_control_flow_factorial);
    RUN_TEST(test_stackvm_fibonacci);
    RUN_TEST(test_ptx_emission);

    std::cout << "===================================================\n";
    std::cout << " Test Summary: " << passed_count << " Passed, " << failed_count << " Failed\n";
    std::cout << "===================================================\n";

    return (failed_count == 0) ? 0 : 1;
}
