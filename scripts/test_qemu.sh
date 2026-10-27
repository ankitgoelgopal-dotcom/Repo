#!/usr/bin/env bash
# Boots the ISO headless in QEMU, captures serial output, and checks for the boot marker.
# Phase 1 checks: boots to long mode, prints memory map, reaches "BOOT OK".
# Network check is wired in for Phase 6+ (e1000 NIC is already attached so drivers can find it).
set -u
ISO="${1:-build/qaros.iso}"
LOG="${2:-build/qemu-serial.log}"
TIMEOUT="${QEMU_TIMEOUT:-20}"
mkdir -p "$(dirname "$LOG")"; : > "$LOG"

timeout "$TIMEOUT" qemu-system-x86_64 \
  -cdrom "$ISO" -m 256M -display none -no-reboot \
  -serial "file:$LOG" \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  >/dev/null 2>&1
rc=$?   # 124 = killed by timeout (expected: kernel idles in hlt)

echo "---- serial log ----"; cat "$LOG"; echo "--------------------"

fail=0
check() { if grep -q "$1" "$LOG"; then echo "PASS: $2"; else echo "FAIL: $2"; fail=1; fi; }
check "QAR-OS 0.1.0"        "kernel banner printed"
check "long mode active"    "reached 64-bit long mode"
check "usable RAM"          "memory map parsed"
check "QAR-OS: BOOT OK"     "boot completed"
if grep -q "KERNEL PANIC" "$LOG"; then echo "FAIL: kernel panic"; fail=1; fi
[ "$rc" -ne 124 ] && [ "$rc" -ne 0 ] && { echo "WARN: qemu exit code $rc"; }
exit $fail
