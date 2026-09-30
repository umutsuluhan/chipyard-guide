# Native Installation (No Docker)

This guide is for students who **cannot run the course Docker image**, typically
because their computer does not have enough RAM and they have to work on a
university server where the Docker daemon is not available.

It installs the same Chipyard environment the Docker image contains, built from
source in your own home directory. **No administrator rights are required.**

> If you can run Docker, use the [Docker path](/README.md) instead. It is one
> download rather than a multi-hour build.

---

# 1. Before You Start

You need:

| Requirement | Notes |
| ----------- | ----- |
| A Linux account on the server | No `sudo` needed. |
| `git` and `curl` (or `wget`) | Usually already installed, or available as a module. |
| **45 GB of free disk space** | The Chipyard tree plus its conda environments are 35-40 GB. Check your quota first. |
| **1-3 hours** | Mostly unattended. The script can be interrupted and resumed. |
| Outbound HTTPS | The build downloads conda packages and clones Chipyard. |

Each student installs their own copy. There is no shared installation.

If your home directory has a small quota, ask whether the server has local
scratch space and clone the repository there instead.

---

# 2. Install

Clone the course repository, then run the installer from it:

```bash
git clone https://github.com/tosiron/ua-chipyard.git
cd ua-chipyard
bash scripts/native-setup.sh
```

It installs Miniforge into `$HOME/miniforge3`, clones Chipyard 1.14.0 into
`chipyard/`, builds it (**this is the long step**), sets up `output/` and the
course configurations, and finishes by checking the installation. You are done
when it prints `Environment: PASS`.

It does **not** run `conda init` or modify your `~/.bashrc`, so any other conda
you use is left alone.

## If sbt cannot reach Maven Central

On a network that blocks or proxies Maven Central, re-run with:

```bash
bash scripts/native-setup.sh --pin-sbt-repos
```

This writes `~/.sbt/repositories` and prints an `export SBT_OPTS=...` line to
add to your `~/.bashrc`. Use it only if sbt actually fails to resolve
dependencies - pointing sbt at an unreachable mirror causes its own confusing
failures.

## The build was interrupted - now what?

Just run `bash scripts/native-setup.sh` again.

The installer records each finished phase in `.native-setup-state/` and skips it
on a re-run, so an interrupted install resumes rather than restarting.

If the Chipyard build itself was interrupted, the installer stops and prints
your two options - start that build over, or resume it from where it stopped.
Run the one you want, then re-run the installer to finish.

---

# 3. Every Session

Docker students run `docker compose run --rm chipyard`. You run:

```bash
cd ~/ua-chipyard
source scripts/native-activate.sh
source env.sh
```

`source`, not `bash` - the settings have to stay in your shell.

`native-activate.sh` installs any new course configurations (so `git pull` alone
is enough when a new one is released), puts the course scripts on your `PATH` so
bare `quiz-run` works, activates conda, and drops you in `chipyard/`.

To do it automatically at every login, add one line to your `~/.bashrc`:

```bash
source ~/ua-chipyard/scripts/native-activate.sh
```

`source env.sh` stays a separate, manual step.

---

# 4. Sharing the Server

The server is shared with the rest of the class. Two rules:

**Build with `make -j4`, never `make -j$(nproc)`.** On a machine with 64 cores,
`-j$(nproc)` from several students at once makes the machine unusable for
everyone, including you.

```bash
cd $WS/chipyard/sims/verilator
make -j4 CONFIG=BaselineConfig
```

**The JVM heap is already reduced for you.** `native-activate.sh` sets
`JAVA_HEAP_SIZE=4G`, instead of Chipyard's default of 8G. If a very large
elaboration genuinely runs out of memory, raise it for that one command:

```bash
JAVA_HEAP_SIZE=6G make -j4 CONFIG=SomeBigConfig
```

---

# 5. Your Work Persists

`chipyard/` and `output/` are ordinary directories in your clone, so they are
still there next session. The container throws them away, which is why a guide
such as [ISA Extension](/docs/isa-extension.md) tells you to reapply the
`clamp8` patch every session. Natively:

- **Do not** reapply a patch that is already applied - you would apply it twice
  and get a failed or doubled patch. Check first with:

  ```bash
  cd $WS/chipyard/generators/rocket-chip
  git diff --stat
  ```

  If it already shows the expected changes, the patch is applied. Skip that step
  in the guide.

- You do not need to rebuild simulators or benchmarks you already built.

- To get back to a clean Rocket, reset it yourself:

  ```bash
  cd $WS/chipyard/generators/rocket-chip
  git checkout .
  ```

Everything else in the course documents applies unchanged.

---

# 6. Paths in the Other Course Documents

The other course documents use absolute `/workspace/...` paths. Natively,
`/workspace` is your clone: after `source scripts/native-activate.sh`, `$WS`
holds it, so in every command, replace `/workspace` with `$WS`. For example,
`cd /workspace/chipyard` becomes `cd $WS/chipyard`.

Two are not a straight substitution:

| In the course documents | Natively |
| ----------------------- | -------- |
| `docker compose run --rm chipyard` | `source scripts/native-activate.sh` |
| `/workspace/test-install.sh` | `$WS/scripts/test-install.sh` |

`$WS` is exported, so it also works in `make`:

```bash
make -C $WS/course/benchmarks     # writes into $WS/output
```

---

# 7. Troubleshooting

**`quiz-run: command not found`** - you did not source `native-activate.sh` in
this shell. Run `source scripts/native-activate.sh`.

**`riscv64-unknown-elf-gcc: command not found`** - you did not run
`source env.sh` after activating. Both steps are needed, in that order.

**The installer sits for a long time before the build starts** - if this
machine's glibc (`ldd --version`) is older than 2.34, Chipyard regenerates its
conda lockfiles, which means a full dependency solve. It is slow, not stuck.

**`No space left on device` / quota exceeded** - free space, then re-run
`bash scripts/native-setup.sh`. It resumes from where it stopped.

**Something else** - see [Troubleshooting](/docs/troubleshooting.md). In any
command it gives, replace `/workspace` with `$WS`.
