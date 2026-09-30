/* ECE 462/562 - shared cycle/instruction measurement harness.
 *
 * This is the mcycle/minstret recipe from USAGE.md ("Performance Metrics"),
 * extended with the hardware performance counters from hpm.h and packaged so
 * that a run prints one table: one row per measured phase, one column per
 * metric.
 *
 *   phase          cycles    instret     branch       load ...
 *   gain           312044      53248      12291       4096 ...
 *   vfilter       1284410      95232      15873      47616 ...
 *
 * The header row is emitted once, lazily, by the first BENCH_END. Its counter
 * columns are named by hpm.h, and run.sh reads those names off the header
 * rather than hard-coding them - so adding or renaming an event in hpm.h needs
 * no edit either here or in run.sh.
 *
 * CPI is computed on the host, because the htif_nano C library has no
 * floating-point printf (and no config in this lab except FpuConfig has an FPU
 * to run it with).
 *
 * Every counter is sampled before anything is printed, so printf never lands
 * inside a measured quantity. The csrr instructions themselves add a fixed
 * overhead of a few tens of cycles per measurement, so keep each measured
 * region above ~10k cycles if you want that overhead to be in the noise.
 */

#ifndef ECEX62_METRICS_H
#define ECEX62_METRICS_H

#include <stdint.h>
#include <stdio.h>

#include "hpm.h"

/* Width of every numeric column, as a string so it pastes straight into the
 * format literals below. Star widths ("%*s") would need printf's '*' support,
 * which htif_nano's cut-down libc is not worth betting on. The phase column is
 * 10 wide, so a table with HPM_N = 8 counters comes to 118 characters. */
#define BENCH_COLW "10"

static inline uint64_t rd_mcycle(void)
{
    uint64_t x;
    asm volatile("csrr %0, mcycle" : "=r"(x));
    return x;
}

static inline uint64_t rd_minstret(void)
{
    uint64_t x;
    asm volatile("csrr %0, minstret" : "=r"(x));
    return x;
}

static int bench_header_done;

static inline void bench_report(const char *name, uint64_t cycles,
                                uint64_t instret, const uint64_t *h0,
                                const uint64_t *h1)
{
    int k;

    if (!bench_header_done) {
        bench_header_done = 1;
        printf("%-10s %" BENCH_COLW "s %" BENCH_COLW "s",
               "phase", "cycles", "instret");
        for (k = 0; k < HPM_N; k++)
            printf(" %" BENCH_COLW "s", hpm_names[k]);
        printf("\n");
    }

    printf("%-10s %" BENCH_COLW "lu %" BENCH_COLW "lu", name,
           (unsigned long)cycles, (unsigned long)instret);
    for (k = 0; k < HPM_N; k++)
        printf(" %" BENCH_COLW "lu", (unsigned long)(h1[k] - h0[k]));
    printf("\n");
}

/* Usage:
 *     BENCH_BEGIN();
 *     sink = kernel(...);
 *     BENCH_END("kernel-name");
 *
 * Read order matters: mcycle is taken last at BEGIN and first at END, so the
 * counter reads sit outside the cycle window rather than inside it.
 */
#define BENCH_BEGIN()                                                         \
    uint64_t _bench_i0, _bench_c0;                                            \
    uint64_t _bench_h0[HPM_N], _bench_h1[HPM_N];                              \
    do {                                                                      \
        hpm_read(_bench_h0);                                                  \
        _bench_i0 = rd_minstret();                                            \
        _bench_c0 = rd_mcycle();                                              \
    } while (0)

#define BENCH_END(name)                                                       \
    do {                                                                      \
        uint64_t _bench_c1 = rd_mcycle();                                     \
        uint64_t _bench_i1 = rd_minstret();                                   \
        hpm_read(_bench_h1);                                                  \
        bench_report((name), _bench_c1 - _bench_c0,                           \
                     _bench_i1 - _bench_i0, _bench_h0, _bench_h1);            \
    } while (0)

/* Somewhere for a kernel's result to go so -O2 cannot delete the kernel. */
extern volatile uint64_t sink;

#endif /* ECEX62_METRICS_H */
