# ISA Extension: Adding an Instruction to Rocket

> **If you are not using Docker and installed Chipyard natively on Linux:** skip `docker compose run --rm chipyard` and run `source scripts/native-activate.sh` from your clone instead. In every command in this guide, replace `/workspace` with `$WS`. For example, `cd /workspace/chipyard` becomes `cd $WS/chipyard`. See [Native Installation](/docs/native-install.md), Sections 5 and 6.

The [RoCC tutorial](rocc.md) attaches an accelerator *next to* Rocket. This tutorial goes one level deeper: it adds a new instruction *inside* Rocket's pipeline, so the processor executes it the same way it executes `add`.

The example instruction is `clamp8`, and the flow is:

1. understand how a native instruction differs from a RoCC instruction;
2. apply the example patch to rocket-chip;
3. read the four changes that make up the instruction;
4. build a processor that includes it;
5. call it from C, check it, and measure it; and
6. save and re-apply your own changes.

Before continuing, complete [Building and Running the Course Benchmark](benchmarks.md) and [Performance Counters](performance-counters.md).

---

# 1. Native Instruction vs. RoCC

Rocket is a 5-stage pipeline:

```text
Fetch -> Decode -> Execute -> Memory -> Write-back
```

A **RoCC** instruction is decoded by Rocket but executed by the accelerator:

```text
Decode:      "this is a RoCC instruction"
Write-back:  command handed to the accelerator
             ... accelerator works ...
Later:       result returned to Rocket and written to rd
```

The command leaves the core only after the instruction reaches write-back, and the result comes back later. Even a trivial RoCC operation therefore costs several cycles. That is fine for a large accelerator, but expensive for a small operation.

A **native** instruction is handled entirely by the core:

```text
Decode:      control signals, exactly like add
Execute:     result computed by the ALU (arithmetic/logic unit)
Write-back:  result written to rd
```

It costs the same as any other R-type instruction.

| | RoCC | Native instruction |
| --- | --- | --- |
| Where the work happens | Separate accelerator module | Rocket's own ALU |
| Latency | Several cycles | Same as `add` |
| What you change | A new module plus a config | Rocket's source code |
| Good for | Large or multi-cycle operations | Small combinational operations |

The price of a native instruction is that you must modify the processor itself. There is no configuration switch for adding instructions: Rocket builds its decode table inside the core's source code.

---

# 2. The Example: `clamp8`

`clamp8` saturates a signed value to the range of an unsigned byte:

```text
clamp8 rd, rs1        rd = (rs1 < 0) ? 0 : (rs1 > 255) ? 255 : rs1
```

The course benchmark already needs this. In `isp_bench.c`, `stage_gain()` calls `clamp8()` once per pixel. On `BaselineConfig`, which has no branch prediction, that costs a call, a return, and two branches per pixel.

## Encoding

RISC-V reserves the **custom-0** major opcode (`0001011`) for non-standard instructions. `clamp8` uses the R-type layout inside it, with the unused fields fixed to zero:

```text
 31      25 24   20 19   15 14  12 11    7 6       0
+----------+-------+-------+------+-------+---------+
| 0000000  | 00000 |  rs1  | 000  |  rd   | 0001011 |
+----------+-------+-------+------+-------+---------+
   funct7     rs2             funct3          custom-0
```

For example, `clamp8 a0, a0` is `0x0005050b`.

The assembler does not know the name `clamp8`, but its `.insn` directive can emit any R-type encoding from its fields:

```text
.insn r CUSTOM_0, 0, 0, a0, a0, zero
         opcode  f3 f7  rd  rs1  rs2
```

---

# 3. Apply the Patch

The hardware changes are provided as a patch to rocket-chip:

```text
/workspace/course/patches/clamp8/rocket-chip.patch
```

Move to the rocket-chip source:

```bash
cd /workspace/chipyard/generators/rocket-chip
```

Check that the patch applies, then apply it:

```bash
git apply --check /workspace/course/patches/clamp8/rocket-chip.patch
git apply /workspace/course/patches/clamp8/rocket-chip.patch
```

See which files changed:

```bash
git diff --stat
```

You should see:

```text
 src/main/scala/rocket/ALU.scala                | 10 ++++++++++
 src/main/scala/rocket/CustomInstructions.scala |  2 ++
 src/main/scala/rocket/IDecode.scala            |  6 ++++++
 src/main/scala/rocket/RocketCore.scala         |  3 +++
 4 files changed, 21 insertions(+)
```

Use `git diff` to read the complete change while you work through the next four sections.

> **Important:** Changes inside `/workspace/chipyard` are part of the container and are gone when the container exits. Apply the patch again in every new session. Section 12 shows how to do the same for your own changes.
>
> **If you are not using Docker and installed Chipyard natively on Linux:** `chipyard/` is an ordinary directory in your clone, so the patch stays applied between sessions. Apply it once. Applying it again fails. If `git diff --stat` already shows the four files above, skip the `git apply` step.

---

# 4. Change 1: The Instruction Pattern

File: `src/main/scala/rocket/CustomInstructions.scala`

```scala
def CLAMP8             = BitPat("b000000000000?????000?????0001011")
```

A `BitPat` is the 32-bit encoding from Section 2, written most significant bit first. Each `?` is a "don't care" bit. Here those are the `rs1` and `rd` fields, so any register can be used.

The same file already defines `CUSTOM0`, `CUSTOM0_RD_RS1`, and similar patterns. Those are the patterns RoCC uses when an accelerator is attached.

---

# 5. Change 2: The Decode Row

File: `src/main/scala/rocket/IDecode.scala`

```scala
class Clamp8Decode(implicit val p: Parameters) extends DecodeConstants
{
  val table: Array[(BitPat, List[BitPat])] = Array(
    CLAMP8-> List(Y,N,N,N,N,N,N,Y,A2_ZERO,A1_RS1, IMM_X, DW_XPR,FN_CLAMP8,N,M_X,       N,N,N,N,N,N,Y,CSR.N,N,N,N,N))
}
```

The decoder maps each instruction pattern to a row of control signals. The column names are in the comment above `IntCtrlSigs.default` near the top of the same file. The columns that matter for `clamp8`:

| Column | Value | Meaning |
| --- | --- | --- |
| `val` (legal) | `Y` | A valid instruction, so no illegal-instruction trap |
| `renx2` | `N` | Does not read `rs2` |
| `renx1` | `Y` | Reads `rs1`, so the pipeline forwards or stalls for it |
| `s_alu2` | `A2_ZERO` | ALU input 2 is zero |
| `s_alu1` | `A1_RS1` | ALU input 1 is `rs1` |
| `dw` | `DW_XPR` | Operates on the full 64-bit register |
| `alu` | `FN_CLAMP8` | Which ALU function to perform |
| `wxd` | `Y` | Writes the result to `rd` |

Everything else is `N`: no memory access, multiply/divide, CSR, or floating point.

Just above the new class, `ConditionalZeroDecode` is the decode class of Zicond, a real ratified extension. `clamp8` follows exactly the same shape. The only difference is that Zicond reads two registers (`renx2 = Y`, `A2_RS2`).

---

# 6. Change 3: The ALU Operation

File: `src/main/scala/rocket/ALU.scala`

## A function code

```scala
def FN_BEXT = 19.U
def FN_CLAMP8 = 20.U
```

The ALU function code is 5 bits wide, so there are 32 codes. Codes **20, 21, 22, 23, and 27** are unused.

The code's bits are not arbitrary. The ALU reuses bits of the code: for example, bit 3 turns the shared adder into a subtractor. Code 20 is `10100`. Bit 3 is `0`, and 20 is not one of the compare or shift codes, so no existing ALU logic reacts to it.

## The logic

```scala
val clamp8_out = Mux(io.in1(xLen-1), 0.U, Mux(io.in1(xLen-1,8).orR, 255.U, io.in1(7,0)))
```

Read it from the outside in:

* if the sign bit of `in1` is set, the value is negative, so the result is `0`;
* otherwise, if any bit above bit 7 is set, the value is above 255, so the result is `255`;
* otherwise, the result is the low 8 bits of `in1`.

## Selecting the result

```scala
) else Nil) ++ (if (useClamp8) Seq(
  FN_CLAMP8 -> clamp8_out,
) else Nil))
```

The ALU's output is a lookup from function code to result. The Zbb extension entries directly above are added the same way. `clamp8_out` is added only when the core is configured with `useClamp8`, so a core without it gets no extra hardware.

`useClamp8` is read with:

```scala
val useClamp8 = coreParams match {
  case r: RocketCoreParams => r.useClamp8
  case _ => false
}
```

The match is needed because other cores in Chipyard reuse Rocket's ALU. For them, the parameter does not exist and `clamp8` stays disabled.

---

# 7. Change 4: Hooking It into the Core

File: `src/main/scala/rocket/RocketCore.scala`

A new core parameter, off by default:

```scala
useConditionalZero: Boolean = false,
useClamp8: Boolean = false,
```

The decode row joins the decode table only when the parameter is on:

```scala
(usingConditionalZero.option(new ConditionalZeroDecode)) ++:
(rocketParams.useClamp8.option(new Clamp8Decode)) ++:
```

A safety check:

```scala
require(!(usingRoCC && rocketParams.useClamp8), "Can't select both RoCC and clamp8: RoCC claims the custom-0 opcode that clamp8 uses")
```

When a RoCC accelerator is attached, Rocket decodes every custom-0 to custom-3 instruction as a RoCC instruction. `clamp8` would then match two decode rows, and the result would be undefined. The `require` turns that mistake into a clear error while Chipyard generates the hardware.

Because the parameter defaults to `false`, applying the patch does not change `CourseRocketConfig`, `BaselineConfig`, or `ROCCTest`.

---

# 8. Build a Processor with `clamp8`

The configuration that turns the instruction on is:

```text
/workspace/course/patches/clamp8/Clamp8Config.scala
```

```scala
class WithClamp8 extends RocketCoreConfig(_.copy(useClamp8 = true))

class Clamp8Config extends Config(
  new WithClamp8 ++
  new BaselineConfig)
```

`WithClamp8` sets `useClamp8 = true` in every Rocket core. `Clamp8Config` is `BaselineConfig` plus that one change, so any difference you measure comes from `clamp8`.

This file is not in `course/configs/`. Every file in `course/configs/` is linked into Chipyard when the container starts (natively, when you source `native-activate.sh`), and every build compiles all of them. `Clamp8Config.scala` only compiles after the patch is applied. If it were in `course/configs/`, every build, even `CourseRocketConfig`, would fail for anyone who had not applied the patch.

Link it into Chipyard yourself, after applying the patch:

```bash
ln -s /workspace/course/patches/clamp8/Clamp8Config.scala \
  /workspace/chipyard/generators/chipyard/src/main/scala/config/Clamp8Config.scala
```

> **If you are not using Docker and installed Chipyard natively on Linux:** the link stays between sessions, so create it once. Running `ln -s` again fails with `File exists`, which you can ignore.

Build the simulator:

```bash
cd /workspace/chipyard/sims/verilator
make CONFIG=Clamp8Config
```

If you already built another configuration in this session, expect the Scala compilation to run again: rocket-chip's sources changed, so they must be recompiled. The result is:

```text
simulator-chipyard.harness-Clamp8Config
```

---

# 9. Call `clamp8` from C

The test program is:

```text
/workspace/course/benchmarks/clamp8_test.c
```

The instruction is wrapped in an inline function:

```c
static inline int64_t clamp8_hw(int64_t v)
{
    register int64_t a0 asm("a0") = v;
    asm volatile(".insn r CUSTOM_0, 0, 0, a0, a0, zero" : "=r"(a0) : "r"(a0));
    return a0;
}
```

* `register int64_t a0 asm("a0")` ties the C variable to register `a0`.
* The `.insn` line emits `clamp8 a0, a0`.
* `"=r"(a0)` tells the compiler that the instruction writes `a0`, and `"r"(a0)` that it reads `a0`.

This is the same inline-assembly technique that `metrics.h` uses to read `mcycle`.

The program does two things:

1. **Checks correctness.** It compares `clamp8_hw()` against a software `clamp8_sw()` on edge cases (the most negative and most positive 32- and 64-bit values, -1, 0, 255, 256, and others) and on every value from -1000 to 1000.
2. **Measures.** It runs both versions over the same 20,000 inputs inside `BENCH_BEGIN()` / `BENCH_END()`.

Build it together with the benchmark:

```bash
cd /workspace/course/benchmarks
make
```

This writes:

```text
/workspace/output/clamp8_test.riscv
```

Optionally, look at the machine code:

```bash
make dump
grep 0005050b /workspace/output/clamp8_test.dump
```

The disassembler does not know `clamp8` either, so it prints the instruction as `.insn 4, 0x0005050b`.

---

# 10. Run the Test

```bash
cd /workspace/chipyard/sims/verilator
./simulator-chipyard.harness-Clamp8Config /workspace/output/clamp8_test.riscv
```

You should see:

```text
clamp8_test
clamp8: all checks PASS
```

followed by a measurement table with a `clamp_sw` row and a `clamp_hw` row.

If any check prints `FAIL`, the hardware computes a different result than the software. Do not use the timing results until every check passes.

## The Same Program Without `clamp8`

Now run the same binary on the baseline processor:

```bash
./simulator-chipyard.harness-BaselineConfig /workspace/output/clamp8_test.riscv
```

It never prints `PASS`. The baseline core does not recognize the encoding, so the first `clamp8` traps as an illegal instruction. If the simulator does not exit on its own, stop it with `Ctrl+C`.

This confirms that the result in `Clamp8Config` really comes from the new hardware.

---

# 11. Interpret the Measurement

Compare the `clamp_sw` and `clamp_hw` rows:

* **cycles:** total processor cycles for the 20,000 clamps;
* **instret:** instructions retired;
* **branch:** branch instructions retired. Both loops have one loop branch per iteration, and the software version adds the branches inside `clamp8_sw()`.

Compute the cost per clamp:

$$
\text{cycles per clamp} = \frac{\text{cycles}}{20{,}000}
$$

Then explain the difference from what the software version does for every input: a call, two compare-and-branch instructions, and a return. Each taken branch or jump costs extra cycles on a core without branch prediction. The hardware version replaces all of that with one instruction.

As in the RoCC tutorial, a faster kernel is only part of the story. How much the whole application gains depends on how much of its time that kernel takes.

---

# 12. Designing Your Own Instruction

Use `clamp8` as the template. Every change has the same four parts:

| Part | File | What to decide |
| --- | --- | --- |
| Pattern | `CustomInstructions.scala` | A custom-0 encoding that does not overlap `CLAMP8`, for example a different `funct7` |
| Decode row | `IDecode.scala` | Which registers it reads (`renx1`, `renx2`), the ALU inputs, and whether it writes `rd` |
| ALU logic | `ALU.scala` | An unused function code (21, 22, 23, or 27) and the result logic |
| Hook | `RocketCore.scala` | A parameter, the decode table entry, and a `require` against RoCC |

Plus a configuration fragment like `WithClamp8`.

Some guidelines:

* **Two operands:** set `renx2 = Y` and use `A2_RS2`, as `ConditionalZeroDecode` does.
* **Function code 27** (`11011`) has bit 3 set, so the ALU's adder subtracts. `io.adder_out` then holds `in1 - in2`, which is useful if your operation needs a difference.
* **Keep it combinational.** The result must be computed within the Execute stage, like any other ALU operation. Operations that need many cycles or memory access are a better fit for RoCC.
* **Do not combine with RoCC.** RoCC claims all custom opcodes.

## Saving Your Work

Your rocket-chip changes are lost when the container exits. Before exiting, save them as a patch in your persistent directory:

```bash
cd /workspace/chipyard/generators/rocket-chip
git add -A
git diff --cached > /workspace/student-work/my-extension.patch
```

`git add -A` makes new files part of the diff as well. Without it, `git diff` only shows changes to files that already existed.

> **If you are not using Docker and installed Chipyard natively on Linux:** your rocket-chip changes are not lost between sessions, so you do not need to re-apply the patch next session. Re-applying it fails. Saving a patch is still a useful backup.

In the next session, re-apply it exactly as you applied the `clamp8` patch:

```bash
cd /workspace/chipyard/generators/rocket-chip
git apply --check /workspace/student-work/my-extension.patch
git apply /workspace/student-work/my-extension.patch
```

If you built your instruction on top of `clamp8`, your saved patch already contains the `clamp8` changes. Apply only your patch; applying both fails. To start from a clean rocket-chip instead, remove `clamp8` first:

```bash
git apply -R /workspace/course/patches/clamp8/rocket-chip.patch
```

Keep your configuration file in `/workspace/student-work` too, and link it into Chipyard *after* applying your patch, the same way as `Clamp8Config.scala` in Section 8.

---

# 13. Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| `error: patch failed` / `patch does not apply` | The patch is probably already applied | `git apply -R --check <patch>` succeeds if it is already applied |
| Scala compile error mentioning `useClamp8` | `Clamp8Config.scala` is linked, but the patch is not applied (for example, in a new session) | Apply the patch, or remove the link from `generators/chipyard/src/main/scala/config/` |
| `requirement failed: Can't select both RoCC and clamp8` | `WithClamp8` is combined with a RoCC configuration | Use `clamp8` without RoCC |
| `clamp8: ... FAILED` | The hardware result differs from the software result | Check the ALU logic and the decode row |
| Test never prints `PASS` | The simulator was built without `clamp8` | Run it on `simulator-chipyard.harness-Clamp8Config` |

---

# 14. Key Lesson

An instruction set is not fixed. Adding an instruction means changing three things together: the **encoding** (what software emits), the **decoder** (how the processor recognizes it), and the **datapath** (what it computes).

The design question is the same as for RoCC:

> **Does the new instruction remove enough work from a real bottleneck to justify changing the processor?**
