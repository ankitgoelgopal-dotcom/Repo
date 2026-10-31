#!/usr/bin/env bash
# Builds the SDK, compiles the example apps with qar-cc, and packs them into signed .qar files.
#   usage: build_apps.sh <build-dir>
set -euo pipefail
B="${1:-build}"; ROOT="$(cd "$(dirname "$0")/.." && pwd)"; B="$(cd "$ROOT" && mkdir -p "$B" && cd "$B" && pwd)"
CC="${CC:-gcc}"; export CC
SDK="$B/sdk"; APPS="$B/isoroot/apps"; PKG="$B/pkg"; KEYS="$B/keys"
rm -rf "$SDK" "$PKG" "$APPS"; mkdir -p "$SDK/lib" "$SDK/obj" "$APPS" "$PKG" "$KEYS"
cp -r "$ROOT/sdk/include" "$ROOT/sdk/bin" "$ROOT/sdk/app.ld" "$ROOT/sdk/crt0.S" "$SDK/"
cp "$ROOT/tools/qar.py" "$SDK/bin/qar"; mkdir -p "$SDK/examples"; cp -r "$ROOT/sdk/examples/." "$SDK/examples/"

CF="-std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -fno-math-errno -ffp-contract=off -Wall -Wextra -Werror -I$SDK/include"
$CC $CF -c "$SDK/crt0.S" -o "$SDK/crt0.o"
for f in "$ROOT"/sdk/libc/*.c; do
  case "$(basename "$f")" in os_tls.c|tls_glue.c|sqrt.c|strtod.c|ldmath.c) continue;; esac
  $CC $CF -c "$f" -o "$SDK/obj/$(basename "$f" .c).o"
done

# libm: musl's math routines (double + float), freestanding, SSE2 hardware floating point. -w silences third-party warnings only.
MM="$ROOT/third_party/musl-math"
mkdir -p "$SDK/obj_m"
MF="-std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -fno-math-errno -ffp-contract=off -w -include $MM/shim/qar_musl.h -I$SDK/include -I$MM/shim -I$MM/internal -I$MM/src"
for f in "$MM"/src/*.c; do $CC $MF -c "$f" -o "$SDK/obj_m/$(basename "$f" .c).o"; done
$CC $MF -c "$ROOT/sdk/libc/sqrt.c" -o "$SDK/obj_m/sqrt_hw.o"
ar rcs "$SDK/lib/libm.a" "$SDK"/obj_m/*.o; rm -rf "$SDK/obj_m"
# strtod/strtof: musl's exact float scanner (needs a few x87 long double helpers), built into libc
for f in "$MM"/scan/floatscan.c "$MM"/scan/shgetc.c "$MM"/scan/scalbnl.c "$MM"/scan/fmodl.c "$ROOT"/sdk/libc/strtod.c "$ROOT"/sdk/libc/ldmath.c; do
  $CC $MF -I"$MM/scan" -c "$f" -o "$SDK/obj/s_$(basename "$f" .c).o"
done

# TLS: Mbed TLS (vendored in third_party/) + glue, built with the same freestanding flags. Warnings are silenced for third-party code only.
MB="$ROOT/third_party/mbedtls"
MBF="-std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -w -I$SDK/include -I$ROOT/sdk/mbedtls -I$MB/include -I$MB/library -DMBEDTLS_CONFIG_FILE=\"qar_config.h\""
mkdir -p "$SDK/obj_tls"
NJ="$(nproc 2>/dev/null || echo 2)"
for f in "$MB"/library/*.c; do
  $CC $MBF -c "$f" -o "$SDK/obj_tls/$(basename "$f" .c).o" &
  while [ "$(jobs -r | wc -l)" -ge "$NJ" ]; do sleep 0.05; done
done
wait
for f in os_tls tls_glue; do $CC $MBF -c "$ROOT/sdk/libc/$f.c" -o "$SDK/obj_tls/$f.o"; done
python3 "$ROOT/tools/mkcabundle.py" "$SDK/obj_tls/ca_bundle.c" >/dev/null
$CC $MBF -c "$SDK/obj_tls/ca_bundle.c" -o "$SDK/obj_tls/ca_bundle.o"; rm -f "$SDK/obj_tls/ca_bundle.c"
ar rcs "$SDK/lib/libqar.a" "$SDK"/obj/*.o "$SDK"/obj_tls/*.o
rm -rf "$SDK/obj" "$SDK/obj_tls"

# throwaway CA + server cert for the local HTTPS test, embedded in the tlsdemo app
mkdir -p "$B/tlstest" && python3 "$ROOT/tools/mktestcerts.py" "$B/tlstest" >/dev/null
python3 - "$B/tlstest/ca.pem" "$B/tlstest/test_ca.h" <<'PY'
import sys
pem = open(sys.argv[1]).read()
open(sys.argv[2], "w").write("static const char test_ca_pem[] =\n" + "\n".join('"' + l + '\\n"' for l in pem.splitlines()) + ";\n")
PY

[ -f "$KEYS/dev.key" ]   || python3 "$ROOT/tools/qar.py" keygen "$KEYS/dev"   >/dev/null
[ -f "$KEYS/other.key" ] || python3 "$ROOT/tools/qar.py" keygen "$KEYS/other" >/dev/null

stage() {   # stage <name> <manifest> <extra qar-cc args...>
  local n="$1" m="$2"; shift 2; mkdir -p "$PKG/$n"
  "$SDK/bin/qar-cc" "$@" -o "$PKG/$n/main"
  cp "$m" "$PKG/$n/manifest.json"
}
E="$ROOT/sdk/examples"
stage hello  "$E/hello/manifest.json" "$E/hello/main.c"
stage crash  "$E/crash/manifest.json" "$E/crash/main.c"
stage netdemo "$E/netdemo/manifest.json" "$E/netdemo/main.c"
stage tlsdemo "$E/tlsdemo/manifest.json" "$E/tlsdemo/main.c" -I"$B/tlstest"
for t in A B; do
  lt=$(echo $t | tr A-Z a-z); sed "s/@T@/$lt/g" "$E/ticker/manifest.json.in" > "$PKG/ticker_$lt.json"
  stage "ticker_$lt" "$PKG/ticker_$lt.json" "$E/ticker/main.c" -DTAG="\"$t\""
done
stage mathtest "$E/mathtest/manifest.json" "$E/mathtest/main.c"
sed "s/@T@/a/g" "$E/fputest/manifest.json.in" > "$PKG/fputest_a.json"; sed "s/@T@/b/g" "$E/fputest/manifest.json.in" > "$PKG/fputest_b.json"
stage fputest_a "$PKG/fputest_a.json" "$E/fputest/main.c" -DTAG='"A"' -DX0=1.5 -DY0=0.5
stage fputest_b "$PKG/fputest_b.json" "$E/fputest/main.c" -DTAG='"B"' -DX0=2.5 -DY0=3.5
for n in hello crash netdemo tlsdemo ticker_a ticker_b mathtest fputest_a fputest_b; do python3 "$ROOT/tools/qar.py" pack "$PKG/$n" "$APPS/$n.qar" --key "$KEYS/dev.key" >/dev/null; done

# security demos: one tampered package and one signed by an untrusted key. The kernel must reject both.
python3 "$ROOT/tools/qar.py" pack "$PKG/hello" "$APPS/untrusted.qar" --key "$KEYS/other.key" >/dev/null
python3 - "$APPS/hello.qar" "$APPS/tampered.qar" <<'PY'
import sys, struct
b = bytearray(open(sys.argv[1], 'rb').read()); off = struct.unpack_from("<Q", b, 52)[0]
b[off + 40] ^= 0xFF; open(sys.argv[2], 'wb').write(b)
PY
# trusted key header for the kernel
python3 - "$KEYS/dev.pub" "$B/trusted_key.h" <<'PY'
import sys
k = open(sys.argv[1], 'rb').read()
open(sys.argv[2], 'w').write("/* generated: dev trust anchor (a fresh key per build) */\n#include <stdint.h>\n"
  "static const uint8_t trusted_keys[1][32] = {{" + ",".join(str(x) for x in k) + "}};\n#define TRUSTED_KEY_COUNT 1\n")
PY
( cd "$B" && rm -rf qar-sdk && mkdir qar-sdk && cp -r sdk/include sdk/bin sdk/lib sdk/crt0.o sdk/app.ld sdk/examples qar-sdk/ \
  && cat > qar-sdk/README.txt <<'TXT'
QAR-OS SDK. Build:  bin/qar-cc app.c -o main     Package:  bin/qar pack <dir with main + manifest.json> app.qar --key your.key
Needs: gcc (x86_64 Linux), python3, python3-cryptography.
TXT
  tar czf qar-sdk.tar.gz qar-sdk )
echo "apps: $(ls "$APPS" | tr '\n' ' ')"
# app-store bundle (index + signed packages) that a future in-OS `qpm` will download
python3 "$ROOT/tools/qar.py" keygen "$KEYS/store" >/dev/null 2>&1 || true
python3 "$ROOT/tools/mkstore.py" build "$APPS" "$B/store" --key "$KEYS/store.key" --base-url "https://YOUR-HOST/store"
python3 "$ROOT/tools/mkstore.py" verify "$B/store" --pub "$KEYS/store.pub"
