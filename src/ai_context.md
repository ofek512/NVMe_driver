# AI Context: Phase 1 Build Architecture

## Role and Project Scope
You are assisting a systems engineer in building a user-space C++ NVMe driver from scratch. This project bridges modern C++ object-oriented architecture with the strictly procedural C open-source frameworks SPDK and DPDK. The application will bypass the standard Linux kernel NVMe driver to perform Direct Memory Access reads and writes via physical PCIe motherboard registers.

## Phase 1 Objective
Design and implement a robust `Makefile` and a `main.cpp` skeleton that successfully compiles the C++ application against the static C archives of SPDK and DPDK without generating architecture mismatch or undefined reference errors.

## Strict Architectural Rules for Code Generation

1. **C++ Name Mangling Prevention**
The C++ compiler will alter function names, which breaks linkage to the C-based SPDK libraries. Every single SPDK or DPDK header included in the `main.cpp` file must be enclosed within `extern "C"` blocks.

2. **The DPDK Static Linking Problem**
DPDK is packaged as a massive tree of individual `.a` static archives. Standard compilation will strip out hardware drivers deemed unused by the linker, causing the compiled binary to fail during PCIe probing. The `Makefile` must explicitly pass `-Wl,--whole-archive` to the linker before attaching the DPDK libraries, and `-Wl,--no-whole-archive` immediately afterward.

3. **Dynamic Dependency Resolution**
Do not hardcode library names or paths in the `Makefile`. You must use `pkg-config` targeting the `spdk.pc` configuration file. The framework is located at `~/spdk`. The `Makefile` must dynamically extract `CFLAGS` and `LIBS` from this package configuration.

4. **Memory Allocation Rules**
Standard C++ memory operators like `new` or `malloc` provide virtual addresses and are strictly forbidden for data buffers interacting with the hardware. The AI must only use SPDK Environment Abstraction Layer memory functions for hardware communication.

## File Workspace
* Primary Entry Point: `src/main.cpp`
* Build Script: `src/Makefile`
* SPDK Installation Path: `~/spdk`

## Directives for AI Output
When the user requests the `Makefile` or `main.cpp` code, adhere strictly to these constraints. Generate production-ready code with no placeholders.
