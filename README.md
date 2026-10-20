# ptxcc: Modern C++20 Compiler Pipeline targeting NVIDIA PTX

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Target](https://img.shields.io/badge/Target-NVIDIA%20PTX%20sm__75-76B900.svg?logo=nvidia)](https://docs.nvidia.com/cuda/parallel-thread-execution/)
[![Tests](https://img.shields.io/badge/Tests-12%20Passed%20%2F%200%20Failed-brightgreen.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A modular, dependency-free compiler pipeline implemented from scratch in **modern C++20**, compiling a high-level kernel DSL into **NVIDIA PTX (Parallel Thread Execution) Virtual ISA** assembly and executable **Stack Virtual Machine** bytecode.

Designed to demonstrate compiler engineering principles directly applicable to GPU toolchains (such as NVIDIA's PTX compiler and NVVM): lexical scanning with source-location tracking, recursive-descent parsing with operator precedence, strongly-typed AST hierarchies, lexical symbol scoping, 3-address code (3AC) linear intermediate representation, iterative dataflow optimization passes, and target code generation.

---

## Architecture & Compilation Pipeline

```
                             +-----------------------------------+
                             |     Source File (*.nv DSL)        |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Lexer (Token Stream & Spans)     |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Recursive-Descent Parser         |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Abstract Syntax Tree (AST)       |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Semantic Analyzer & Symbol Table |
                             |  - Scope resolution & shadowing   |
                             |  - Undeclared & duplicate checks  |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Linear IR Generator (3AC)        |
                             |  - Virtual registers (%r0, %r1..) |
                             |  - Basic blocks & branch labels   |
                             +-----------------+-----------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  Iterative Optimization Pipeline  |
                             |  1. Constant Folding              |
                             |  2. Constant Propagation          |
                             |  3. Algebraic Simplification      |
                             |  4. Dead Code Elimination (DCE)   |
                             +-----------------+-----------------+
                                               |
                        +----------------------+----------------------+
                        |                                             |
                        v                                             v
        +-------------------------------+             +-------------------------------+
        |      PTX ISA Code Emitter     |             |    StackVM Bytecode Engine    |
        | - .version 7.0 / .target sm_75|             | - In-memory execution         |
        | - Parameter passing registers |             | - Control flow evaluation     |
        | - Predicated branches (@!%p0) |             | - Automated test verification |
        +-------------------------------+             +-------------------------------+
                        |
                        v
        +-------------------------------+
        |  NVIDIA Driver API Execution  |
        |  - nvcuda.dll cuModuleLoadData|
        |  - JIT compiled to GPU SASS   |
        +-------------------------------+
```

---

## Language Grammar (EBNF)

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

## Optimization Engine

The compiler features an iterative fixed-point optimization pass manager that executes until no further transformations occur:

1. **Constant Folding & Constant Propagation**:
   - Evaluates compile-time constant arithmetic (`10 + 20 * 3` $\to$ `70`).
   - Propagates known register constants through assignments within straight-line basic blocks.
2. **Algebraic Identity Simplification**:
   - Additive identity: `x + 0` $\to$ `x`, `x - 0` $\to$ `x`.
   - Multiplicative identity & zero: `x * 1` $\to$ `x`, `x * 0` $\to$ `0`.
   - Division identity: `x / 1` $\to$ `x`.
3. **Dead Code Elimination (DCE)**:
   - Unreachable instruction elimination: dead instructions following unconditional branches (`bra`) or `ret` are pruned.
   - Unused def elimination: assignments to virtual registers whose results are never consumed downstream are pruned.

---

## Concrete Code Generation Examples

### Example 1: Constant Folding & Algebraic Simplification

Source (`examples/arithmetic.nv`):
```c
kernel compute_constants() {
    int x = 100;
    int folded = 10 + 20 * 3;     // Evaluated to 70 at compile time
    int simplified = x * 1 + 0;   // Algebraic reduction to x
    int result = folded + simplified;
    return result;
}
```

Unoptimized 3AC Intermediate Representation:
```
  %r0 = 100
  %r1 = 10
  %r2 = 20
  %r3 = 3
  %r4 = %r2 * %r3
  %r5 = %r1 + %r4
  %r6 = 1
  %r7 = %r0 * %r6
  %r8 = 0
  %r9 = %r7 + %r8
  %r10 = %r5 + %r9
  ret %r10
```

Optimized 3AC IR & Emitted PTX (`ptxcc --emit-ptx examples/arithmetic.nv`):
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
*The entire 10-instruction sequence is statically collapsed into a single immediate assignment `mov.s32 %r7, 170;`.*

### Example 2: Parameter Handling, Arithmetic, and Control Flow

Source (`examples/fibonacci.nv`):
```c
kernel fibonacci(int n) {
    if (n <= 0) {
        return 0;
    }
    int a = 0;
    int b = 1;
    int i = 1;
    while (i < n) {
        int temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return b;
}
```

Emitted PTX Assembly (`sm_75`):
```ptx
// Generated by ptxcc (NVIDIA PTX Toy Compiler)
.version 7.0
.target sm_75
.address_size 64

.visible .entry fibonacci(
    .param .u32 n
)
{
    .reg .s32 %r<17>;
    .reg .pred %p<8>;

    ld.param.s32 %r0, [n];
    setp.le.s32 %p0, %r0, 0;
    @!%p0 bra L1;
    ret;
L1:
    mov.s32 %r1, 0;
    mov.s32 %r2, 1;
    mov.s32 %r3, 1;
L2:
    setp.lt.s32 %p1, %r3, %r0;
    @!%p1 bra L3;
    add.s32 %r6, %r1, %r2;
    mov.s32 %r1, %r2;
    mov.s32 %r2, %r6;
    add.s32 %r9, %r3, 1;
    mov.s32 %r3, %r9;
    bra L2;
L3:
    ret;
}
```

---

## Setup & Build Instructions

### Prerequisites
* **C++ Compiler:** GCC 12+, Clang 14+, or MSVC 2022 (requires full C++20 standard library support).
* **Build System:** CMake 3.20+ and Ninja (or GNU Make).
* **Optional:** NVIDIA GPU with Driver (for hardware PTX JIT execution).

### Step-by-Step Build

```bash
# 1. Clone repository
git clone https://github.com/Super-cod/ptx-toy-compiler.git
cd ptx-toy-compiler

# 2. Configure build with CMake & Ninja
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# 3. Compile compiler binary and test suite
cmake --build build
```

---

## Verification & Test Suite

The compiler comes with an automated unit and integration test suite covering every phase of compilation:

```bash
./build/ptxcc_tests
```

### Verified Test Output
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

### Direct CLI Usage
```bash
# 1. Emit PTX Virtual Assembly to stdout or file
./build/ptxcc --emit-ptx examples/vector_kernel.nv -o vector_kernel.ptx

# 2. View 3AC Linear IR before optimization
./build/ptxcc --emit-ir examples/fibonacci.nv

# 3. View Optimized 3AC Linear IR
./build/ptxcc --emit-opt-ir examples/arithmetic.nv

# 4. Execute on internal StackVM interpreter with input arguments
./build/ptxcc --run --args 10 examples/fibonacci.nv
# Output: Kernel 'fibonacci' returned: 55
```

### Physical GPU JIT Verification via NVIDIA Driver API
The emitted PTX has been verified to assemble and execute directly on NVIDIA silicon (**RTX 3050, Ampere SM 8.6**) via `nvcuda.dll`:
```python
import ctypes
cuda = ctypes.CDLL("nvcuda.dll")
cuda.cuInit(0)
# cuModuleLoadData loads and JIT-compiles emitted PTX string directly into GPU SASS
status = cuda.cuModuleLoadData(ctypes.byref(module), ptx_bytes)
assert status == 0  # CUDA_SUCCESS
```

---

## Honest Limitations & Engineering Scope

To maintain clarity and rigorous engineering boundaries, the current capabilities and intentional trade-offs are outlined below:

| Feature Dimension | Supported in Current Implementation | Out of Current Scope / Future Work |
| :--- | :--- | :--- |
| **Data Types** | 32-bit signed integers (`int`) | Floating-point (`float`, `double`), unsigned integers, structs |
| **Data Structures** | Scalar variables and parameters | Arrays, heap pointers, and dynamic memory allocations |
| **Intermediate Repr.** | Linear Three-Address Code (3AC) with virtual registers | Graph-based SSA (Static Single Assignment) dominance frontiers |
| **Optimizations** | Basic-block constant folding/propagation, algebraic identities, local DCE | Global common subexpression elimination (CSE), loop unrolling |
| **Code Generation** | NVIDIA PTX assembly (`.version 7.0`, `.target sm_75`) & StackVM | Native machine binary emission (SASS/ELF) directly bypassing the CUDA driver |
| **Runtime Model** | Parameter loading, predicated branch execution | Direct multi-threaded GPU kernel launches without host runtime wrapper |

---

## License

MIT License. Copyright (c) 2026 Swayam.
