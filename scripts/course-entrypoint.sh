#!/bin/bash
set -e

/workspace/course-scripts/install-course-configs.sh

printf '#!/bin/bash\nexec bash /workspace/course-scripts/quiz-run.sh "$@"\n' \
    > /usr/local/bin/quiz-run
chmod +x /usr/local/bin/quiz-run

# On emulated hosts (Apple Silicon) conda's CRT objects falsely mark every
# linked binary as needing x86-64-v3 (AVX), so glibc refuses to run it:
# "CPU ISA level is lower than required". Strip that marker. No-op on native x86.
(
    cd /workspace/chipyard
    source env.sh >/dev/null 2>&1 || exit 0
    printf 'int main(void){return 0;}\n' > /tmp/.isa.c
    if gcc -o /tmp/.isa /tmp/.isa.c 2>/dev/null && /tmp/.isa 2>/dev/null; then
        exit 0
    fi
    echo "[course] emulated CPU: stripping bogus ISA markers from conda CRT objects"
    for o in crt1.o Scrt1.o crti.o crtn.o crtbegin.o crtbeginS.o crtend.o crtendS.o; do
        f=$(gcc -print-file-name="$o" 2>/dev/null) || continue
        [ -f "$f" ] || continue
        objcopy --remove-section .note.gnu.property "$f" "$f.tmp" 2>/dev/null && mv "$f.tmp" "$f"
    done
    if gcc -o /tmp/.isa /tmp/.isa.c 2>/dev/null && /tmp/.isa 2>/dev/null; then
        echo "[course] done - locally linked binaries now run"
    else
        echo "[course] WARNING: binaries still fail to run after CRT patch" >&2
    fi
    rm -f /tmp/.isa /tmp/.isa.c
) || true

export PATH="/workspace/course-scripts:$PATH"

cd /workspace/chipyard

exec /bin/bash
