# What a web browser needs from QAR-OS

Honest summary: a browser is a stack. The libraries (HTML, CSS, JS, images, fonts, TLS) are the top of it. Whole engines
(Chromium, WebKit, Gecko, Servo, Ladybird) also need a *POSIX-like operating system* underneath. This file lists what each
level needs and where QAR-OS stands today. Sizes are rough orders of magnitude.

## 1. Engine choices

| Engine | Size | Language | JS engine | Hard OS requirements | Realistic for QAR-OS |
|---|---|---|---|---|---|
| Own renderer on small libs (Lexbor/NetSurf libs + QuickJS) | 100s of KLOC, you own the glue | C | QuickJS | screen, input, fonts, files, sockets | **first target** |
| NetSurf | ~500 KLOC | C | Duktape (partial) | same as above + its framebuffer front end | good fit for static and simple sites |
| litehtml | small | C++ | none | C++ runtime | HTML/CSS only |
| Ladybird (LibWeb/LibJS) | millions of lines | C++20 | LibJS | threads, mmap, sockets, files, C++20 runtime, Skia/Qt-class graphics | later, after the POSIX layer |
| WebKit | millions of lines | C++ | JavaScriptCore | threads, mmap, ICU, cairo/skia, many libs | later |
| Servo | millions of lines | Rust | SpiderMonkey | Rust `std` for the OS, threads, mmap, GPU or software GL | later |
| Gecko (Firefox) | tens of millions of lines | C++/Rust | SpiderMonkey | everything above plus a large dependency tree | very late |
| Chromium | tens of millions of lines | C++ | V8 | threads, futex, shared memory, Unix sockets, mmap/mprotect, multi-process sandbox, Skia, many libs | last |

Static and simple sites (news, Wikipedia, docs) work with the first row. Modern web apps (YouTube, Gmail, Docs, maps) need a
full engine, and the engine needs everything in section 2.

## 2. OS requirements for full engines, and QAR-OS status

| Area | What engines need | QAR-OS today |
|---|---|---|
| Threads | pthreads, mutex/condvar, **futex**, thread-local storage (`%fs`), atomics, SMP scheduling | **missing** (one thread per process, one CPU) |
| Memory | `mmap`/`munmap`/`mprotect`, lazy (demand) paging, guard pages, large 64-bit address space, JIT needs W^X remap | partial: `sbrk` heap only, 256 MB user window |
| Files | VFS, POSIX `open/read/write/stat/readdir`, rename, temp files, `mmap` of files, a real disk file system | **missing** (modules are loaded from the ISO only) |
| IPC | pipes, Unix domain sockets, `socketpair`, shared memory, `eventfd`, fd passing (multi-process browsers) | **missing** (capability IPC is planned) |
| Event loop | `poll`/`epoll`/`select`, non-blocking sockets, timers | **missing** (sockets block with timeouts) |
| Processes | `fork`/`exec` or `posix_spawn`, signals, exit status, process sandboxing | partial: spawn from `.qar`, kill on fault; no signals |
| Network | non-blocking BSD sockets, `getaddrinfo`, IPv6, TLS 1.3, HTTP/2, UDP for QUIC | partial: IPv4 TCP client, UDP, DNS, DHCP, TLS 1.2; **missing**: IPv6, TLS 1.3, TCP listen, out-of-order TCP |
| Time | monotonic + wall clock, timers, time zones | partial: RTC wall clock, uptime; no time zones |
| Graphics | framebuffer, compositor, window system, shared surfaces, vsync; GPU (Vulkan/GL) or a fast software rasterizer (Skia CPU) | **missing** (window-system code is in progress, unpushed) |
| Input | keyboard, mouse, touch, scroll, IME, clipboard | **missing** |
| Fonts/text | FreeType, HarfBuzz (shaping), fontconfig or a font loader, ICU (Unicode, bidi, line breaking) | **missing** |
| Media | image codecs (PNG done; JPEG, WebP, AVIF, GIF), audio/video codecs, audio output | partial: PNG, zlib |
| Compression | gzip/deflate, Brotli, zstd | partial: zlib; **missing**: Brotli, zstd |
| Language runtime | C++17/20 (`libc++`, exceptions, RTTI, `libunwind`), STL; Rust `std` for Servo | **missing** (C only; no C++ runtime) |
| Toolchain | clang/LLVM with a QAR-OS target, `ld.lld`, CMake/GN/Ninja friendly SDK | partial: gcc freestanding wrapper (`qar-cc`) |
| Storage libs | SQLite (cookies, history, IndexedDB), leveldb-like stores | **missing** (needs files) |
| Security | per-site process isolation, sandbox (no filesystem/network for renderers), capability model | partial: signed packages, per-app capabilities (`net.connect`) |
| JS engine | QuickJS/LibJS/V8 (V8 JIT needs `mprotect`; can run jitless) | **QuickJS runs** (64 tests) |

## 3. Suggested order

1. **Screen + input** (window system, keyboard, mouse) — nothing can be shown without this.
2. **File system** (VFS + ramdisk, then a disk FS) and **fonts** (FreeType, then HarfBuzz).
3. **HTML/CSS layer** (Lexbor or NetSurf libs), DOM bindings for QuickJS, image codecs (JPEG, WebP), Brotli, TLS 1.3, HTTP client with redirects/chunked/gzip. => first usable text-and-image browser.
4. **POSIX layer for engines**: threads + futex + TLS, `mmap`, `poll/epoll`, pipes and Unix sockets, non-blocking sockets, SMP.
5. **C++ runtime** (`libc++`, `libunwind`) and a clang target. => LibWeb/WebKit become portable.
6. **Graphics stack** (Skia CPU backend, then GPU), audio, media codecs, IPv6/HTTP2/QUIC.
7. Multi-process sandboxing (site isolation). => Chromium-class engines are conceivable.

Steps 1-3 are weeks of focused work; 4-5 are months; 6-7 are a long-term effort even with a team.
