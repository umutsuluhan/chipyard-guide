/* ECE 462/562 - grayscale motion-detection camera pipeline.
 *
 * This embedded inspired benchmark simulates the image path of a camera 
 * microcontroller looking for motion and each of its five stages is
 * deliberately sensitive to various microarchitectural changes:
 *
 *   gain       one integer multiply per pixel, saturated through an
 *              out-of-line clamp        -> MulDiv (mul), and the return
 *                                          address stack
 *   vfilter    vertical 3-tap blur walked COLUMN-major; shifts only, no
 *              multiply                 -> L1 capacity
 *   downscale  3x3 weighted box downscale to a thumbnail, normalised by a
 *              kernel sum computed at run time
 *                                       -> MulDiv (mul and div)
 *   bgmodel    temporal background over an 8-frame ring, sampled on a coarse
 *              grid of one point per cache line
 *                                       -> L2
 *   detect     threshold against that background, plus a 256-bin histogram
 *                                       -> branch prediction
 *
 * Sizing is chosen against the cache geometries in MicroarchConfigs.scala:
 *
 *   frame           4 KiB   fills the baseline 4 KiB direct-mapped L1 exactly,
 *                           so frame + tmp (8 KiB) thrash it and fit the
 *                           16 KiB 4-way L1 of LargeL1Config
 *   ring           32 KiB   misses both L1s, fits the 512 KiB LLC of
 *                           L2CacheConfig
 *   thumbnail       1 KiB   fits every L1, so `detect` is cache-flat and its
 *                           cycle delta is misprediction cost
 *
 *
 * Every config must print the same CHECKSUM line. Microarchitecture changes
 * how long the answer takes, never what the answer is.
 *
 * Each stage also reports the eight hardware performance counters from hpm.h
 * (branch mispredictions, cache misses and their denominators), so the claims
 * above can be checked directly rather than inferred from cycle counts.
 */

#include <stdint.h>
#include <stdio.h>

#include "metrics.h"

#define IMG_W        128u
#define IMG_H         32u
#define IMG_PIXELS   (IMG_W * IMG_H)          /* 4 KiB */

#define NFRAMES        8u                     /* ring: 32 KiB, power of two */

#define THUMB_W      (IMG_W / 2u)
#define THUMB_H      (IMG_H / 2u)
#define THUMB_PIXELS (THUMB_W * THUMB_H)      /* 1 KiB */

#define GRID_STRIDE   64u                     /* one sample per cache line */
#define GRID_PTS     (IMG_PIXELS / GRID_STRIDE)

#define VFILT_REPS     2u
#define DETECT_REPS   32u

volatile uint64_t sink;

/* Opaque to the compiler, so it cannot fold the multiply into a shift, turn
 * the divide into a reciprocal multiply, or hoist the threshold. */
static volatile int32_t opaque_gain   = 300;
static volatile int32_t opaque_offset = 12;
static volatile int32_t opaque_thresh = 120;
static volatile int32_t opaque_seed   = 3;

static uint8_t  ring[NFRAMES][IMG_PIXELS];
static uint8_t  frame[IMG_PIXELS];
static uint8_t  tmp[IMG_PIXELS];
static uint8_t  thumb[THUMB_PIXELS];
static uint8_t  bglevel[NFRAMES][GRID_PTS];
static uint32_t hist[256];

static int32_t  kern[9];                      /* filled at run time */
static int32_t  ksum;

static uint32_t fg_count;

/* ---------------------------------------------------------------- setup --
 * Untimed, and deliberately multiply-free: at mulUnroll = 1 a multiply costs
 * ~64 cycles, so a multiply in the frame generator would cost more than the
 * whole measured pipeline.
 */

static void gen_ring(void)
{
    uint32_t lfsr = 0xACE1u;
    uint32_t f, x, y;

    for (f = 0; f < NFRAMES; f++) {
        /* A bright blob drifting across the sensor - this is the "motion". */
        int32_t bx = 24 + (int32_t)(f << 3);
        int32_t by = 8 + (int32_t)f;

        for (y = 0; y < IMG_H; y++) {
            for (x = 0; x < IMG_W; x++) {
                int32_t dx, dy;
                uint32_t v;

                /* 16-bit Galois LFSR: shifts and xors only. */
                lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);

                v = (x + y) & 0xFFu;           /* diagonal illumination ramp */

                dx = (int32_t)x - bx;
                dy = (int32_t)y - by;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx + dy < 10)
                    v += 90u;

                v += lfsr & 7u;                /* sensor noise */
                if (v > 255u)
                    v = 255u;

                ring[f][y * IMG_W + x] = (uint8_t)v;
            }
        }
    }
}

static void init_kernel(void)
{
    int32_t s = opaque_seed;
    int i;

    /* A 3x3 tent kernel, but built from a volatile so gcc sees nine unknown
     * multiplicands and one unknown divisor. */
    static const int32_t shape[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };

    ksum = 0;
    for (i = 0; i < 9; i++) {
        kern[i] = shape[i] + s;
        ksum += kern[i];
    }
}

/* ------------------------------------------------------------- stage 1 --
 * Sensor gain and black-level offset. One multiply per pixel; the clamp is
 * out of line, so this is also a call/return per pixel.
 */

static uint8_t __attribute__((noinline)) clamp8(int32_t v)
{
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return (uint8_t)v;
}

static void stage_gain(int32_t g, int32_t off)
{
    uint32_t i;

    for (i = 0; i < IMG_PIXELS; i++) {
        int32_t v = ((int32_t)frame[i] * g) >> 8;
        frame[i] = clamp8(v - off);
    }
}

/* ------------------------------------------------------------- stage 2 --
 * Vertical 3-tap blur, walked one column at a time. No multiply: (a + 2b + c)
 * is a shift and two adds, so what this stage measures is purely where the
 * pixels live. Consecutive rows of a column are IMG_W bytes apart, so every
 * step down a column touches a different 64-byte line.
 */

static void stage_vfilter(void)
{
    uint32_t r, x, y;

    for (r = 0; r < VFILT_REPS; r++) {
        for (x = 0; x < IMG_W; x++) {
            for (y = 1; y < IMG_H - 1u; y++) {
                uint32_t a = frame[(y - 1u) * IMG_W + x];
                uint32_t b = frame[y * IMG_W + x];
                uint32_t c = frame[(y + 1u) * IMG_W + x];

                tmp[y * IMG_W + x] = (uint8_t)((a + (b << 1) + c) >> 2);
            }
        }
    }
}

/* ------------------------------------------------------------- stage 3 --
 * 3x3 weighted downscale by two, into the thumbnail the detector works on.
 * Nine multiplies and one divide per output pixel, all with operands the
 * compiler cannot see. The border is left at zero.
 */

static void stage_downscale(void)
{
    uint32_t tx, ty;

    for (ty = 1; ty < THUMB_H - 1u; ty++) {
        for (tx = 1; tx < THUMB_W - 1u; tx++) {
            uint32_t sx = tx << 1;
            uint32_t sy = ty << 1;
            int32_t acc = 0;
            uint32_t j, i;

            for (j = 0; j < 3u; j++) {
                const uint8_t *row = &tmp[(sy - 1u + j) * IMG_W + (sx - 1u)];
                for (i = 0; i < 3u; i++)
                    acc += (int32_t)row[i] * kern[j * 3u + i];
            }

            thumb[ty * THUMB_W + tx] = (uint8_t)(acc / ksum);
        }
    }
}

/* ------------------------------------------------------------- stage 4 --
 * Temporal background model over the whole 8-frame ring, sampled on a coarse
 * grid - one point per cache line, which is how a real ISP computes exposure
 * and background statistics without touching every pixel. The ring is walked
 * once per time step, so the same 32 KiB is re-read NFRAMES times: that reuse
 * is what an LLC can hold and the baseline cannot.
 */

static uint64_t stage_bgmodel(void)
{
    uint64_t acc = 0;
    uint32_t t, g, f;

    for (t = 0; t < NFRAMES; t++) {
        for (g = 0; g < GRID_PTS; g++) {
            uint32_t idx = g * GRID_STRIDE;
            uint32_t sum = 0;
            int32_t d;

            for (f = 0; f < NFRAMES; f++)
                sum += ring[(t + f) & (NFRAMES - 1u)][idx];

            /* How far this frame sits from the temporal mean. */
            d = (int32_t)ring[t][idx] - (int32_t)(sum >> 3);
            if (d < 0)
                d = -d;

            bglevel[t][g] = (uint8_t)d;
            acc += (uint64_t)d;
        }
    }

    return acc;
}

/* ------------------------------------------------------------- stage 5 --
 * Motion detection: threshold the thumbnail and histogram it. The thumbnail
 * fits in every L1 in the ladder, so this stage's cycle count moves only when
 * the front end gets better at guessing the data-dependent branch below.
 *
 * The empty asm statements stop gcc if-converting that branch into branchless
 * arithmetic, which would delete what we are trying to measure. Check
 * isp_bench.dump if you suspect it has done so anyway.
 */

static uint64_t stage_detect(uint32_t thr)
{
    uint64_t acc = 0;
    uint32_t fg = 0;
    uint32_t r, i;

    for (r = 0; r < DETECT_REPS; r++) {
        for (i = 0; i < THUMB_PIXELS; i++) {
            uint32_t p = thumb[i];

            hist[p]++;

            if (p > thr) {
                asm volatile("" : "+r"(fg));
                fg++;
            } else {
                asm volatile("" : "+r"(acc));
                acc ^= (uint64_t)i;
            }
        }
    }

    fg_count = fg;
    return acc + (uint64_t)fg;
}

/* ------------------------------------------------------------------------ */

static uint64_t checksum(void)
{
    uint64_t ck = fg_count;
    uint32_t i;

    for (i = 0; i < 256u; i++)
        ck = (ck << 5) - ck + (uint64_t)hist[i];

    for (i = 0; i < GRID_PTS; i++)
        ck = (ck << 5) - ck + (uint64_t)bglevel[NFRAMES - 1u][i];

    return ck;
}

int main(void)
{
    uint32_t i;
    uint32_t thr;

    printf("isp_bench\n");

    hpm_setup();

    gen_ring();
    init_kernel();

    /* The frame the pipeline is about to process is the newest one. */
    for (i = 0; i < IMG_PIXELS; i++)
        frame[i] = ring[NFRAMES - 1u][i];

    {
        BENCH_BEGIN();
        stage_gain(opaque_gain, opaque_offset);
        BENCH_END("gain");
    }

    {
        BENCH_BEGIN();
        stage_vfilter();
        BENCH_END("vfilter");
    }

    {
        BENCH_BEGIN();
        stage_downscale();
        BENCH_END("downscale");
    }

    {
        BENCH_BEGIN();
        sink = stage_bgmodel();
        BENCH_END("bgmodel");
    }

    thr = (uint32_t)opaque_thresh + (bglevel[NFRAMES - 1u][0] >> 2);

    {
        BENCH_BEGIN();
        sink = stage_detect(thr);
        BENCH_END("detect");
    }

    printf("CHECKSUM,%lu\n", (unsigned long)checksum());

    return 0;
}
