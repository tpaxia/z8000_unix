#!/bin/bash
# Compile a C program with ACK, wrap with Z8001 reset vector, and run in emulator.
#
# Usage: ./run_test.sh <source.c> [emulator flags...]
# Example: ./run_test.sh test_add.c -t       # with instruction trace
#          ./run_test.sh test_add.c -t -r     # with register trace

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
ACK="$ROOT/ack"
EMU="$ROOT/z8000_emu/build/z8000emu"
ACKBIN="$ACK/.obj/staging/bin/ack"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.c> [emulator flags...]"
    exit 1
fi

SRC="$1"
shift
EMU_FLAGS="$@"

# Resolve source path
if [ ! -f "$SRC" ]; then
    SRC="$SCRIPT_DIR/$SRC"
fi
if [ ! -f "$SRC" ]; then
    echo "Error: source file not found: $1"
    exit 1
fi

BASENAME="$(basename "$SRC" .c)"
OUTDIR="$SCRIPT_DIR/build"
mkdir -p "$OUTDIR"

# Check tools exist
if [ ! -x "$ACKBIN" ]; then
    echo "Error: ACK not built. Run: cd ack && gmake HOSTCC=cc CC=cc -j8 NINJA='ninja -k0'"
    exit 1
fi
if [ ! -x "$EMU" ]; then
    echo "Error: Emulator not built. Run: cd z8000_emu && make -j8"
    exit 1
fi

# Compile with ACK
echo "Compiling $SRC..."
ACKDIR="$ACK/.obj/staging" "$ACKBIN" -mz8000 -o "$OUTDIR/$BASENAME.out" "$SRC"

# Extract flat binary
echo "Extracting flat binary..."
"$ACK/.obj/staging/bin/aslod" "$OUTDIR/$BASENAME.out" "$OUTDIR/$BASENAME.flat"

# Prepend Z8001 reset vector
echo "Creating Z8001 binary..."
python3 -c "
import struct, sys
# Z8001 reset vector (8 bytes):
#   0x0000: Reserved
#   0x0002: FCW = 0xC000 (segmented + system mode)
#   0x0004: Segment word = 0x8000 (segment 0, long format)
#   0x0006: Offset = 0x0008 (entry point, right after reset vector)
reset_vector = struct.pack('>HHHH', 0x0000, 0xC000, 0x8000, 0x0008)
with open('$OUTDIR/$BASENAME.flat', 'rb') as f:
    code = f.read()
with open('$OUTDIR/$BASENAME.bin', 'wb') as f:
    f.write(reset_vector)
    f.write(code)
print(f'Binary: {len(reset_vector) + len(code)} bytes (code at 0x0008)')
"

# Run in emulator
echo "Running on Z8001..."
echo "---"
"$EMU" -s $EMU_FLAGS "$OUTDIR/$BASENAME.bin"
