# QAR-OS window system

A software-rendered window system: framebuffer, compositor, window manager, PS/2 keyboard and mouse, and a window API for apps.
Everything here is tested in CI: QEMU boots the ISO, a Python driver injects key presses and mouse clicks through QMP, and the
screenshots are checked pixel by pixel (`tests/gui_driver.py`, screenshots are uploaded as the `gui-screenshots` artifact).

## Architecture
* **Framebuffer.** The kernel asks GRUB (Multiboot2 framebuffer tag) for 1024x768, 32-bit xRGB. If the machine cannot provide it
  the window system stays off and the OS keeps its text console. The framebuffer is mapped uncached; all drawing goes to a
  back buffer in RAM and only the damaged rectangle is copied to the screen.
* **Compositor** (`kernel/gui.c`). A kernel thread (preemptible like any task) draws the desktop, every window back to front,
  and the mouse cursor, only inside the dirty rectangle. IRQ handlers never draw; they only queue raw input events.
* **Window manager.** Title bar, border, close button, focus (title bar turns orange), raise on click, drag by the title bar,
  Alt+Tab. Windows keep their pixels in a kernel-owned surface: apps copy pixels in with `os_win_present`, so a crashing or
  malicious app cannot touch the screen, other windows or kernel memory. When a process exits or faults, its windows are removed.
* **Log window.** The kernel log (and everything apps print) is shown in a window titled "QAR-OS log", so the screen is never
  blank while booting. It keeps the last 28 lines; the serial port has the full log.
* **Input.** PS/2 controller, keyboard (US layout, Shift, Caps Lock, Ctrl, Alt, arrows, F1-F12) and mouse (wheel when the
  emulator provides an IntelliMouse). Keyboard events go to the focused window, mouse events to the window under the cursor
  (or the one that was pressed, while a button is held).

## App API (`os_api.h`; the manifest must list the `gui.window` capability)
```c
struct qar_screen s;  os_screen_info(&s);                       // 1024 x 768
struct qar_win_create wc = { x, y, w, h, 0, "Title" };           // x,y = content position; x = -1: auto place
int win = os_win_create(&wc);                                    // window id (<0: error)
os_win_present(win, pixels, stride_bytes, NULL /* or a struct qar_rect */);   // copy xRGB8888 pixels into the window
struct qar_event e; while (os_win_poll(&e, 500) > 0) { ... }     // 1 event, 0 timeout, <0 error / no windows
os_win_title(win, "new title");  os_win_destroy(win);
```
Events (`struct qar_event { type, win, x, y, a, b }`): `QAR_EV_KEY_DOWN/UP` (a = key code, b = character, x = modifiers),
`QAR_EV_MOUSE_MOVE/DOWN/UP` (x,y inside the window content; a = button), `QAR_EV_WHEEL` (a = +up/-down), `QAR_EV_CLOSE`
(the user clicked the close button: the app decides what to do, normally `os_win_destroy` and exit), `QAR_EV_FOCUS`.
`qar_gfx.h` has small drawing helpers (`qar_fill`, `qar_rect`, `qar_text` with the built-in 8x8 font, `qar_blit`).
A complete example is `sdk/examples/guidemo/main.c`.

## Trying it
* **QEMU on a PC:** `qemu-system-x86_64 -cdrom qaros.iso -m 256M -serial stdio` (default VGA, PS/2 mouse and keyboard).
  Click the "Click me" buttons, type, drag the title bars, press Alt+Tab, click the red X.
* **Limbo on Android:** use the default standard VGA, keep the keyboard and mouse as PS/2. If the pointer does not move,
  set the mouse option to PS/2 (a USB tablet or touch input is not supported yet).

## Limits (honest list)
* PS/2 input only. No USB HID, no touch, no absolute-pointer tablet. US keyboard layout, no IME, no clipboard.
* One fixed resolution (1024x768 requested; other sizes are used as given if the bootloader grants them, 32-bit xRGB only).
* Windows cannot be resized, minimized or maximized; the title bar and cursor are drawn by the kernel (no themes).
* Software rendering only, no vsync; apps copy pixels per update (no shared-memory surfaces yet, so very large windows
  redraw slower than they could). Frames are produced at most 100 times a second.
* Text uses the built-in 8x8 bitmap font (ASCII only). Real fonts need FreeType and a file system.
* The window system does not yet isolate apps from reading each other's windows' events by design only (events are per owner);
  there is no focus-stealing protection beyond that.
