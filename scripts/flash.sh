
  #!/usr/bin/env bash
  set -euo pipefail

  BIN="${1:-build/Debug/app_signed.bin}"
  ADDR="${2:-0x08023000}"
  DEVICE="${3:-GD32F303CC}"

  JLinkExe -device "$DEVICE" -if SWD -speed 4000 -autoconnect 1 <<EOF
  r
  h
  loadbin $BIN $ADDR
  verifybin $BIN $ADDR
  r
  g
  q
  EOF