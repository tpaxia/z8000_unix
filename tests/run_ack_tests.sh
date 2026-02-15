#!/bin/bash
# Run the ACK platform test suite on Z8002.
#
# Usage: ./tests/run_ack_tests.sh [-v] [-t testname]

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ACK="$ROOT/ack"
EMU="$ROOT/z8000_emu/build/z8000emu"
ACKBIN="$ACK/.obj/staging/bin/ack"
ASLOD="$ACK/.obj/staging/bin/aslod"
TESTLIB="$ACK/tests/plat/lib/test.c"
TESTDIR="$ACK/tests/plat"
OUTDIR="$ROOT/tests/build/ack_tests"
PLATDIR="$ACK/.obj/staging/share/ack/z8000"

VERBOSE=0
FILTER=""
MAX_CYCLES=500000

while getopts "vt:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        t) FILTER="$OPTARG" ;;
        *) echo "Usage: $0 [-v] [-t testname]"; exit 1 ;;
    esac
done

mkdir -p "$OUTDIR"

pass=0
fail=0
skip=0
errors=""

run_test() {
    local src="$1"
    local name="$2"
    local lang="$3"
    local cflags="$4"
    local out="$OUTDIR/${name}.out"
    local ack="ACKDIR=$ACK/.obj/staging $ACKBIN"

    if [ -n "$FILTER" ] && [ "$name" != "$FILTER" ]; then
        return
    fi

    local ack_flags="-mz8000 -z8002 -I${TESTDIR}/lib"

    if [ "$lang" = "e" ]; then
        # EM-style: compile to .o, link without c-ansi.o
        ACKDIR="$ACK/.obj/staging" "$ACKBIN" $ack_flags $cflags \
            -c -o "$OUTDIR/${name}.o" "$src" 2>"$OUTDIR/${name}.err" && \
        ACKDIR="$ACK/.obj/staging" "$ACKBIN" $ack_flags \
            -c -o "$OUTDIR/${name}_testlib.o" "$TESTLIB" 2>>"$OUTDIR/${name}.err" && \
        ACKDIR="$ACK/.obj/staging" "$ACKBIN" $ack_flags \
            -o "$out" "$OUTDIR/${name}.o" "$OUTDIR/${name}_testlib.o" \
            "$PLATDIR/libsys.a" 2>>"$OUTDIR/${name}.err" || {
            if [ $VERBOSE -eq 1 ]; then cat "$OUTDIR/${name}.err"; fi
            echo "  SKIP $name (build failed)"
            skip=$((skip + 1))
            return
        }
    else
        # C-style: normal compile+link (includes c-ansi.o)
        ACKDIR="$ACK/.obj/staging" "$ACKBIN" $ack_flags $cflags \
            -o "$out" "$src" "$TESTLIB" \
            2>"$OUTDIR/${name}.err" || {
            if [ $VERBOSE -eq 1 ]; then cat "$OUTDIR/${name}.err"; fi
            echo "  SKIP $name (build failed)"
            skip=$((skip + 1))
            return
        }
    fi

    # Extract flat binary + prepend z8002 reset vector
    "$ASLOD" "$out" "$OUTDIR/${name}.flat" 2>/dev/null || {
        echo "  SKIP $name (aslod failed)"
        skip=$((skip + 1))
        return
    }
    python3 -c "
import struct
with open('$OUTDIR/${name}.flat', 'rb') as f:
    code = f.read()
reset = struct.pack('>HHHH', 0x0000, 0x4000, 0x0008, 0x0000)
with open('$OUTDIR/${name}.bin', 'wb') as f:
    f.write(reset)
    f.write(code)
"

    # Run on emulator
    local output
    output=$("$EMU" -c $MAX_CYCLES "$OUTDIR/${name}.bin" 2>&1) || true

    # Check result
    if echo "$output" | grep -q "@@FINISHED"; then
        if echo "$output" | grep -q "@@FAIL"; then
            local failline
            failline=$(echo "$output" | grep "@@FAIL" | head -1)
            echo "  FAIL $name ($failline)"
            fail=$((fail + 1))
            errors="$errors\n  FAIL $name: $failline"
            if [ $VERBOSE -eq 1 ]; then echo "$output"; fi
        else
            echo "  PASS $name"
            pass=$((pass + 1))
        fi
    else
        if echo "$output" | grep -q "Halted: Yes"; then
            echo "  FAIL $name (halted without @@FINISHED)"
        else
            echo "  FAIL $name (timed out or crashed)"
        fi
        fail=$((fail + 1))
        errors="$errors\n  FAIL $name: no @@FINISHED"
        if [ $VERBOSE -eq 1 ]; then echo "$output"; fi
    fi
}

echo "ACK Z8002 Test Suite"
echo "===================="
echo ""

# Core tests
echo "Core tests:"
for src in "$TESTDIR"/core/*,.c; do
    [ -f "$src" ] || continue
    base=$(basename "$src")
    name=$(echo "$base" | cut -d, -f1)
    lang=$(echo "$base" | cut -d, -f2)
    run_test "$src" "$name" "$lang" ""
done

# Bug regression tests
echo ""
echo "Bug regression tests:"
for src in "$TESTDIR"/bugs/*,.c; do
    [ -f "$src" ] || continue
    base=$(basename "$src")
    name=$(echo "$base" | cut -d, -f1)
    lang=$(echo "$base" | cut -d, -f2)
    flags=$(echo "$base" | sed 's/[^,]*,[^,]*//' | sed 's/,\.c$//' | tr ',' ' ')
    run_test "$src" "$name" "$lang" "$flags"
done

echo ""
echo "===================="
echo "Results: $pass passed, $fail failed, $skip skipped"

if [ $fail -gt 0 ]; then
    echo ""
    echo "Failures:"
    echo -e "$errors"
    exit 1
fi
