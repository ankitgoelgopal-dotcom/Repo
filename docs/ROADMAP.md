# QAR-OS roadmap

Each phase must boot and pass `scripts/test_qemu.sh` in CI before the next begins.

| Phase | Deliverable | Status |
|------:|-------------|--------|
| 1 | Multiboot2 boot, long mode, serial + VGA console, CI + QEMU boot test | **done** |
| 2 | Frame allocator, per-process address spaces (4-level paging), kernel heap | **done** (kernel still identity-mapped; higher-half move later) |
| 3 | GDT/TSS/IDT, PIT timer, preemptive round-robin scheduler, ring 3, `int 0x80` syscalls, ELF loader | **done** (single CPU, PIC not APIC) |
| 4 | Message-passing IPC (Binder-style), capability table | next |
| 5 | VFS + ramdisk | |
| 6 | Drivers: PS/2 kbd/mouse, PCI, e1000 / virtio-net, framebuffer + compositor | |
| 7 | Net service: ARP, IPv4, UDP, DHCP, DNS, TCP, IPv6; TLS | |
| 8 | SDK v0 + in-kernel `.qar` verifier + sample apps | **started** (SDK, libc subset, verifier done; sockets, graphics API, `pkgd`, `qpm` not yet) |
| 9 | Browser | |
| 10 | Bluetooth HCI; Wi-Fi interface and stubs | |

Notes
* Microkernel direction: kernel keeps scheduling, memory, IPC and capabilities; drivers,
  VFS, network and `pkgd` become user-space services from Phase 4 onward.
* Wi-Fi cannot be tested in QEMU (it emulates wired NICs). Real Wi-Fi needs hardware and firmware.
* HTTPS (needed by most of the web) needs TLS 1.2/1.3: plan to port an existing library such as BearSSL.
