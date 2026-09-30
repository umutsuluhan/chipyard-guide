/* ECE 462/562 - test for the clamp8 custom instruction.
 *
 *   clamp8 rd, rs1      rd = rs1 < 0 ? 0 : rs1 > 255 ? 255 : rs1
 *
 * encoded in the custom-0 opcode space with funct7 = 0, rs2 = 0, funct3 = 0.
 * The hardware comes from course/patches/clamp8 (see docs/isa-extension.md).
 *
 * The program first checks the instruction against a software clamp, then
 * times both over the same inputs. Run it on Clamp8Config. On a core without
 * clamp8 the first use of the instruction traps as an illegal instruction.
 */

#include <stdint.h>
#include <stdio.h>

#include "metrics.h"

#define N_TIMED  20000u

volatile uint64_t sink;

/* The instruction itself. gcc has no mnemonic for it, so .insn spells out the
 * fields: R-type, opcode custom-0, funct3 0, funct7 0, rd a0, rs1 a0, rs2 x0. */
static inline int64_t clamp8_hw(int64_t v)
{
    register int64_t a0 asm("a0") = v;
    asm volatile(".insn r CUSTOM_0, 0, 0, a0, a0, zero" : "=r"(a0) : "r"(a0));
    return a0;
}

/* The software version. */
static int64_t __attribute__((noinline)) clamp8_sw(int64_t v)
{
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return v;
}

static int check(int64_t v)
{
    int64_t hw = clamp8_hw(v);
    int64_t sw = clamp8_sw(v);

    if (hw != sw) {
        printf("FAIL clamp8(%ld): hw %ld, expected %ld\n",
               (long)v, (long)hw, (long)sw);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const int64_t edges[] = {
        INT64_MIN, INT64_MIN + 1, INT32_MIN, -256, -255, -1, 0, 1,
        127, 128, 254, 255, 256, 257, 511, 512, INT32_MAX, INT64_MAX
    };
    static int64_t inputs[N_TIMED];
    uint32_t i, lfsr = 0xACE1u;
    int64_t v;
    int fails = 0;
    uint64_t acc_sw = 0, acc_hw = 0;

    printf("clamp8_test\n");

    hpm_setup();

    for (i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
        fails += check(edges[i]);
    for (v = -1000; v <= 1000; v++)
        fails += check(v);

    if (fails) {
        printf("clamp8: %d FAILED\n", fails);
        return 1;
    }
    printf("clamp8: all checks PASS\n");

    /* Signed values in [-512, 511]: about half clamp, half pass through. */
    for (i = 0; i < N_TIMED; i++) {
        lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
        inputs[i] = (int64_t)(lfsr & 0x3FFu) - 512;
    }

    {
        BENCH_BEGIN();
        for (i = 0; i < N_TIMED; i++)
            acc_sw += (uint64_t)clamp8_sw(inputs[i]);
        sink = acc_sw;
        BENCH_END("clamp_sw");
    }

    {
        BENCH_BEGIN();
        for (i = 0; i < N_TIMED; i++)
            acc_hw += (uint64_t)clamp8_hw(inputs[i]);
        sink = acc_hw;
        BENCH_END("clamp_hw");
    }

    /* Both timed loops saw the same inputs, so their sums must agree. */
    if (acc_sw != acc_hw) {
        printf("clamp8: timed results differ (sw %lu, hw %lu) FAILED\n",
               (unsigned long)acc_sw, (unsigned long)acc_hw);
        return 1;
    }

    return 0;
}
