#!/usr/bin/env bash
# build.sh — Cross-compile a Windows EXE (≥120 MB) using MinGW.
#
# Usage:
#   bash tools/largeexe/build.sh [output_dir]
#
# The finished executable is written to <output_dir>/sdrangel_large.exe
# (default: tools/largeexe/out/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${1:-${SCRIPT_DIR}/out}"
mkdir -p "${OUT_DIR}"

CROSS=x86_64-w64-mingw32
CC="${CROSS}-gcc"
OBJCOPY="${CROSS}-objcopy"

# ---------------------------------------------------------------------------
# 1. Generate a 130 MB binary data blob (130 × 1 MiB).
#    Using /dev/urandom so the data is non-zero and will be stored in the
#    .data section rather than being silently collapsed by the linker.
# ---------------------------------------------------------------------------
DATA_BIN="${OUT_DIR}/bigdata.bin"
echo "[1/4] Generating 130 MiB data blob → ${DATA_BIN}"
dd if=/dev/urandom of="${DATA_BIN}" bs=1M count=130 status=none

# ---------------------------------------------------------------------------
# 2. Convert the blob to a PE/COFF object file that the MinGW linker can use.
#    objcopy injects three symbols:
#      _binary_<mangled>_start
#      _binary_<mangled>_end
#      _binary_<mangled>_size
# ---------------------------------------------------------------------------
DATA_OBJ="${OUT_DIR}/bigdata.o"
echo "[2/4] Converting blob to PE object → ${DATA_OBJ}"
# Run objcopy from OUT_DIR so the generated symbol names are simply
# _binary_bigdata_bin_{start,end,size} rather than including the full path.
(cd "${OUT_DIR}" && "${OBJCOPY}" \
    --input-target  binary \
    --output-target pe-x86-64 \
    --binary-architecture i386:x86-64 \
    bigdata.bin bigdata.o)

# ---------------------------------------------------------------------------
# 3. Compile main.c
# ---------------------------------------------------------------------------
MAIN_OBJ="${OUT_DIR}/main.o"
echo "[3/4] Compiling main.c → ${MAIN_OBJ}"
"${CC}" -O2 -c "${SCRIPT_DIR}/main.c" -o "${MAIN_OBJ}"

# ---------------------------------------------------------------------------
# 4. Link everything into a Windows console EXE
# ---------------------------------------------------------------------------
EXE="${OUT_DIR}/sdrangel_large.exe"
echo "[4/4] Linking → ${EXE}"
"${CC}" -o "${EXE}" "${MAIN_OBJ}" "${DATA_OBJ}"

SIZE_BYTES=$(stat -c%s "${EXE}")
SIZE_MB=$(awk "BEGIN { printf \"%.2f\", ${SIZE_BYTES}/1048576 }")
echo "Done. ${EXE} — ${SIZE_BYTES} bytes (${SIZE_MB} MiB)"

if [ "${SIZE_BYTES}" -lt $((120 * 1024 * 1024)) ]; then
    echo "ERROR: EXE is smaller than 120 MiB!" >&2
    exit 1
fi
