# RoCC Accelerator Integration

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

Rocket Custom Coprocessor (RoCC) provides an interface for attaching custom hardware accelerators to a Rocket processor.

This tutorial uses the example accumulator accelerator included with the course repository to demonstrate the complete flow:

1. inspect the RoCC accelerator;
2. build an accelerated Rocket system;
3. build the corresponding software test; and
4. execute the test on the accelerated architecture.

RoCC will primarily be used for semester projects.

---

# 1. The Course RoCC Example

The course repository contains:

```text
course/configs/ROCC.scala
```

This file is based on Chipyard's example accumulator accelerator and packages the accelerator together with the configuration required to attach it to Rocket.

The original course tutorial divides the file conceptually into:

| Section                    | Purpose                           |
| -------------------------- | --------------------------------- |
| Imports                    | Required Chipyard/Rocket classes  |
| Accelerator implementation | Hardware behavior                 |
| Accelerator configuration  | Attaches accelerator              |
| SoC configuration          | Selects accelerator and processor |

The exact line numbers may change if the file is updated, so focus on the classes rather than line numbers.

---

# 2. Examine the Accelerator Configuration

Open:

```text
ROCC.scala
```

and locate the accelerator configuration class.

The example contains a configuration similar to:

```scala
class WithAccumulator(
  op: OpcodeSet = OpcodeSet.custom0
) extends Config((site, here, up) => {
    ...
})
```

The important part is:

```scala
OpcodeSet.custom0
```

This tells Rocket that the accelerator is invoked using the RISC-V custom opcode space `custom0`.

RISC-V reserves custom opcode spaces specifically so designers can add implementation-specific instructions without conflicting with standard ISA instructions.

---

# 3. The Accelerator Is Invoked by Custom Instructions

Conceptually, the software executes:

```text
custom instruction
        |
        v
Rocket decoder
        |
        v
RoCC command interface
        |
        v
Accumulator accelerator
```

The processor sends the accelerator:

* an instruction;
* operand values;
* control information.

The accelerator performs its operation and can return a result.

The example accumulator demonstrates this mechanism without requiring you to design a new accelerator first.

---

# 4. Locate the `ROCCTest` Configuration

The supplied `ROCC.scala` defines a complete system configuration named:

```text
ROCCTest
```

This configuration attaches the accumulator to a Rocket-based system.

You can verify that Chipyard recognizes it with:

```bash
cd /workspace/chipyard/sims/verilator
make find-configs
```

Look for:

```text
ROCCTest
```

---

# 5. Build the Accelerated Processor

From:

```bash
cd /workspace/chipyard/sims/verilator
```

run:

```bash
make CONFIG=ROCCTest
```

This generates the hardware for the Rocket + RoCC system and compiles the Verilator simulator.

The resulting simulator should be:

```text
simulator-chipyard.harness-ROCCTest
```

This is a different hardware system from:

```text
simulator-chipyard.harness-CourseRocketConfig
```

because the RoCC-enabled system contains additional accelerator hardware.

---

# 6. Build the Accumulator Software Test

Chipyard includes an example software test for the accumulator.

Move to:

```bash
cd /workspace/chipyard/tests
```

If you have not already created the test build directory:

```bash
mkdir -p build
cd build
cmake ..
make
```

If the build directory already exists:

```bash
cd /workspace/chipyard/tests/build
cmake ..
make
```

This should build:

```text
/workspace/chipyard/tests/accum.riscv
```

---

# 7. Run the Accumulator Test

Return to the Verilator directory:

```bash
cd /workspace/chipyard/sims/verilator
```

Run:

```bash
./simulator-chipyard.harness-ROCCTest ../../tests/build/accum.riscv
```

If the accelerator and software interface are configured correctly, the accumulator test should complete successfully.

You have now executed software that invokes a custom hardware accelerator attached to Rocket.

---

# 8. What Just Happened?

The complete path was:

```text
accum.riscv
     |
     v
Rocket processor
     |
custom0 instruction
     |
     v
RoCC interface
     |
     v
Accumulator accelerator
     |
     v
result returned to Rocket
```

This is fundamentally different from simply calling an ordinary C function.

The relevant operation is implemented in hardware attached to the processor.

---

# 9. Baseline vs. Accelerated Architecture

For a real accelerator experiment, preserve both:

```text
CourseRocketConfig
```

and:

```text
YourAcceleratedConfig
```

Conceptually:

```text
                     +----------------------+
                     | CourseRocketConfig   |
Application -------->| software kernel      |
                     +----------------------+

                     +----------------------+
                     | AcceleratedConfig    |
Application -------->| Rocket + RoCC        |
                     +----------------------+
```

The goal is to compare the same application with and without specialized hardware.

---

# 10. Why Profiling Comes First

Suppose an application requires:

```text
1,000,000 cycles
```

and a candidate function requires:

```text
100,000 cycles
```

Then:

$$
f = 0.10
$$

Even if an accelerator makes that function infinitely fast:

$$
S_{max} =
\frac{1}{1-f}
$$

so:

$$ S_{max} = \frac{1}{0.90} = 1.11 $$

The theoretical maximum application speedup is only about:

$$
1.11\times
$$

Therefore:

> **Do not choose an accelerator target merely because it is easy to implement in hardware.**

Choose it because application profiling shows that accelerating it can matter.

---

# 11. Accelerator Invocation Has Overhead

Suppose the original software kernel requires:

```text
100 cycles
```

The accelerator itself requires:

```text
10 cycles
```

but communication requires another:

```text
15 cycles
```

Then the effective accelerated time is:

$$
10+15=25\ cycles
$$

and effective kernel speedup is:

$$ \frac{100}{25} = 4\times $$

not:

$$
10\times.
$$

A useful accelerator must account for:

* command overhead;
* operand transfer;
* result transfer;
* memory access;
* synchronization;
* accelerator execution.

---

# 12. Implementing Your Own Accelerator

To create a new accelerator, do **not** modify the accumulator example destructively.

Instead, create a new Scala file under:

```text
/workspace/chipyard/generators/chipyard/src/main/scala/config/
```

For example:

```text
MyAccelerator.scala
```

Use the supplied:

```text
ROCC.scala
```

as a reference.

A typical structure is:

```text
MyAccelerator.scala
|
+-- imports
|
+-- accelerator hardware class
|
+-- accelerator configuration fragment
|
+-- complete Chipyard configuration
```

For example:

```text
WithMyAccelerator
```

could describe how the accelerator is attached, while:

```text
MyAcceleratedConfig
```

could define the complete system.

---

# 13. Use a Separate Custom Opcode

The example uses:

```scala
OpcodeSet.custom0
```

Other custom opcode spaces may be available, depending on the configuration.

If a project includes more than one custom accelerator or instruction extension, make sure the opcode spaces do not conflict.

Assignments will provide specific guidance when multiple custom operations are involved.

---

# 14. Software Support

Your C code needs a way to issue the corresponding custom instruction.

The supplied `accum.riscv` test demonstrates how Chipyard's accumulator example performs that interaction.

Before writing your own interface, inspect the source code associated with the accumulator test under:

```text
/workspace/chipyard/tests
```

Look for the source that produces:

```text
accum.riscv
```

Pay particular attention to:

* the custom instruction invocation;
* source register operands;
* destination register;
* function fields;
* how results are checked.

The software encoding and the accelerator hardware must agree.

---

# 15. Correctness First

Before measuring speedup:

```text
Software implementation
          |
          v
Known correct result
```

Then:

```text
Accelerated implementation
          |
          v
Compare result
```

Only after confirming equivalent behavior should you collect performance results.

A hardware accelerator that produces a different result is not an optimization.

---

# 16. Measuring the Accelerator

You can use the same performance-counter approach described in [Performance Counters](performance-counters.md).

For example:

```c
asm volatile(
    "csrr %0, mcycle"
    : "=r"(start)
);

/*
 * Invoke accelerator
 */

asm volatile(
    "csrr %0, mcycle"
    : "=r"(end)
);
```

Then compare:

```text
software execution cycles
```

against:

```text
accelerator invocation cycles
```

However, the most important measurement is usually **overall application performance**, not merely accelerator latency.

---

# 17. A Good Accelerator Experiment

Suppose profiling identifies:

```text
feature_extraction()
```

as consuming:

```text
48% of application cycles
```

You might then:

```text
1. Measure feature_extraction() in software
2. Estimate achievable accelerator speedup
3. Apply Amdahl's Law
4. Implement accelerator
5. Verify correctness
6. Measure accelerator invocation
7. Measure complete application
8. Compare prediction with observed speedup
```

This connects hardware specialization directly to the performance analysis introduced earlier in the course.

---

# 18. Architectural Cost Matters

An accelerator is not free.

Depending on the project, evaluate:

* additional hardware;
* area;
* memory bandwidth;
* accelerator state;
* communication interface;
* energy;
* design complexity.

Therefore, the design question is not simply:

> Did the accelerator improve performance?

It is:

> Was the performance improvement sufficient to justify the additional architectural cost?

---

# 19. Suggested Project Workflow

For semester projects using RoCC:

```text
Profile application
        |
        v
Identify significant bottleneck
        |
        v
Predict maximum useful speedup
        |
        v
Design accelerator
        |
        v
Integrate through RoCC
        |
        v
Verify correctness
        |
        v
Measure kernel speedup
        |
        v
Measure application speedup
        |
        v
Analyze architectural tradeoff
```

---

# 20. Reference Example

For your first RoCC implementation, use:

```text
ROCC.scala
```

from the course repository as the reference implementation.

The basic test flow is:

```bash
cd /workspace/chipyard/sims/verilator

make CONFIG=ROCCTest
```

Then build the software:

```bash
cd /workspace/chipyard/tests

mkdir -p build
cd build

cmake ..
make
```

Finally:

```bash
cd /workspace/chipyard/sims/verilator

./simulator-chipyard.harness-ROCCTest \
  ../../tests/accum.riscv
```

If this succeeds, the complete Rocket → RoCC → accelerator → software path is working.

---

# 21. Key Lesson

RoCC is a mechanism for architectural specialization.

The educational objective is not merely to learn how to attach hardware to Rocket.

The more important question is:

> **When is specialized hardware the right response to an application bottleneck, and how much application-level improvement does it actually produce?**
