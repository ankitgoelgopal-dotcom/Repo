#!/usr/bin/env bash
# Boots the ISO headless in QEMU, captures serial output, and checks for the boot marker.
# Phase 1 checks: boots to long mode, prints memory map, reaches "BOOT OK".
# Network check is wired in for Phase 6+ (e1000 NIC is already attached so drivers can find it).
set -u
ISO="${1:-build/qaros.iso}"
LOG="${2:-build/qemu-serial.log}"
TIMEOUT="${QEMU_TIMEOUT:-60}"
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
check "QAR-OS 0.2.0"                         "kernel banner printed"
check "long mode active"                     "reached 64-bit long mode"
check "frame allocator ready"                "physical memory manager up"
check "GDT/TSS/IDT loaded"                   "interrupts + timer configured"
check "QAR-OS: BOOT OK"                      "kernel init completed"
check "verified hello.qar org.qar.hello"     "signed package verified in-kernel"
check "REJECTED tampered.qar"                "tampered package rejected"
check "REJECTED untrusted.qar"               "untrusted signer rejected"
check "Hello from a signed .qar app"         "ring-3 app ran (ELF load + syscalls)"
check "heap works: 20 + 22 = 42"             "SDK libc malloc/snprintf work"
check "socket syscall -> -38"                "network ABI reserved (ENOSYS)"
check "faulted: vector 14.*killed"           "user fault killed the process only"
check "QAR-OS: ALL APPS FINISHED"            "kernel survived and reaped all processes"
# preemption: both busy-looping tickers must interleave (A, B, A, B ... not all A then all B)
if tr -d '\r' < "$LOG" | grep -E '^\[[AB]\] tick' | awk '{print $1}' | uniq | wc -l | awk '{exit !($1>=4)}'; then
  echo "PASS: preemptive scheduling (tickers interleaved)"; else echo "FAIL: preemptive scheduling"; fail=1; fi
if grep -q "THIS LINE MUST NEVER PRINT" "$LOG"; then echo "FAIL: crash app continued after fault"; fail=1; fi
if grep -q "KERNEL PANIC" "$LOG"; then echo "FAIL: kernel panic"; fail=1; fi
[ "$rc" -ne 124 ] && [ "$rc" -ne 0 ] && { echo "WARN: qemu exit code $rc"; }
exit $fail
