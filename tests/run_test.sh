#!/bin/bash
# Compile a C program with ACK, wrap with reset vector, and run in emulator.
#
# Usage: ./run_test.sh <source.c> [-p z8001|z8002] [emulator flags...]
# Example: ./run_test.sh test_add.c -t           # Z8001 (default), instruction trace
#          ./run_test.sh test_add.c -p z8002 -t   # Z8002 non-segmented

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
ACK="$ROOT/ack"
EMU="$ROOT/z8000_emu/build/z8000emu"
ACKBIN="$ACK/.obj/staging/bin/ack"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.c> [-p z8001|z8002] [emulator flags...]"
    exit 1
fi

SRC="$1"
shift

# Parse -p flag
PLATFORM="z8001"
if [ "$1" = "-p" ]; then
    shift
    PLATFORM="$1"
    shift
fi

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

# Set platform-specific options
case "$PLATFORM" in
    z8001)
        ACK_FLAGS="-mz8000"
        EMU_MODE="-s"
        ;;
    z8002)
        ACK_FLAGS="-mz8000 -z8002"
        EMU_MODE=""
        ;;
    *)
        echo "Error: unknown platform '$PLATFORM' (use z8001 or z8002)"
        exit 1
        ;;
esac

# Compile with ACK
echo "Compiling $SRC ($PLATFORM)..."
ACKDIR="$ACK/.obj/staging" "$ACKBIN" $ACK_FLAGS -o "$OUTDIR/$BASENAME.out" "$SRC"

# Extract flat binary
echo "Extracting flat binary..."
"$ACK/.obj/staging/bin/aslod" "$OUTDIR/$BASENAME.out" "$OUTDIR/$BASENAME.flat"

# Prepend reset vector
echo "Creating $PLATFORM binary..."
python3 -c "
import struct, sys
with open('$OUTDIR/$BASENAME.flat', 'rb') as f:
    code = f.read()
if '$PLATFORM' == 'z8001':
    # Z8001 reset vector (8 bytes):
    #   0x0000: Reserved
    #   0x0002: FCW = 0xC000 (segmented + system mode)
    #   0x0004: Segment word = 0x8000 (segment 0, long format)
    #   0x0006: Offset = 0x0008 (entry point)
    reset_vector = struct.pack('>HHHH', 0x0000, 0xC000, 0x8000, 0x0008)
    entry = '0x0008'
else:
    # Z8002 reset vector (8 bytes, padded to match -b0:0x0008):
    #   0x0000: Reserved
    #   0x0002: FCW = 0x4000 (non-segmented + system mode)
    #   0x0004: PC = 0x0008 (entry point)
    #   0x0006: Padding
    reset_vector = struct.pack('>HHHH', 0x0000, 0x4000, 0x0008, 0x0000)
    entry = '0x0008'
with open('$OUTDIR/$BASENAME.bin', 'wb') as f:
    f.write(reset_vector)
    f.write(code)
print(f'Binary: {len(reset_vector) + len(code)} bytes (code at {entry})')
"

# Run in emulator
echo "Running on $PLATFORM..."
echo "---"
"$EMU" $EMU_MODE $EMU_FLAGS "$OUTDIR/$BASENAME.bin"
