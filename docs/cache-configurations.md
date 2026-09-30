# Custom Data Cache Configuration

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

This tutorial demonstrates how to create a Rocket processor configuration with a modified L1 data cache.

You will:

1. locate the existing Rocket cache configuration;
2. identify the default cache parameters;
3. create a new configuration;
4. change the number of sets and ways;
5. build the new processor;
6. verify the generated hardware configuration; and
7. execute a program on the modified processor.

---

# 1. Examine the Rocket Core Configuration

Rocket core configurations are defined in:

```text
/workspace/chipyard/generators/rocket-chip/src/main/scala/rocket/Configs.scala
```

This tutorial uses the Rocket `BigCore` configuration as a convenient example.

Open the file:

```bash
cd /workspace/chipyard
vim generators/rocket-chip/src/main/scala/rocket/Configs.scala
```

or inspect it using another editor.

---

# 2. Locate the Data Cache Parameters

Default cache parameter definitions can also be found in Rocket's cache implementation, including:

```text
generators/rocket-chip/src/main/scala/rocket/HellaCache.scala
```

Important D-cache parameters include:

```text
nSets
nWays
blockBytes
```

These correspond to:

```text
nSets       number of cache sets
nWays       associativity
blockBytes  number of bytes in each cache block
```

For a set-associative cache:

$$
Cache\ Capacity =
nSets \times nWays \times blockBytes.
$$

---

# 3. Baseline D-Cache Description

A default D-cache description may look similar to:

```scala
dcache = Some(DCacheParams(
  rowBits = site(SystemBusKey).beatBits,
  nMSHRs = 0,
  blockBytes = site(CacheBlockBytes))),
```

In the original Rocket `BigCore` configuration, unspecified parameters receive defaults from `DCacheParams`.

For this experiment, we will explicitly specify cache organization parameters.

---

# 4. Create a New Configuration File

Do **not** modify the default Rocket configuration directly.

Instead create:

```bash
cd /workspace/chipyard
touch generators/chipyard/src/main/scala/config/BigCorewithUpdatedDCache.scala
```

Open:

```text
generators/chipyard/src/main/scala/config/BigCorewithUpdatedDCache.scala
```

and add:

```scala
package chipyard

import org.chipsalliance.cde.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.rocket._
import freechips.rocketchip.tile._

class WithNBigCoresDCache(
  n: Int,
  location: HierarchicalLocation,
  crossing: RocketCrossingParams,
) extends Config((site, here, up) => {

  case TilesLocated(`location`) => {

    val prev = up(TilesLocated(`location`), site)
    val idOffset = up(NumTiles)

    val big = RocketTileParams(

      core = RocketCoreParams(
        mulDiv = Some(MulDivParams(
          mulUnroll = 8,
          mulEarlyOut = true,
          divEarlyOut = true
        ))
      ),

      dcache = Some(DCacheParams(
        nSets = 32,
        nWays = 4,
        rowBits = site(SystemBusKey).beatBits,
        nMSHRs = 0,
        blockBytes = site(CacheBlockBytes)
      )),

      icache = Some(ICacheParams(
        rowBits = site(SystemBusKey).beatBits,
        blockBytes = site(CacheBlockBytes)
      ))
    )

    List.tabulate(n)(i =>
      RocketTileAttachParams(
        big.copy(tileId = i + idOffset),
        crossing
      )
    ) ++ prev
  }

  case NumTiles =>
    up(NumTiles) + n

}) {

  def this(
    n: Int,
    location: HierarchicalLocation = InSubsystem
  ) =
    this(
      n,
      location,
      RocketCrossingParams(
        master =
          HierarchicalElementMasterPortParams
            .locationDefault(location),

        slave =
          HierarchicalElementSlavePortParams
            .locationDefault(location),

        mmioBaseAddressPrefixWhere =
          location match {
            case InSubsystem =>
              CBUS

            case InCluster(clusterId) =>
              CCBUS(clusterId)
          }
      )
    )
}
```

Then append the SoC configuration:

```scala
class BigCorewithDCacheUpdateConfig extends Config(
  new WithNBigCoresDCache(1) ++
  new chipyard.config.AbstractConfig
)
```

---

# 5. What Did We Change?

The important portion is:

```scala
dcache = Some(DCacheParams(
  nSets = 32,
  nWays = 4,
  rowBits = site(SystemBusKey).beatBits,
  nMSHRs = 0,
  blockBytes = site(CacheBlockBytes)
)),
```

We explicitly set:

```text
nSets = 32
nWays = 4
```

Assuming:

```text
blockBytes = 64
```

cache capacity is:

$$ 32 \times 4 \times 64 = 8192\ bytes $$

Therefore:

$$
Cache\ Size = 8\ KiB.
$$

---

# 6. Build the New Processor

Move to:

```bash
cd /workspace/chipyard/sims/verilator
```

Build:

```bash
make CONFIG=BigCorewithDCacheUpdateConfig
```

This generates a new simulator:

```text
simulator-chipyard.harness-BigCorewithDCacheUpdateConfig
```

The baseline Rocket simulator remains unchanged.

---

# 7. Verify the Generated Cache Configuration

Do not simply assume that the hardware contains the cache configuration you requested.

Chipyard generates a JSON description of the resulting system.

Inspect:

```text
generated-src/chipyard.harness.TestHarness.BigCorewithDCacheUpdateConfig/chipyard.harness.TestHarness.BigCorewithDCacheUpdateConfig.json
```

For example:

```bash
grep -o -E '"d-cache-(block-size|sets|size)":\[[0-9]+\]' generated-src/chipyard.harness.TestHarness.BigCorewithDCacheUpdateConfig/chipyard.harness.TestHarness.BigCorewithDCacheUpdateConfig.json
```

You should see values corresponding to approximately:

```text
"d-cache-block-size":[64]
"d-cache-sets":[32]
"d-cache-size":[8192]
```

The number of ways can then be verified using:

$$
nWays =
\frac{Cache\ Size}
{Number\ of\ Sets \times Block\ Size}.
$$

Therefore: 

$$ nWays = \frac{8192}{32\times64} = 4 $$

This is an important habit:

> **Verify the generated architecture rather than assuming that a configuration change had the intended effect.**

---

# 8. Run a Program on the Modified Core

If you created `ecex62.riscv` in the [Running Programs](/docs/running-programs.md) tutorial:

```bash
./simulator-chipyard.harness-BigCorewithDCacheUpdateConfig ../../tests/build/ecex62.riscv
```

The program should execute normally.

You have now run the same software on a processor with a different cache architecture.

---

# 9. Compare Against the Baseline

Run the same application on:

```text
CourseRocketConfig
```

and:

```text
BigCorewithDCacheUpdateConfig
```

For example:

```bash
./simulator-chipyard.harness-CourseRocketConfig ../../tests/build/ecex62.riscv
```

followed by:

```bash
./simulator-chipyard.harness-BigCorewithDCacheUpdateConfig ../../tests/build/ecex62.riscv
```

For meaningful cache-performance experiments, use a memory-intensive benchmark rather than the tiny `ecex62` example.

The example program exists only to verify that the custom architecture works.

---

# 10. Experiment: Change the Number of Sets

You can now create variants such as:

```text
nSets = 16
nSets = 32
nSets = 64
nSets = 128
```

while holding:

```text
nWays
blockBytes
```

constant.

For every configuration:

1. calculate cache capacity;
2. predict the expected effect;
3. build the processor;
4. verify the generated JSON;
5. run the same benchmark;
6. measure cycles and instructions;
7. compare performance.

---

# 11. Experiment: Change Associativity

Similarly, hold cache capacity approximately constant while changing:

```text
nWays = 1
nWays = 2
nWays = 4
nWays = 8
```

You may need to change `nSets` simultaneously to preserve total capacity.

For example:

| Sets | Ways | Block Size | Capacity |
| ---: | ---: | ---------: | -------: |
|  128 |    1 |       64 B |    8 KiB |
|   64 |    2 |       64 B |    8 KiB |
|   32 |    4 |       64 B |    8 KiB |
|   16 |    8 |       64 B |    8 KiB |

This provides a controlled experiment on associativity because overall cache capacity remains fixed.

---

# 12. Experiment: Cache Capacity

Alternatively, hold associativity and line size fixed and vary capacity.

For example:

| Sets | Ways | Block Size | Capacity |
| ---: | ---: | ---------: | -------: |
|   16 |    4 |       64 B |    4 KiB |
|   32 |    4 |       64 B |    8 KiB |
|   64 |    4 |       64 B |   16 KiB |
|  128 |    4 |       64 B |   32 KiB |

Before simulation, predict where diminishing returns should occur for the assigned workload.

---

# 13. What to Report

For each configuration, record at least:

| Configuration | Sets | Ways | Block Size | Capacity | Cycles | Instructions | CPI |
| ------------- | ---: | ---: | ---------: | -------: | -----: | -----------: | --: |
| Baseline      |      |      |            |          |        |              |     |
| Config A      |      |      |            |          |        |              |     |
| Config B      |      |      |            |          |        |              |     |

Calculate:

$$
CPI =
\frac{Cycles}{Instructions}
$$

and:

$$
Speedup =
\frac{Cycles_{baseline}}
{Cycles_{modified}}
$$

when clock frequency is assumed unchanged.

Then explain the architectural reason for the result.

---

# 14. Key Lesson

The objective of the exercise is not simply to learn how to edit:

```scala
nSets
```

or:

```scala
nWays
```

The architectural question is:

> **How does the application's memory behavior interact with cache organization, and which cache design best serves this workload?**

The configuration mechanism gives us a way to experimentally answer that question.
