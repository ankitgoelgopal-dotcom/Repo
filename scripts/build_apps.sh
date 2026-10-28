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

CF="-std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Werror -I$SDK/include"
$CC $CF -c "$SDK/crt0.S" -o "$SDK/crt0.o"
for f in "$ROOT"/sdk/libc/*.c; do $CC $CF -c "$f" -o "$SDK/obj/$(basename "$f" .c).o"; done
ar rcs "$SDK/lib/libqar.a" "$SDK"/obj/*.o

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
for t in A B; do
  lt=$(echo $t | tr A-Z a-z); sed "s/@T@/$lt/g" "$E/ticker/manifest.json.in" > "$PKG/ticker_$lt.json"
  stage "ticker_$lt" "$PKG/ticker_$lt.json" "$E/ticker/main.c" -DTAG="\"$t\""
done
for n in hello crash netdemo ticker_a ticker_b; do python3 "$ROOT/tools/qar.py" pack "$PKG/$n" "$APPS/$n.qar" --key "$KEYS/dev.key" >/dev/null; done

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
