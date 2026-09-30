# Custom Chipyard Configurations

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

This guide introduces the configuration mechanism used to create different Rocket processor designs in Chipyard.

The goal is to create **new configurations rather than modifying the baseline architecture in place**.

---

# 1. Locate the Existing Rocket Configurations

Chipyard's standard Rocket configurations are located at:

```text
/workspace/chipyard/generators/chipyard/src/main/scala/config/RocketConfigs.scala
```

You can inspect this file to see examples of complete Rocket-based SoC configurations.

The implementation of Rocket itself is located under:

```text
/workspace/chipyard/generators/rocket-chip/src/main/scala/
```

Within that directory, the `rocket/` directory contains processor components such as:

```text
DCache.scala
HellaCache.scala
TLB.scala
RocketCore.scala
```

Do not modify these implementation files unless an assignment specifically instructs you to do so.

---

# 2. Build Another Existing Configuration

Chipyard supports multiple predefined processor configurations.

For example, from:

```bash
cd /workspace/chipyard/sims/verilator
```

you can build:

```bash
make CONFIG=DualRocketConfig
```

This generates a simulator named:

```text
simulator-chipyard.harness-DualRocketConfig
```

Assuming you already created `ecex62.riscv` using the [Running Programs](running-programs.md) tutorial, run:

```bash
./simulator-chipyard.harness-DualRocketConfig ../../tests/build/ecex62.riscv
```

This illustrates an important point:

```text
same program
    +
different processor configuration
    =
architectural experiment
```

---

# 3. Course Baseline Configuration

For this course, the standard baseline configuration is:

```text
CourseRocketConfig
```

Unless an assignment explicitly says otherwise, use this configuration as the starting point for architectural experiments.

Do not modify:

```text
RocketConfig
```

or:

```text
CourseRocketConfig
```

directly.

Instead, create a new configuration representing the architectural change.

For example:

```text
CourseRocketConfig
        |
        +--- CourseCacheConfig
        |
        +--- CourseBranchConfig
        |
        +--- ProjectConfig
```

This allows the original baseline to remain available for direct comparison.

---

# 4. Find Available Configurations

From:

```bash
cd /workspace/chipyard/sims/verilator
```

run:

```bash
make find-configs
```

This lists configuration classes that Chipyard can build.

When you create a new configuration, use this command to verify that Chipyard recognizes it.

---

# 5. Build a Custom Configuration

Suppose you create:

```text
MyCourseConfig
```

Build it with:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=MyCourseConfig
```

Chipyard will generate:

```text
simulator-chipyard.harness-MyCourseConfig
```

Run the same program used for your baseline:

```bash
./simulator-chipyard.harness-MyCourseConfig ../../tests/build/ecex62.riscv
```

Compare against:

```bash
./simulator-chipyard.harness-CourseRocketConfig ../../tests/build/ecex62.riscv
```

---

# 6. Why Create Separate Configurations?

Suppose you are evaluating cache size.

A poor approach would be:

```text
edit baseline
run experiment
edit baseline again
run experiment
edit baseline again
```

After several changes, it becomes difficult to know which architecture generated which result.

Instead, create:

```text
CourseRocketConfig
CourseCache4KBConfig
CourseCache8KBConfig
CourseCache16KBConfig
```

Then every result maps to an explicit architecture.

---

# 7. Configuration Composition

Chipyard uses Scala configuration fragments.

A configuration commonly has the form:

```scala
class MyConfig extends Config(
  new SomeModification ++
  new SomeBaselineConfig
)
```

Conceptually:

```text
SomeBaselineConfig
       +
SomeModification
       =
MyConfig
```

Later tutorials will show concrete examples.

The cache tutorial, for example, creates a new Rocket configuration with explicitly modified D-cache parameters.

---

# 8. Controlled Architectural Experiments

When comparing architectures, change one major factor at a time.

For example:

```text
CourseRocketConfig
        ↓
change number of D-cache sets
        ↓
CourseDCacheConfig
```

Keep constant:

* benchmark;
* input;
* compiler configuration;
* measured region;
* everything else in the processor.

Then measured performance differences can reasonably be attributed to the architectural change.

---

# 9. Course Rule

Unless an assignment explicitly instructs otherwise:

> **Do not edit the default Rocket or Chipyard configurations in place.**

Create a new configuration.

The [cache](/docs/cache-configurations.md) tutorial demonstrates this approach by changing the Rocket data-cache organization.
