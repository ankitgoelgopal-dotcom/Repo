# QAR-OS roadmap

Each phase must boot and pass `scripts/test_qemu.sh` in CI before the next begins.

| Phase | Deliverable | Status |
|------:|-------------|--------|
| 1 | Multiboot2 boot, long mode, serial + VGA console, CI + QEMU boot test | **done** |
| 2 | Frame allocator, per-process address spaces (4-level paging), kernel heap | **done** (kernel still identity-mapped; higher-half move later) |
| 3 | GDT/TSS/IDT, PIT timer, preemptive round-robin scheduler, ring 3, `int 0x80` syscalls, ELF loader | **done** (single CPU, PIC not APIC) |
| 4 | Message-passing IPC (Binder-style), capability table | next |
| 5 | VFS + ramdisk | |
| 6 | Drivers: PCI scan + e1000 NIC | **done** (polled; PS/2 keyboard/mouse, virtio-net, framebuffer + compositor still to do) |
| 7 | ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP client, socket syscalls with `net.connect` capability | **done** (IPv6, TCP listen, out-of-order reassembly, user-space net service still to do) |
| 8 | SDK v0 + in-kernel `.qar` verifier + sample apps | **started** (SDK, libc subset, verifier done; sockets, graphics API, `pkgd`, `qpm` not yet) |
| 7b | TLS 1.2 client (Mbed TLS), certificate + host-name verification, RTC, entropy pool, `sbrk` heap | **done** (TLS 1.3 not supported) |
| 8b | Floating point: SSE2/x87 enabled, per-task FXSAVE state, SDK `libm` (musl), exact `printf %f/%e/%g`, `strtod`, `fenv.h` | **done** |
| 9 | Browser | |
| 10 | Bluetooth HCI; Wi-Fi interface and stubs | |

Notes
* Microkernel direction: kernel keeps scheduling, memory, IPC and capabilities; drivers,
  VFS, network and `pkgd` become user-space services from Phase 4 onward.
* Wi-Fi cannot be tested in QEMU (it emulates wired NICs). Real Wi-Fi needs hardware and firmware.
* HTTPS (needed by most of the web) needs TLS 1.2/1.3: plan to port an existing library such as BearSSL.
