# ptxcc: Modern C++20 Compiler Pipeline targeting NVIDIA PTX

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Target](https://img.shields.io/badge/Target-NVIDIA%20PTX%20sm__75-76B900.svg?logo=nvidia)](https://docs.nvidia.com/cuda/parallel-thread-execution/)
[![Build Status](https://img.shields.io/badge/Tests-12%20Passed%20%2F%200%20Failed-brightgreen.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A complete, modular compiler pipeline implemented from scratch in **modern C++20**, designed for a domain-specific kernel language and targeting **NVIDIA PTX (Parallel Thread Execution) Virtual ISA** as well as an in-memory **Stack Virtual Machine** bytecode interpreter.

Built to demonstrate compiler engineering principles directly relevant to GPU compiler toolchains (such as NVIDIA's PTX compiler and NVVM): lexical analysis, recursive-descent parsing, strongly-typed AST, lexical scoping with semantic validation, 3-address code linear IR, iterative dataflow optimization passes, and PTX target code emission.

---

## Compiler Architecture

```
                  +--------------------------------+
                  |  Source Code (*.nv kernel DSL) |
                  +--------------------------------+
                                  |
                                  v
                      +------------------------+
                      |      Lexer / Token     |  (Line/Column tracking,
                      +------------------------+   Comment/Whitespace stripping)
                                  |
                                  v
                      +------------------------+
                      | Recursive-Descent      |  (Operator precedence,
                      | Parser                 |   Syntax error diagnostics)
                      +------------------------+
                                  |
                                  v
                      +------------------------+
                      | Abstract Syntax Tree   |  (Strongly-typed AST Nodes)
                      +------------------------+
                                  |
                                  v
                      +------------------------+
                      | Semantic Analyzer &    |  (Scope resolution,
                      | Symbol Table           |   Undeclared/Duplicate checks)
                      +------------------------+
                                  |
                                  v
                      +------------------------+
                      | Linear IR Generator    |  (Three-Address Code (3AC),
                      |                        |   Virtual Registers %r0, %r1...)
                      +------------------------+
                                  |
                                  v
             +------------------------------------------+
             |         Optimization Pipeline            |
             |  1. Constant Folding & Propagation       |
             |  2. Algebraic Identity Simplification    |
             |  3. Dead Code Elimination (DCE)          |
             +------------------------------------------+
                                  |
                 +----------------+----------------+
                 |                                 |
                 v                                 v
    +--------------------------+     +--------------------------+
    |   NVIDIA PTX Emitter     |     |   StackVM Interpreter    |
    | - .version 7.0 / sm_75   |     | - In-memory execution    |
    | - Parameter loading      |     | - Loop/Branch dispatch   |
    | - Arithmetic / Branches  |     | - Exact return value     |
    +--------------------------+     +--------------------------+
```

---

## Language Grammar Specification

```ebnf
program         ::= kernel_decl* EOF ;
kernel_decl     ::= "kernel" IDENTIFIER "(" param_list? ")" block_stmt ;
param_list      ::= param ("," param)* ;
param           ::= "int" IDENTIFIER ;
block_stmt      ::= "{" stmt* "}" ;

stmt            ::= var_decl
                  | assign_stmt
                  | if_stmt
                  | while_stmt
                  | return_stmt
                  | block_stmt ;

var_decl        ::= "int" IDENTIFIER ("=" expr)? ";" ;
assign_stmt     ::= IDENTIFIER "=" expr ";" ;
if_stmt         ::= "if" "(" expr ")" stmt ("else" stmt)? ;
while_stmt      ::= "while" "(" expr ")" stmt ;
return_stmt     ::= "return" expr? ";" ;

expr            ::= equality ;
equality        ::= comparison (("==" | "!=") comparison)* ;
comparison      ::= term (("<" | "<=" | ">" | ">=") term)* ;
term            ::= factor (("+" | "-") factor)* ;
factor          ::= unary (("*" | "/" | "%") unary)* ;
unary           ::= ("-") unary | primary ;
primary         ::= INT_LITERAL | IDENTIFIER | "(" expr ")" ;
```

---

## Optimization Pipeline

The optimizer uses a fixed-point iteration strategy across three modular passes:

1. **Constant Folding & Constant Propagation**:
   - Compile-time evaluation of constant arithmetic: `10 + 20 * 3` is evaluated to immediate `70`.
   - Propagates known constants through virtual register assignments within the same basic block.
2. **Algebraic Identity Simplification**:
   - Identity addition/subtraction: `x + 0` $\rightarrow$ `x`, `x - 0` $\rightarrow$ `x`.
   - Multiplicative identity & zero: `x * 1` $\rightarrow$ `x`, `x * 0` $\rightarrow$ `0`.
   - Division identity: `x / 1` $\rightarrow$ `x`.
3. **Dead Code Elimination (DCE)**:
   - Eliminates dead instructions after unconditional branches (`bra`) or `ret` prior to the subsequent label.
   - Computes live registers and eliminates assignments to virtual registers that are never read downstream.

---

## Generated PTX Assembly Example

Compiling `examples/arithmetic.nv`:
```c
kernel compute_constants() {
    int x = 100;
    int folded = 10 + 20 * 3;     // Folds to 70 at compile-time
    int simplified = x * 1 + 0;   // Simplifies to x
    int result = folded + simplified;
    return result;
}
```

Emitted PTX (`ptxcc --emit-ptx examples/arithmetic.nv`):
```ptx
// Generated by ptxcc (NVIDIA PTX Toy Compiler)
.version 7.0
.target sm_75
.address_size 64

.visible .entry compute_constants(

)
{
    .reg .s32 %r<11>;

    mov.s32 %r7, 170;
    ret;
}
```
*Notice how constant folding and algebraic simplification fully collapsed the entire arithmetic sequence down to immediate `170` at compile time.*

Compiling `examples/vector_kernel.nv`:
```c
kernel saxpy_element(int a, int x, int y) {
    int ax = a * x;
    int result = ax + y;
    return result;
}
```

Emitted PTX:
```ptx
// Generated by ptxcc (NVIDIA PTX Toy Compiler)
.version 7.0
.target sm_75
.address_size 64

.visible .entry saxpy_element(
    .param .u32 a,
    .param .u32 x,
    .param .u32 y
)
{
    .reg .s32 %r<17>;

    ld.param.s32 %r0, [a];
    ld.param.s32 %r1, [x];
    ld.param.s32 %r2, [y];

    mul.lo.s32 %r4, %r0, %r1;
    mov.s32 %r3, %r4;
    add.s32 %r6, %r3, %r2;
    mov.s32 %r5, %r6;
    ret;
}
```

---

## Building and Testing

### Prerequisites
* C++20 compliant compiler (GCC 12+, Clang 14+, or MSVC 2019+)
* CMake 3.20+
* Ninja or Make

### Build
```bash
cmake -S . -B build -G "Ninja"
cmake --build build
```

### Run Test Suite
```bash
./build/ptxcc_tests
```
Output:
```
===================================================
 ptxcc Compiler Test Suite (C++20)
===================================================
[*] Running test_lexer_tokens... PASSED
[*] Running test_lexer_malformed_input... PASSED
[*] Running test_parser_precedence... PASSED
[*] Running test_parser_malformed_syntax... PASSED
[*] Running test_semantic_undeclared_variable... PASSED
[*] Running test_semantic_duplicate_variable... PASSED
[*] Running test_optimizer_constant_folding... PASSED
[*] Running test_optimizer_algebraic_simplification... PASSED
[*] Running test_optimizer_dead_code_elimination... PASSED
[*] Running test_stackvm_control_flow_factorial... PASSED
[*] Running test_stackvm_fibonacci... PASSED
[*] Running test_ptx_emission... PASSED
===================================================
 Test Summary: 12 Passed, 0 Failed
===================================================
```

### CLI Usage
```bash
# Emit PTX Virtual Assembly
./build/ptxcc --emit-ptx examples/vector_kernel.nv -o kernel.ptx

# View 3AC Linear IR
./build/ptxcc --emit-ir examples/fibonacci.nv

# View Optimized 3AC Linear IR
./build/ptxcc --emit-opt-ir examples/arithmetic.nv

# Execute directly on internal StackVM interpreter with input arguments
./build/ptxcc --run --args 7 examples/fibonacci.nv
# Output: Kernel 'fibonacci' returned: 13
```

---

## Design Decisions & Engineering Trade-offs

1. **Hand-Written Recursive Descent vs. Parser Generator (Bison/ANTLR)**:
   - A hand-written recursive descent parser was selected to provide precise, human-readable syntax diagnostics with source code column offsets, and to avoid heavyweight external runtime dependencies.
2. **Linear Three-Address Code (3AC) vs. Graph-Based SSA**:
   - A linear 3AC intermediate representation with virtual registers (`%r0`, `%r1`) mirrors the structure of NVIDIA's PTX virtual machine register model. This simplifies lowering to PTX parameter registers while keeping dataflow analysis (def-use chains) clean and maintainable.
3. **Dual Backends (StackVM + PTX Emitter)**:
   - Emitting PTX proves compatibility with the target parallel virtual architecture. The in-memory StackVM interpreter provides 100% automated test verification without requiring a live GPU driver during CI test execution.
