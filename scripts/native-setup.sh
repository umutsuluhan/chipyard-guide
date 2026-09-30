#!/usr/bin/env bash
#
# ECE 462/562 - native (no-Docker) Chipyard installer.
#
# One-time setup for students who cannot run the course Docker image (no
# container runtime, no sudo). Builds the same environment the image builds,
# with the same build-setup.sh flags, into the student's own clone of this
# repository.
#
# Usage:
#     bash scripts/native-setup.sh [--pin-sbt-repos]
#
# The build takes 1-3 hours. The script is resumable: each phase writes a
# stamp file under $WS/.native-setup-state/ and is skipped on a re-run.

set -eo pipefail

PIN_SBT_REPOS=0

usage() {
    cat <<'EOF'
Usage: bash scripts/native-setup.sh [options]

  --pin-sbt-repos   Pin sbt to the Google Maven Central mirror, as the course
                    Docker image does. Default off: on a normal university
                    network the default sbt resolvers work, and a mirror that
                    is not reachable breaks dependency resolution silently.
  -h, --help        Show this message.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --pin-sbt-repos)  PIN_SBT_REPOS=1 ;;
        -h|--help)        usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

die() {
    echo >&2
    echo "ERROR: $*" >&2
    echo >&2
    exit 1
}

step() {
    echo
    echo "======================================"
    echo "$*"
    echo "======================================"
    echo
}

# WS is the workspace root: the clone this script lives in. It replaces the
# container's /workspace. Derived from the script's own location so it cannot
# be wrong, but an explicit WS in the environment wins.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS="${WS:-$(cd "$SCRIPT_DIR/.." && pwd)}"
export WS

if command -v curl >/dev/null; then
    DOWNLOADER=curl
elif command -v wget >/dev/null; then
    DOWNLOADER=wget
else
    die "Neither curl nor wget was found. One of them is required."
fi

STATE="$WS/.native-setup-state"
mkdir -p "$STATE"

stamped()  { [ -f "$STATE/$1" ]; }
stamp()    { : > "$STATE/$1"; }

CONDA_DIR="$HOME/miniforge3"
CHIPYARD="$WS/chipyard"

# ---------------------------------------------------------------------------
# 1. Miniforge
# ---------------------------------------------------------------------------

step "[1/6] Miniforge"

if stamped miniforge.done; then
    echo "Already installed at $CONDA_DIR; skipping."
else
    if [ -e "$CONDA_DIR" ]; then
        die "$CONDA_DIR already exists but this script did not finish
       installing it. Remove or rename it and re-run."
    fi

    INSTALLER="$STATE/Miniforge3-$(uname)-$(uname -m).sh"
    URL="https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-$(uname)-$(uname -m).sh"

    echo "Downloading $URL"
    if [ "$DOWNLOADER" = curl ]; then
        curl -fsSL -o "$INSTALLER" "$URL"
    else
        wget -q -O "$INSTALLER" "$URL"
    fi

    # -b -p: batch, into our own prefix. We deliberately do not run
    # `conda init` and do not touch ~/.bashrc, so any other conda on this
    # machine, and any conda work of your own, is left alone. Activation is
    # explicit, in scripts/native-activate.sh.
    echo "Installing to $CONDA_DIR"
    bash "$INSTALLER" -b -p "$CONDA_DIR"
    rm -f "$INSTALLER"

    stamp miniforge.done
    echo "Miniforge installed."
fi

# ---------------------------------------------------------------------------
# 2. Clone chipyard
# ---------------------------------------------------------------------------

step "[2/6] Chipyard 1.14.0"

if stamped clone.done; then
    echo "Already cloned at $CHIPYARD; skipping."
else
    if [ -e "$CHIPYARD" ]; then
        die "$CHIPYARD already exists but was not cloned by this script.
       Remove or rename it and re-run."
    fi
    git clone --branch 1.14.0 https://github.com/ucb-bar/chipyard.git "$CHIPYARD"
    stamp clone.done
fi

# chipyard's build-setup.sh does CYDIR=$(git rev-parse --show-toplevel). If
# chipyard/ were an unpacked tarball rather than a real clone, that would
# resolve to the course repository root and the build would install into the
# wrong tree.
[ -d "$CHIPYARD/.git" ] || die "$CHIPYARD is not a git clone (no .git directory).
       chipyard must be cloned, not unpacked: build-setup.sh resolves its own
       root with 'git rev-parse --show-toplevel'. Remove $CHIPYARD and re-run."

# ---------------------------------------------------------------------------
# 3. Optional sbt mirror pinning
# ---------------------------------------------------------------------------

step "[3/6] sbt repositories"

SBT_REPOS="$HOME/.sbt/repositories"

if [ "$PIN_SBT_REPOS" -eq 0 ]; then
    echo "Not pinning sbt repositories (default)."
    echo "Re-run with --pin-sbt-repos if sbt cannot reach Maven Central directly."
elif stamped sbtrepos.done; then
    echo "Already written to $SBT_REPOS; skipping."
else
    if [ -f "$SBT_REPOS" ]; then
        die "$SBT_REPOS already exists. Leaving your sbt configuration
       alone. Merge the repositories block by hand, or re-run without
       --pin-sbt-repos."
    fi

    mkdir -p "$HOME/.sbt"
    printf '[repositories]\n  local\n  google-maven-central: https://maven-central.storage-download.googleapis.com/maven2/\n  maven-central\n  sbt-plugin-releases: https://repo.scala-sbt.org/scalasbt/sbt-plugin-releases/, [organization]/[module]/(scala_[scalaVersion]/)(sbt_[sbtVersion]/)[revision]/[type]s/[artifact](-[classifier]).[ext]\n  typesafe-ivy-releases: https://repo.typesafe.com/typesafe/ivy-releases/, [organization]/[module]/(scala_[scalaVersion]/)(sbt_[sbtVersion]/)[revision]/[type]s/[artifact](-[classifier]).[ext]\n' \
        > "$SBT_REPOS"

    stamp sbtrepos.done
    echo "Wrote $SBT_REPOS"
    echo "Add this to your shell (native-activate.sh does not set it):"
    echo "    export SBT_OPTS=\"-Dsbt.override.build.repos=true -Dsbt.repository.config=$SBT_REPOS\""
fi

if [ "$PIN_SBT_REPOS" -eq 1 ]; then
    export SBT_OPTS="-Dsbt.override.build.repos=true -Dsbt.repository.config=$SBT_REPOS"
fi

# ---------------------------------------------------------------------------
# 4. build-setup.sh - the long one
# ---------------------------------------------------------------------------

step "[4/6] Chipyard build-setup (1-3 hours)"

if stamped buildsetup.done; then
    echo "Already built; skipping."
else
    # build-setup.sh refuses to start if .conda-env or .conda-lock-env exists.
    # If we get here with either present, a previous run was interrupted
    # part-way. That is 30 GB of possibly-good work, so decide yourself; this
    # script will not delete it.
    if [ -e "$CHIPYARD/.conda-env" ] || [ -e "$CHIPYARD/.conda-lock-env" ]; then
        cat >&2 <<EOF

======================================
Interrupted build detected
======================================

$CHIPYARD already has a .conda-env and/or .conda-lock-env directory, but the
build never finished. chipyard's build-setup.sh refuses to start when those
exist, and this script will not delete tens of gigabytes on your behalf.

Choose one, then re-run this script:

  (a) Start the Chipyard build over:
        rm -rf "$CHIPYARD/.conda-env" "$CHIPYARD/.conda-lock-env"
        bash "$WS/scripts/native-setup.sh"

  (b) Resume the existing build from its first stage:
        source "$CONDA_DIR/etc/profile.d/conda.sh"
        conda activate base
        cd "$CHIPYARD"
        ./build-setup.sh riscv-tools --skip-marshal --skip-firesim -s 1
      then re-run this script to finish the remaining steps:
        touch "$STATE/buildsetup.done"
        bash "$WS/scripts/native-setup.sh"

EOF
        exit 1
    fi

    # build-setup.sh calls `conda activate`, so the conda shell functions have
    # to be live in this shell first. `set +u` because conda.sh trips over
    # unset variables in some releases.
    set +u
    # shellcheck disable=SC1091
    source "$CONDA_DIR/etc/profile.d/conda.sh"
    conda activate base

    # Exactly the flags the Dockerfile uses, so native and Docker students get
    # identical environments.
    cd "$CHIPYARD"
    ./build-setup.sh riscv-tools --skip-marshal --skip-firesim

    stamp buildsetup.done
    echo "Chipyard build complete."
fi

# ---------------------------------------------------------------------------
# 5. Wire up the course layout
# ---------------------------------------------------------------------------

step "[5/6] Course layout"

mkdir -p "$WS/output"
echo "  $WS/output"

# The container maps host ./scripts to /workspace/course-scripts (compose.yaml).
# This symlink reproduces that name so the same paths work natively.
ln -sfn scripts "$WS/course-scripts"
echo "  $WS/course-scripts -> scripts"

bash "$WS/scripts/install-course-configs.sh"

stamp wireup.done

# ---------------------------------------------------------------------------
# 6. Verify
# ---------------------------------------------------------------------------

step "[6/6] Verifying the installation"

set +u
# shellcheck disable=SC1091
source "$CONDA_DIR/etc/profile.d/conda.sh"
conda activate base

bash "$WS/scripts/test-install.sh"

stamp verify.done

cat <<EOF

======================================
Native setup complete
======================================

Start every session with:

    source "$WS/scripts/native-activate.sh"

Add that line to your ~/.bashrc if you want it automatically. Then, inside
$CHIPYARD:

    source env.sh

See $WS/docs/native-install.md for the full workflow.

EOF
