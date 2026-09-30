# Building and Running the Course Benchmark

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

This guide explains how to build the baseline Rocket configuration, compile the course benchmark for RISC-V, and run the benchmark on the simulated processor.

Before continuing, complete the [Getting Started](/docs/getting-started.md) guide.

---

# 1. Start the Course Environment

From the root of the course repository on your host computer:

```bash
docker compose run --rm chipyard
```

Inside the container:

```bash
source env.sh
```

`env.sh` configures the RISC-V toolchain and the Chipyard environment variables. Source it each time you start a new container.

---

# 2. The Baseline Configuration

This course uses two Rocket configurations:

| Configuration        | Description                                                                                                                  |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `CourseRocketConfig` | Chipyard's standard Rocket core, unchanged.                                                                                 |
| `BaselineConfig`     | A deliberately minimal Rocket: no branch prediction, no FPU, slowest multiplier/divider, 4 KiB direct-mapped L1 caches, no L2. |

`BaselineConfig` is the reference point for the architecture experiments. Each later assignment adds one structure back and measures the difference against it.

Both configurations are defined in:

```text
course/configs/CourseRocketConfig.scala
```

The container makes this file available to Chipyard automatically when it starts (natively, `native-activate.sh` does). Do not edit it.

---

# 3. Build the Baseline Simulator

Move to the Verilator simulation directory:

```bash
cd /workspace/chipyard/sims/verilator
```

Build the baseline processor:

```bash
make CONFIG=BaselineConfig
```

The first build takes a while. When it finishes, the simulator is:

```text
simulator-chipyard.harness-BaselineConfig
```

You only need to rebuild it if the hardware configuration changes.

---

# 4. Build the Benchmark

The benchmark source is provided, read-only, at:

```text
/workspace/course/benchmarks
```

It contains:

```text
isp_bench.c     grayscale motion-detection camera pipeline
metrics.h       cycle / instruction / performance-counter harness
hpm.h           hardware performance-counter definitions
Makefile
```

Build it:

```bash
cd /workspace/course/benchmarks
make
```

The compiled RISC-V executable is written to:

```text
/workspace/output/isp_bench.riscv
```

The source directory is mounted read-only in the container, so the build writes its output to `/workspace/output` instead. A native install writes to `$WS/output` the same way.

To also produce a disassembly, useful for checking which instructions gcc emitted:

```bash
make dump
```

This writes `/workspace/output/isp_bench.dump`.

---

# 5. Run the Benchmark

From the Verilator directory:

```bash
cd /workspace/chipyard/sims/verilator
./simulator-chipyard.harness-BaselineConfig /workspace/output/isp_bench.riscv
```

The simulation prints a per-phase table followed by a checksum:

```text
isp_bench
phase          cycles    instret     branch       load        mul  bp_dir_ms  bp_tgt_ms   md_stall    ic_miss    dc_miss
gain              ...        ...        ...        ...        ...        ...        ...        ...        ...        ...
vfilter           ...        ...        ...        ...        ...        ...        ...        ...        ...        ...
downscale         ...        ...        ...        ...        ...        ...        ...        ...        ...        ...
bgmodel           ...        ...        ...        ...        ...        ...        ...        ...        ...        ...
detect            ...        ...        ...        ...        ...        ...        ...        ...        ...        ...
CHECKSUM,<number>
```

followed by the normal Verilator termination message.

Each row is one stage of the image pipeline:

* `cycles` and `instret` come from the `mcycle` and `minstret` counters.
* The remaining columns are the eight hardware performance counters defined in `hpm.h`: branch and load instruction counts, multiply count, branch-direction and branch-target mispredictions, multiply/divide stall cycles, and I- and D-cache misses.

Your exact counts will vary with the configuration. The `CHECKSUM` line must stay identical across every configuration: an architectural change alters how long the work takes, not the result.

---

# 6. Compare Against Another Configuration

When a later assignment adds a configuration, build it and run the same binary:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=<NewConfig>
./simulator-chipyard.harness-<NewConfig> /workspace/output/isp_bench.riscv
```

Because only the hardware changed, you do **not** rebuild the benchmark. Compare the two tables stage by stage.

---

# 7. Rebuilding: Software vs. Hardware

| You changed                                    | Rebuild                                     |
| ---------------------------------------------- | ------------------------------------------- |
| The benchmark (a `git pull` updated it)        | `make -C /workspace/course/benchmarks`      |
| The Rocket configuration                       | `make CONFIG=<Config>` in `sims/verilator`  |
| Nothing (same binary, same configuration)      | nothing; just re-run the simulator          |

Hardware builds are far more expensive than benchmark builds. Avoid unnecessary simulator rebuilds.

---

# Next Steps

Continue with:

* [Performance Counters](/docs/performance-counters.md)
* [Custom Configurations](/docs/custom-configurations.md)
* [Cache Configurations](/docs/cache-configurations.md)
