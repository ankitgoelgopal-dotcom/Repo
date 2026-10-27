# QAR-OS roadmap

Each phase must boot and pass `scripts/test_qemu.sh` in CI before the next begins.

| Phase | Deliverable | Status |
|------:|-------------|--------|
| 1 | Multiboot2 boot, long mode, serial + VGA console, memory map, CI + QEMU boot test, `.qar` spec + tooling | **done** |
| 2 | Physical frame allocator, 4-level paging, higher-half kernel, kernel heap | next |
| 3 | GDT/IDT/TSS, exceptions, APIC timer, preemptive scheduler, ring 3, syscalls, ELF loader | |
| 4 | Message-passing IPC (Binder-style), capability table, handle passing | |
| 5 | VFS + ramdisk (initrd carries system `.qar` packages) | |
| 6 | Drivers: PS/2 kbd/mouse, PCI enumeration, e1000 / virtio-net, framebuffer + compositor | |
| 7 | Net service: ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP, then IPv6; TLS library | |
| 8 | SDK (`os_api.h`, libc subset, sockets, gfx), in-OS `.qar` verifier, `pkgd`, loader | |
| 9 | Browser: URL fetch over sockets, HTML text renderer, windowing | |
| 10 | Bluetooth HCI layer; Wi-Fi driver interface and stubs | |

Notes
* Microkernel direction: kernel keeps scheduling, memory, IPC and capabilities; drivers,
  VFS, network and `pkgd` become user-space services from Phase 4 onward.
* Wi-Fi cannot be tested in QEMU (it emulates wired NICs). Real Wi-Fi needs hardware and firmware.
* HTTPS (needed by most of the web) needs TLS 1.2/1.3: plan to port an existing library such as BearSSL.
