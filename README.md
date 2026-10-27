# QAR-OS

x86_64 microkernel-oriented OS. Native apps are signed `.qar` packages (see `docs/QAR-SPEC.md`).
Current state: **Phase 1** (boots to a 64-bit kernel and logs to serial/VGA). See `docs/ROADMAP.md`.

## Build locally (Ubuntu/Debian)
```
sudo apt install gcc binutils make nasm xorriso grub-pc-bin grub-common mtools qemu-system-x86 python3-cryptography
make iso          # -> build/qaros.iso
make test         # boots in headless QEMU and checks the serial log
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
