# ECE 462/562 - lab benchmarks
#
# Flags mirror chipyard/tests/CMakeLists.txt so these binaries behave exactly
# like the tests you already built there (same toolchain, same htif linker
# script, same libc).
#
# Build inside the chipyard container, after `source env.sh`:
#     make -C lab/bench
#
# Produces isp_bench.riscv plus a .dump disassembly (useful for checking that
# gcc really emitted the branch / mul / div you were trying to measure).

RISCV_PREFIX ?= riscv64-unknown-elf-
CC      := $(RISCV_PREFIX)gcc
OBJDUMP := $(RISCV_PREFIX)objdump

ARCH   ?= rv64imafd
ABI    ?= lp64d
CMODEL ?= medany
SPECS  ?= htif_nano.specs
LDSCRIPT ?= htif.ld

WS     ?= /workspace
OUTDIR ?= $(WS)/output

# -specs appears once only: we compile and link in a single gcc invocation, and
# reading htif_nano.specs twice makes gcc fail with
#   attempt to rename spec 'link' to already defined spec 'nano_link'
CFLAGS  := -std=gnu99 -O2 -Wall -Wextra -fno-common -fno-builtin-printf \
           -march=$(ARCH) -mabi=$(ABI) -mcmodel=$(CMODEL) -specs=$(SPECS)
LDFLAGS := -static -T $(LDSCRIPT)

BENCHES := isp_bench clamp8_test

ELFS  := $(addprefix $(OUTDIR)/,$(addsuffix .riscv,$(BENCHES)))
DUMPS := $(addprefix $(OUTDIR)/,$(addsuffix .dump,$(BENCHES)))

.PHONY: all dump clean
all: $(ELFS)

dump: $(DUMPS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/%.riscv: %.c metrics.h | $(OUTDIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(OUTDIR)/%.dump: $(OUTDIR)/%.riscv
	$(OBJDUMP) -D $< > $@

clean:
	rm -f $(ELFS) $(DUMPS)
