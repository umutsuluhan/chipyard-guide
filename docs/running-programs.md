# Running Default and Custom RISC-V Programs

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

This guide explains how to build and run the test programs included with Chipyard and how to add your own RISC-V program.

Before continuing, complete the [Getting Started](/docs/getting-started.md) guide.

---

## 1. Start the Course Environment

From the root of the course repository on your host computer:

```bash
docker compose run --rm chipyard
```

Inside the container:

```bash
cd /workspace/chipyard
source env.sh
```

The `env.sh` script configures the RISC-V toolchain and other Chipyard environment variables. It should be sourced each time you start a new container.

---

## 2. Build the Chipyard Test Programs

Chipyard includes a collection of test programs under:

```text
/workspace/chipyard/tests
```

Build them with:

```bash
cd /workspace/chipyard/tests
mkdir -p build
cd build
cmake ..
make
```

This process generates RISC-V executables under the `tests/` directory.

For example:

```text
hello.riscv
```

---

## 3. Build the Course Rocket Simulator

Move to:

```bash
cd /workspace/chipyard/sims/verilator
```

Build the standard course processor:

```bash
make CONFIG=CourseRocketConfig
```

The generated simulator will be:

```text
simulator-chipyard.harness-CourseRocketConfig
```

If you already built this configuration and have not modified the hardware, you do not need to rebuild it.

---

## 4. Run the Included `hello` Program

From:

```text
/workspace/chipyard/sims/verilator
```

run:

```bash
./simulator-chipyard.harness-CourseRocketConfig ../../tests/build/hello.riscv
```

You should see output similar to:

```text
[UART] UART0 is here (stdin/stdout).
Hello world from core 0, a rocket
```

followed by the normal Verilator termination message.

This confirms that the test executable is running on the simulated Rocket processor.

---

# Adding a Custom Test

We will now create a simple RISC-V program and execute it on Rocket.

## 5. Create `ecex62.c`

Move to:

```bash
cd /workspace/chipyard/tests
```

Create:

```bash
touch ecex62.c
```

Open the file in an editor and add:

```c
#include <stdio.h>
#include <riscv-pk/encoding.h>
#include <stdint.h>

int main(void) {
    printf("Welcome to ECE462/562\n");

    uint64_t a = 1;
    uint64_t b = 2;
    uint64_t c = a + b;

    printf("The result of addition is: %lu\n", c);

    return 0;
}
```

---

## 6. Add the Program to the Chipyard Test Build

Open:

```text
/workspace/chipyard/tests/CMakeLists.txt
```

Find the section containing the existing `add_executable(...)` declarations and add:

```cmake
add_executable(ecex62 ecex62.c)
```

Then find the section containing the existing `add_dump_target(...)` declarations and add:

```cmake
add_dump_target(ecex62)
```

The precise line numbers may change between versions, so locate the surrounding declarations rather than relying on a specific line number.

---

## 7. Rebuild the Tests

Return to the test build directory:

```bash
cd /workspace/chipyard/tests/build
cmake ..
make
```

This should create:

```text
/workspace/chipyard/tests/ecex62.riscv
```

---

## 8. Run the Custom Program

Return to:

```bash
cd /workspace/chipyard/sims/verilator
```

Run:

```bash
./simulator-chipyard.harness-CourseRocketConfig ../../tests/build/ecex62.riscv
```

You should see output similar to:

```text
[UART] UART0 is here (stdin/stdout).
Welcome to ECE462/562
The result of addition is: 3
```

followed by the normal Verilator termination message.

You have now:

1. written a C program;
2. compiled it for RISC-V;
3. built a Rocket processor simulator; and
4. executed the program on the simulated processor.

---

# Using Custom Programs for Architecture Experiments

The same procedure can be used to create targeted architectural benchmarks.

For example, if you are studying branch prediction, you might create a program containing control-flow patterns designed to stress the branch predictor.

If you are studying caches, you might create a program with:

* sequential array accesses;
* strided accesses;
* repeated reuse;
* conflicting memory addresses.

If you are studying pipeline behavior, you might create instruction sequences containing different data dependencies.

The important point is that the application is executed on the actual simulated Rocket architecture.

---

# Software Changes vs. Hardware Changes

If you change only:

```text
ecex62.c
```

you normally need only to rebuild the tests:

```bash
cd /workspace/chipyard/tests/build
make
```

You do **not** need to rebuild the Verilator simulator.

If you change the architecture—for example:

* cache configuration;
* branch predictor;
* Rocket hardware;
* Chipyard configuration;

you must rebuild the corresponding simulator:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=<YourConfig>
```

Remember:

```text
software change
    ↓
recompile program

hardware change
    ↓
rebuild simulator
```

Hardware builds are much more expensive than software builds, so avoid unnecessary rebuilds.

---

# Next Steps

Continue with:

* [Performance Counters](/docs/performance-counters.md)
* [Custom Configurations](/docs/custom-configurations.md)
* [Cache Configuration](/docs/cache-configuration.md)

These guides build directly on the `ecex62.riscv` program created here.
