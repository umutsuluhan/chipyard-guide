# Performance Counters

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

This tutorial shows how to measure the number of processor cycles and retired instructions for a region of code running on Rocket.

These measurements provide the basis for calculating CPI, speedup, and other performance metrics used throughout the course.

Before continuing, complete the [Running Programs](running-programs.md) tutorial so that you are comfortable creating and executing a custom RISC-V test.

---

# 1. RISC-V Performance Counters

RISC-V provides hardware counters that can be read through control and status registers (CSRs).

Two important counters are:

```text
mcycle
minstret
```

`mcycle` records processor cycles.

`minstret` records retired instructions.

These counters can be read directly from C using inline RISC-V assembly.

---

# 2. Create a Performance Test

Move to the Chipyard test directory:

```bash
cd /workspace/chipyard/tests
```

Create a new test:

```bash
touch perf_test.c
```

Add the following code:

```c
#include <stdint.h>
#include <stdio.h>

int main(void) {

    uint64_t start_c;
    uint64_t end_c;

    uint64_t start_i;
    uint64_t end_i;

    /*
     * Record counters immediately before the
     * region of code we want to measure.
     */
    asm volatile(
        "csrr %0, mcycle"
        : "=r"(start_c)
    );

    asm volatile(
        "csrr %0, minstret"
        : "=r"(start_i)
    );

    /*
     * Region of interest
     */
    for (volatile int i = 0; i < 1000; i++) {
        /*
         * Replace this with the code
         * you want to measure.
         */
    }

    /*
     * Record counters immediately after
     * the region of interest.
     */
    asm volatile(
        "csrr %0, mcycle"
        : "=r"(end_c)
    );

    asm volatile(
        "csrr %0, minstret"
        : "=r"(end_i)
    );

    printf(
        "Cycles: %lu\n",
        end_c - start_c
    );

    printf(
        "Instructions: %lu\n",
        end_i - start_i
    );

    return 0;
}
```

This is the same basic measurement approach used in the original Chipyard course guide.

---

# 3. Add the Test to CMake

Open:

```text
/workspace/chipyard/tests/CMakeLists.txt
```

Add:

```cmake
add_executable(perf_test perf_test.c)
```

and:

```cmake
add_dump_target(perf_test)
```

Place these alongside the corresponding declarations for the other tests.

Do not rely on a specific line number because the file may change between Chipyard versions.

---

# 4. Build the Test

If the test build directory already exists:

```bash
cd /workspace/chipyard/tests/build
cmake ..
make
```

Otherwise:

```bash
cd /workspace/chipyard/tests

mkdir -p build
cd build

cmake ..
make
```

After a successful build, the RISC-V executable should be:

```text
/workspace/chipyard/tests/build/perf_test.riscv
```

---

# 5. Run the Test on Rocket

Move to the Verilator simulation directory:

```bash
cd /workspace/chipyard/sims/verilator
```

If necessary, build the course processor:

```bash
make CONFIG=CourseRocketConfig
```

Run:

```bash
./simulator-chipyard.harness-CourseRocketConfig ../../tests/build/perf_test.riscv
```

You should see output containing values similar to:

```text
Cycles: 6134
Instructions: 5012
```

Your exact values may differ.

---

# 6. What Are We Measuring?

The expressions:

```c
end_c - start_c
```

and:

```c
end_i - start_i
```

measure the change in the counters across the region of interest.

Conceptually:

```text
read counters
     |
     v
+-------------------+
| region of interest|
+-------------------+
     |
     v
read counters
```

The measurements therefore exclude most program initialization and output activity.

This is preferable to measuring the entire application when only one kernel or code region is under investigation.

---

# 7. Calculating CPI

Cycles per instruction is:

$$
CPI =
\frac{Cycles}{Instructions}
$$

For example, suppose the program reports:

```text
Cycles       = 6,134
Instructions = 5,012
```

Then:

$$
CPI =
\frac{6134}{5012}
\approx 1.224
$$

CPI tells us the average number of cycles required per retired instruction during the measured region.

---

# 8. Modify the Region of Interest

Replace:

```c
for (volatile int i = 0; i < 1000; i++) {
    /*
     * Replace this with the code
     * you want to measure.
     */
}
```

with a real computation.

For example:

```c
volatile uint64_t sum = 0;

for (volatile int i = 0; i < 1000; i++) {
    sum += i;
}
```

Rebuild:

```bash
cd /workspace/chipyard/tests/build
make
```

Then run the test again:

```bash
cd /workspace/chipyard/sims/verilator

./simulator-chipyard.harness-CourseRocketConfig \
  ../../tests/perf_test.riscv
```

Because only the software changed, you do **not** need to rebuild the Rocket simulator.

---

# 9. Why Use `volatile`?

The example deliberately uses:

```c
volatile int i
```

and may use `volatile` variables in the measured computation.

Without this, the compiler may optimize away code whose result is not externally observable.

For example:

```c
int result = 0;

for (int i = 0; i < 1000; i++) {
    result += i;
}
```

could potentially be transformed or eliminated by compiler optimization.

Performance experiments must measure the computation you think they are measuring.

---

# 10. Measure the Same Program on Two Architectures

Suppose you have built:

```text
CourseRocketConfig
BigCorewithDCacheUpdateConfig
```

Run:

```bash
./simulator-chipyard.harness-CourseRocketConfig \
  ../../tests/perf_test.riscv
```

Record:

```text
Cycles_baseline
Instructions_baseline
```

Then run:

```bash
./simulator-chipyard.harness-BigCorewithDCacheUpdateConfig \
  ../../tests/perf_test.riscv
```

Record:

```text
Cycles_modified
Instructions_modified
```

Now you can directly compare the same software on two architectures.

---

# 11. Calculating Speedup

If the two architectures are being compared under the same clock-period assumption:

$$
Speedup =
\frac{Cycles_{baseline}}
{Cycles_{modified}}
$$

For example:

```text
Baseline cycles = 10,000
Modified cycles = 8,000
```

then:

$$ Speedup = \frac{10000}{8000} = 1.25 $$

The modified architecture provides a:

$$
1.25\times
$$

speedup for that workload.

---

# 12. Percentage Reduction Is Different from Speedup

For the same example:

```text
Baseline = 10,000 cycles
Modified = 8,000 cycles
```

the cycle reduction is:

$$ \frac{10000-8000}{10000} = 0.20 = 20\% $$

but the speedup is:

$$
1.25\times
$$

Do not use these terms interchangeably.

---

# 13. Connecting the Measurement to the CPU Performance Equation

Recall:

$$
CPU\ Time =
Instruction\ Count
\times CPI
\times Clock\ Cycle\ Time
$$

Since:

$$
CPI =
\frac{Cycles}{Instructions},
$$

we can use the counters to determine whether an architectural change affects:

* instruction count,
* CPI,
* or both.

For example:

| Configuration | Instructions | Cycles |  CPI |
| ------------- | -----------: | -----: | ---: |
| Baseline      |       10,000 | 15,000 | 1.50 |
| Modified      |       10,000 | 12,000 | 1.20 |

Here, instruction count is unchanged.

The improvement came from a lower CPI.

---

# 14. Measuring a Specific Function

You can place the counter reads around a specific function:

```c
asm volatile(
    "csrr %0, mcycle"
    : "=r"(start_c)
);

asm volatile(
    "csrr %0, minstret"
    : "=r"(start_i)
);

compute_kernel();

asm volatile(
    "csrr %0, mcycle"
    : "=r"(end_c)
);

asm volatile(
    "csrr %0, minstret"
    : "=r"(end_i)
);
```

This allows you to profile different portions of an application separately.

For example:

```text
filter()
feature_extraction()
classification()
output_processing()
```

could each be measured individually.

This will become important when applying Amdahl's Law.

---

# 15. Using Measurements with Amdahl's Law

Suppose a program executes for:

```text
100,000 cycles
```

and profiling shows that one function accounts for:

```text
40,000 cycles
```

Then the fraction of original execution time represented by that function is:

$$ f = \frac{40000}{100000} = 0.40 $$

If an architectural change accelerates that function by 4x:

$$
S =
\frac{1}
{(1-f)+\frac{f}{4}}
$$

so:

$$ S = \frac{1}{0.6+0.1} = 1.43 $$

The performance counters therefore allow us to move from a qualitative statement such as:

> "This looks like the expensive part."

to a quantitative statement:

> "This region accounts for 40% of execution cycles."

---

# 16. Measurement Overhead

Reading:

```text
mcycle
```

and:

```text
minstret
```

requires instructions.

Therefore, the measurement itself introduces a small amount of overhead.

For long regions of interest this is normally negligible.

For extremely short code regions, the overhead may represent a meaningful fraction of the measurement.

A common solution is to run the operation many times:

```c
for (volatile int i = 0; i < 1000; i++) {
    operation();
}
```

and measure the complete loop.

---

# 17. A Recommended Experiment Structure

For architecture experiments, use the following sequence:

```text
1. Measure baseline
2. Identify bottleneck
3. Predict effect of optimization
4. Modify architecture
5. Measure modified architecture
6. Calculate speedup
7. Explain difference between prediction and measurement
```

For example, your analysis should look like:

> The baseline required 1.4 million cycles. Based on the fraction of execution time attributable to memory stalls, we predicted a maximum speedup of 1.20×. The modified cache produced a measured speedup of 1.13×. The lower observed speedup suggests that reducing cache misses exposed other pipeline and memory-system costs.

---

# 18. What to Record

Unless an assignment says otherwise, record:

| Metric               | Value |
| -------------------- | ----: |
| Cycles               |       |
| Retired instructions |       |
| CPI                  |       |
| Speedup vs. baseline |       |

For comparisons:

| Configuration      | Cycles | Instructions | CPI | Speedup |
| ------------------ | -----: | -----------: | --: | ------: |
| CourseRocketConfig |        |              |     |   1.00× |
| ModifiedConfig     |        |              |     |         |

Assignments may later add measurements such as:

* cache misses,
* branch mispredictions,
* memory accesses,
* stall behavior,
* accelerator execution time.

---

# 19. Key Lesson

The performance counters are not merely values to report. They allow us to answer the central question of the course:

> **What limits the performance of this application, and did our architectural change actually address that bottleneck?**
