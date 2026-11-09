#!/usr/bin/env bash
# Builds Lexbor + the htmltest app on the HOST and runs it, to cross-check the guest results.
set -euo pipefail
cd "$(dirname "$0")/.."; W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
L=third_party/lexbor/source
srcs=$(find $L -name '*.c' | grep -v windows)
gcc -O1 -w -std=gnu11 -DLEXBOR_STATIC -I$L -o "$W/ht" sdk/examples/htmltest/main.c $srcs -lm
"$W/ht"
