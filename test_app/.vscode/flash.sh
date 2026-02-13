#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

BIN="${1:-$PROJECT_ROOT/rtthread.bin}"
ADDR="${2:-0x08007800}"
DEVICE="${3:-GD32F303CC}"

if [[ ! -f "$BIN" ]]; then
  echo "Error: bin not found: $BIN" >&2
  exit 1
fi

UNAME_S="$(uname -s)"
case "$UNAME_S" in
  MINGW*|MSYS*|CYGWIN*)
    if command -v cygpath >/dev/null 2>&1; then
      BIN_FOR_JLINK="$(cygpath -w "$BIN")"
    else
      BIN_FOR_JLINK="$BIN"
    fi
    JLINK_BIN="${JLINK_BIN:-JLink.exe}"
    ;;
  *)
    BIN_FOR_JLINK="$BIN"
    JLINK_BIN="${JLINK_BIN:-JLinkExe}"
    ;;
esac

echo "Flashing $BIN_FOR_JLINK to $ADDR on $DEVICE using $JLINK_BIN"

"$JLINK_BIN" -device "$DEVICE" -if SWD -speed 4000 -autoconnect 1 <<EOF
r
h
loadbin $BIN_FOR_JLINK $ADDR
verifybin $BIN_FOR_JLINK $ADDR
r
g
q
EOF
