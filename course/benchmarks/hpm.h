/* ECE 462/562 - RISC-V hardware performance monitor counters.
 *
 * Rocket exposes mhpmcounter3..N / mhpmevent3..N when the core is built with
 * chipyard.config.WithNPerfCounters(N). This header programs eight of them and
 * reads them back, so the lab can *measure* branch mispredictions and cache
 * misses instead of inferring them from cycle differences between configs.
 *
 * Everything fragile about the encoding lives in this one file. If the numbers
 * come back wrong, this is the only place to fix.
 *
 * ---------------------------------------------------------------------------
 * THE ENCODING - verify before trusting the data
 * ---------------------------------------------------------------------------
 * Rocket groups its events into "event sets" and packs a set index plus a mask
 * of events within that set into a single mhpmevent CSR. The layout assumed
 * here is
 *
 *     mhpmevent = (1 << (bit + 8)) | set
 *
 * i.e. the low 8 bits select the set and the rest is a bitmask over that set's
 * events, where `bit` is the event's position in the Seq that defines the set.
 * Confirm both the layout and the per-set event order against your rocket-chip
 * checkout:
 *
 *   grep -n 'new EventSet' -A 30 \
 *     generators/rocket-chip/src/main/scala/rocket/RocketCore.scala
 *   grep -n 'maxEventSetIdBits\|eventSetIdBits\|def decode' \
 *     generators/rocket-chip/src/main/scala/rocket/CSR.scala
 *
 * One event per counter, deliberately. Rocket gates a counter with
 * (mask & hits).orR, so a counter increments at most once per cycle: masking
 * two events into one counter silently undercounts whenever both fire in the
 * same cycle.
 *
 * On a core built without performance counters these CSRs read as zero rather
 * than trapping, so the same binary still runs everywhere - it just reports
 * zeros.
 */

#ifndef ECEX62_HPM_H
#define ECEX62_HPM_H

#include <stdint.h>

#define HPM_MASK_SHIFT 8
#define HPM_EVENT(set, bit) \
    (((uint64_t)1 << ((bit) + HPM_MASK_SHIFT)) | (uint64_t)(set))

#define HPM_N 8

/* slot -> CSR number. csrr/csrw need a compile-time constant CSR name, so the
 * eight counters cannot be walked with a runtime loop variable; this X-macro
 * unrolls them instead. */
#define HPM_FOR_EACH(X) \
    X(0, 3) X(1, 4) X(2, 5) X(3, 6) X(4, 7) X(5, 8) X(6, 9) X(7, 10)

/* Three of the eight are denominators (branch, load, mul) so the interesting
 * counts can be turned into rates rather than left as raw totals.
 *
 * bp_dir_ms and bp_tgt_ms are separate events on purpose: one is the BHT
 * getting the direction wrong, the other is the BTB/RAS getting the target
 * wrong. Splitting them is what lets a single config say which of the three
 * structures in WithBranchPredictor is doing the work.
 *
 * md_stall is the only column that reports *stall cycles* rather than a count
 * of things. Without it a FastMulDivConfig speedup shows up as "cycles fell,
 * every counter flat", because a faster multiplier changes no instruction
 * count, no branch outcome and no memory traffic.
 *
 * Set 0 and set 1 both append their mul/div events after their fixed lists -
 * after jalr (8) and after replay (8) respectively - so both land at index 9,
 * and the FPU events that FpuConfig adds append after those without shifting
 * them. That append order is the one part of this table your BaselineConfig
 * data has not already confirmed; it self-checks, because mul must read ~4096
 * in gain (one per pixel) and ~7812 in downscale (nine per output pixel).
 *
 * Previously here and deliberately dropped:
 *   HPM_EVENT(2, 2)  D$ release - reported writebacks in stages that had
 *                    almost no misses (2540 against 33 in detect), so it does
 *                    not mean "dirty eviction" and could not be trusted.
 *   HPM_EVENT(2, 4)  DTLB miss  - read exactly zero on every stage, which was
 *                    its whole job: catching a bad event encoding. Six exact
 *                    matches against hand-computed counts have since done
 *                    that more convincingly, so the slot was freed.
 * There is no div counter because downscale is the only stage that divides and
 * its count is known statically: one per output pixel, 868 of them. */
static const uint64_t hpm_events[HPM_N] = {
    HPM_EVENT(0, 6),   /* branch instructions   - denominator            */
    HPM_EVENT(0, 1),   /* load instructions     - denominator            */
    HPM_EVENT(0, 9),   /* multiply instructions - denominator            */
    HPM_EVENT(1, 5),   /* branch misprediction              -> BHT       */
    HPM_EVENT(1, 6),   /* control-flow target misprediction -> BTB / RAS */
    HPM_EVENT(1, 9),   /* mul/div interlock - cycles stalled on MulDiv   */
    HPM_EVENT(2, 0),   /* I$ miss - control, stays in the low single digits */
    HPM_EVENT(2, 1)    /* D$ miss                                        */
};

/* These become the column headings of the metrics table printed by
 * metrics.h, and run.sh reads them straight off that header row to label
 * perf.csv. Keep them <= 10 characters (BENCH_COLW) and underscores only, or
 * the table stops lining up and run.sh stops matching them. */
static const char *const hpm_names[HPM_N] = {
    "branch",       /* branch instructions retired                      */
    "load",         /* load instructions retired                        */
    "mul",          /* multiply instructions retired                    */
    "bp_dir_ms",    /* branch misprediction (direction)      -> BHT     */
    "bp_tgt_ms",    /* control-flow target misprediction  -> BTB / RAS  */
    "md_stall",     /* cycles stalled on the multiplier / divider       */
    "ic_miss",      /* I$ miss                                          */
    "dc_miss"       /* D$ miss                                          */
};

#define HPM_WR_ONE(slot, n) \
    asm volatile("csrw mhpmevent" #n ", %0" :: "r"(hpm_events[slot]));

/* Call once, before the first measured region. */
static inline void hpm_setup(void)
{
    HPM_FOR_EACH(HPM_WR_ONE)
}

#define HPM_RD_ONE(slot, n)                                                   \
    do {                                                                      \
        uint64_t _hpm_v;                                                      \
        asm volatile("csrr %0, mhpmcounter" #n : "=r"(_hpm_v));               \
        dst[slot] = _hpm_v;                                                   \
    } while (0);

static inline void hpm_read(uint64_t *dst)
{
    HPM_FOR_EACH(HPM_RD_ONE)
}

#endif /* ECEX62_HPM_H */
