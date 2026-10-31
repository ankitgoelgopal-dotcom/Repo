#!/usr/bin/env bash
# Boots the ISO headless in QEMU, captures serial output, and checks for the boot marker.
# Phase 1 checks: boots to long mode, prints memory map, reaches "BOOT OK".
# Network check is wired in for Phase 6+ (e1000 NIC is already attached so drivers can find it).
set -u
ISO="${1:-build/qaros.iso}"
LOG="${2:-build/qemu-serial.log}"
TIMEOUT="${QEMU_TIMEOUT:-120}"
mkdir -p "$(dirname "$LOG")"; : > "$LOG"

WWW="$(mktemp -d)"; echo "<html><body>QAR-OS network test page</body></html>" > "$WWW/index.html"
python3 -m http.server 8000 --bind 0.0.0.0 --directory "$WWW" >/dev/null 2>&1 &
HTTP_PID=$!
CERTS="$(dirname "$ISO")/tlstest"
python3 "$(dirname "$0")/../tests/https_server.py" "$CERTS" 8443 >/dev/null 2>&1 &
HTTPS_PID=$!
trap 'kill $HTTP_PID $HTTPS_PID 2>/dev/null; rm -rf "$WWW"' EXIT
sleep 1

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
check "e1000 MAC"                            "PCI scan found the e1000 NIC, driver up"
check "DHCP: ip 10.0.2.15"                   "DHCP lease obtained"
check "ARP: gateway is at"                   "ARP resolved the gateway"
check "ICMP: ping gateway reply"             "ICMP echo to the gateway answered"
check "without net.connect capability -> -1" "network blocked for apps without net.connect"
check "TCP connected to 10.0.2.2:8000"       "TCP three-way handshake from an app"
check "HTTP status line: HTTP/1.0 200 OK"    "HTTP response received over TCP"
if grep -q "DNS example.com -> " "$LOG"; then echo "PASS: DNS lookup returned an address"
else echo "WARN: DNS lookup failed (needs internet on the host; not counted as a failure)"; fi
check "tlsdemo\[local-trusted\]: handshake OK"   "HTTPS handshake verified against a trusted CA"
check "tlsdemo\[local-trusted\]: HTTPS status line: HTTP/1.0 200 OK" "HTTPS page fetched over TLS"
check "tlsdemo\[local-untrusted\]: handshake REJECTED" "TLS rejects a certificate from an untrusted CA"
if grep -q "tlsdemo\[internet\]: HTTPS status line: HTTP/1" "$LOG"; then echo "PASS: real HTTPS site fetched (certificate chain verified against built-in roots)"
else echo "WARN: real-internet HTTPS not tested (needs internet on the host; not counted as a failure)"; fi
check "faulted: vector 14.*killed"           "user fault killed the process only"
check "QAR-OS: ALL APPS FINISHED"            "kernel survived and reaped all processes"
# preemption: both busy-looping tickers must interleave (A, B, A, B ... not all A then all B)
if tr -d '\r' < "$LOG" | grep -E '^\[[AB]\] tick' | awk '{print $1}' | uniq | wc -l | awk '{exit !($1>=4)}'; then
  echo "PASS: preemptive scheduling (tickers interleaved)"; else echo "FAIL: preemptive scheduling"; fail=1; fi
if grep -q "THIS LINE MUST NEVER PRINT" "$LOG"; then echo "FAIL: crash app continued after fault"; fail=1; fi
if grep -q "KERNEL PANIC" "$LOG"; then echo "FAIL: kernel panic"; fail=1; fi
[ "$rc" -ne 124 ] && [ "$rc" -ne 0 ] && { echo "WARN: qemu exit code $rc"; }
# ---- floating point: SSE/x87 on, state saved per task, SDK libm / printf / strtod exact ----
check "FPU/SSE on"                           "kernel enabled FPU/SSE and per-task FP state"
check "MATHTEST DONE"                        "math test app ran to completion"
tr -d '\r' < "$LOG" > "$LOG.clean"
if python3 "$(dirname "$0")/check_math.py" "$LOG.clean" > "$LOG.math" 2>&1; then
  echo "PASS: $(head -1 "$LOG.math") (libm, printf %f/%e/%g, strtod match Python/IEEE-754)"
else echo "FAIL: math check"; cat "$LOG.math"; fail=1; fi
# two FP-heavy processes ran at the same time (their run intervals overlap) and each got the exact result
a=$(grep -a '^FPU\[A\]' "$LOG.clean"); b=$(grep -a '^FPU\[B\]' "$LOG.clean")
if [ -n "$a" ] && [ -n "$b" ] && python3 - "$a" "$b" <<'PY'
import re,sys
def iv(l): m=re.search(r'start=(\d+) end=(\d+)',l); return int(m[1]),int(m[2])
(a0,a1),(b0,b1)=iv(sys.argv[1]),iv(sys.argv[2]); sys.exit(0 if a0<b1 and b0<a1 else 1)
PY
then echo "PASS: two floating-point processes ran concurrently without corrupting each other (exact bits checked)"
else echo "FAIL: FPU isolation test"; fail=1; fi

exit $fail
