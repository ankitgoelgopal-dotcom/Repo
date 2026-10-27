# QAR-OS

x86_64 microkernel-oriented OS. Native apps are signed `.qar` packages (see `docs/QAR-SPEC.md`).
Current state: boots, runs signed `.qar` apps in ring 3 with preemptive multitasking. See `docs/ROADMAP.md`.

What runs at boot: the kernel verifies each `.qar` module (SHA-256 + Ed25519), loads the ELF inside, and starts it
as an isolated process. It also rejects a tampered package and one signed by an unknown key (both are in the ISO on
purpose), and survives a deliberately crashing app.

## SDK
CI uploads `qar-sdk` (`qar-sdk.tar.gz`). It contains headers (`os_api.h`, `stdio.h`, `string.h`, `stdlib.h`), a libc subset,
`bin/qar-cc` (compiler wrapper), `bin/qar` (packer/signer) and examples:
```
tar xzf qar-sdk.tar.gz && cd qar-sdk
bin/qar-cc examples/hello/main.c -o main           # compile for QAR-OS
mkdir pkg && mv main pkg/ && cp examples/hello/manifest.json pkg/
bin/qar keygen mykey && bin/qar pack pkg app.qar --key mykey.key
```
Note: the kernel only trusts the key generated at ISO build time (printed by CI as part of the build, stored only in the
build job). Production use needs a fixed, offline signing key compiled into the kernel image.

## App downloads
See `docs/APP-STORE.md`. The signed store index and its builder exist; the in-OS downloader needs the network stack first.

## Build locally (Ubuntu/Debian)
```
sudo apt install gcc binutils make nasm xorriso grub-pc-bin grub-common mtools qemu-system-x86 python3-cryptography
make iso          # -> build/qaros.iso
make test         # boots in headless QEMU and checks the serial log
make hosttest     # cross-checks the in-OS verifier against the Python packer
make run          # boots with a window; serial output in your terminal
```
Toolchain note: Ubuntu has no `x86_64-elf-gcc` package. The build uses the host `gcc` in freestanding
mode (`-ffreestanding -mno-red-zone -mgeneral-regs-only`), which is fine on an x86_64 Linux host. If you
have a real cross compiler: `make CROSS=x86_64-elf-`.

## CI
`.github/workflows/build.yml` builds the ISO, tests the `.qar` tooling, boots the ISO in QEMU, and uploads
`qaros-iso` and `qemu-logs` as artifacts.

## Test the ISO yourself
**PC (QEMU):** `qemu-system-x86_64 -cdrom qaros.iso -m 256M -serial stdio`
(add `-netdev user,id=n0 -device e1000,netdev=n0` for the NIC the network phase will use).

**Android (Limbo PC Emulator):** download the `qaros-iso` artifact from the Actions run and unzip it.
In Limbo create a VM: Architecture x86_64, Machine pc, CPU qemu64, RAM 256 MB+, CD-ROM = `qaros.iso`,
boot from CD-ROM. Enable a serial/console log if your Limbo build offers it. Expect text on the
screen ending in `QAR-OS: BOOT OK`. Limbo is slow (no hardware acceleration), so give it a minute.

## `.qar` tooling
```
python3 tools/qar.py keygen mykey                       # mykey.key (secret), mykey.pub
python3 tools/qar.py pack myapp/ app.qar --key mykey.key  # myapp/ needs manifest.json
python3 tools/qar.py verify app.qar --trust mykey.pub
```
