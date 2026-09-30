#!/usr/bin/env bash
#
# ECE 462/562 - native (no-Docker) session setup.
#
# The native analogue of scripts/course-entrypoint.sh, which is what runs when
# the course container starts. Source it once per login:
#
#     source scripts/native-activate.sh
#
# Then, as in every course document:
#
#     source env.sh
#
# Sourcing (not running) matters: the exports have to survive into your shell.

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    echo "native-activate.sh must be sourced, not executed:" >&2
    echo "    source ${BASH_SOURCE[0]}" >&2
    exit 1
fi

# Workspace root: the clone this script lives in. Replaces the container's
# /workspace, and every course script honours it as WS.
WS="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export WS

CONDA_DIR="$HOME/miniforge3"

if [ ! -d "$WS/chipyard" ]; then
    echo "[course] $WS/chipyard does not exist. Run the installer first:" >&2
    echo "[course]     bash $WS/scripts/native-setup.sh" >&2
    return 1
fi

# The container writes /usr/local/bin/quiz-run so students can type a bare
# `quiz-run`. That needs root, so natively the same effect comes from PATH.
case ":$PATH:" in
    *":$WS/course-scripts:"*) ;;
    *) export PATH="$WS/course-scripts:$PATH" ;;
esac

# course-entrypoint.sh does this on every container start, so new course
# configurations appear after a plain `git pull`.
bash "$WS/scripts/install-course-configs.sh"

# Chipyard's own JVM heap default is 8G (chipyard/variables.mk). On a login
# node shared with the rest of the class, several 8 GB heaps at once is the
# problem you came to the server to avoid. Override it here only, so the
# Docker path is unaffected.
export JAVA_HEAP_SIZE="${JAVA_HEAP_SIZE:-4G}"

# Make conda's shell functions available without having run `conda init`, so
# nothing in your ~/.bashrc or any other conda on this machine is disturbed.
if [ -f "$CONDA_DIR/etc/profile.d/conda.sh" ]; then
    # shellcheck disable=SC1091
    source "$CONDA_DIR/etc/profile.d/conda.sh"
    conda activate base
else
    echo "[course] conda not found at $CONDA_DIR. Run scripts/native-setup.sh first." >&2
    return 1
fi

cd "$WS/chipyard" || return 1

echo "[course] workspace: $WS"
echo "[course] now run: source env.sh"
