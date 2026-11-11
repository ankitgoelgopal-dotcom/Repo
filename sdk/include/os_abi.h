/* QAR-OS system call ABI (shared by the kernel and the SDK). Invoke with `int 0x80`:
 *   rax = number, rdi/rsi/rdx = args 1..3, return value in rax (negative = -errno). */
#ifndef QAR_OS_ABI_H
#define QAR_OS_ABI_H
#include <stdint.h>
#define SYS_EXIT      0   /* exit(code)                         */
#define SYS_WRITE     1   /* write(fd, buf, len) -> bytes       */
#define SYS_YIELD     2   /* yield()                            */
#define SYS_GETPID    3   /* getpid() -> pid                    */
#define SYS_SLEEP_MS  4   /* sleep_ms(ms)                       */
#define SYS_UPTIME_MS 5   /* uptime_ms() -> ms since boot       */
#define SYS_GETRANDOM 23  /* getrandom(buf, len<=256) -> len    */
#define SYS_TIME_UNIX 24  /* time_unix() -> seconds since 1970 (CMOS RTC, UTC) */
/* Networking: needs the "net.connect" capability in the app manifest, else -EPERM (except NETINFO).
 * IPv4 addresses are host-order uint32: a<<24 | b<<16 | c<<8 | d. */
#define SYS_SOCKET    16  /* socket(AF_INET=2, SOCK_STREAM=1 | SOCK_DGRAM=2, 0) -> fd  */
#define SYS_CONNECT   17  /* connect(fd, ip, port)                                     */
#define SYS_SEND      18  /* send(fd, buf, len) -> bytes                               */
#define SYS_RECV      19  /* recv(fd, buf, len) -> bytes, 0 = peer closed              */
#define SYS_CLOSE     20  /* close(fd)                                                 */
#define SYS_RESOLVE   21  /* resolve(name, uint32_t *ip_out) via DNS -> 0              */
#define SYS_NETINFO   22  /* netinfo(uint32_t out[4]) = {ip, netmask, gateway, dns}    */
/* System services */
#define SYS_SBRK      25  /* sbrk(increment >= 0) -> previous break; grows the heap    */
#define QAR_EPERM        1
#define QAR_ENODEV      19
#define QAR_ENOENT       2
#define QAR_EIO          5
#define QAR_EBADF        9
#define QAR_ENOMEM      12
#define QAR_EFAULT      14
#define QAR_EINVAL      22
#define QAR_EMFILE      24
#define QAR_EPIPE       32
#define QAR_ENOSYS      38
#define QAR_ENETDOWN   100
#define QAR_ENETUNREACH 101
#define QAR_ECONNRESET 104
#define QAR_EISCONN    106
#define QAR_ENOTCONN   107
#define QAR_ETIMEDOUT  110
#define QAR_ECONNREFUSED 111
#define QAR_EHOSTUNREACH 113

/* ---- Window system (needs the "gui.window" capability in the manifest) ---------------------------------------
 * Apps keep their own pixel buffer (xRGB8888) and copy changed parts into their window with WIN_PRESENT.
 * Events (keyboard, mouse, close) are read with WIN_POLL. See docs/WINDOW-SYSTEM.md.                          */
#define SYS_SCREEN_INFO 32  /* screen_info(struct qar_screen *)                                         */
#define SYS_WIN_CREATE  33  /* win_create(struct qar_win_create *) -> window id                         */
#define SYS_WIN_DESTROY 34  /* win_destroy(id)                                                          */
#define SYS_WIN_PRESENT 35  /* win_present(id, pixels, stride_bytes, struct qar_rect *or NULL) -> 0     */
#define SYS_WIN_POLL    36  /* win_poll(struct qar_event *, timeout_ms; <0 = wait forever) -> 1 / 0     */
#define SYS_WIN_TITLE   37  /* win_title(id, const char *)                                              */
struct qar_screen { uint32_t w, h, bpp, pitch; };
struct qar_win_create { int32_t x, y, w, h; uint32_t flags; char title[48]; };   /* x,y = content position; x = -1: auto place */
struct qar_rect { int32_t x, y, w, h; };
struct qar_event { uint32_t type, win; int32_t x, y; uint32_t a, b; };
enum { QAR_EV_NONE = 0, QAR_EV_KEY_DOWN, QAR_EV_KEY_UP, QAR_EV_MOUSE_MOVE, QAR_EV_MOUSE_DOWN, QAR_EV_MOUSE_UP,
       QAR_EV_WHEEL, QAR_EV_CLOSE, QAR_EV_FOCUS };
/* KEY_*: a = key code (printable keys: lower-case ASCII), b = character produced (0 for none / with Ctrl or Alt), x = modifiers.
 * MOUSE_*: x,y = position inside the window content; MOUSE_DOWN/UP: a = button (1 left, 2 right, 4 middle);
 * MOUSE_MOVE: a = buttons held; WHEEL: a = int32 delta, positive = wheel up. FOCUS: a = 1 gained, 0 lost. */
#define QAR_MOD_SHIFT 1
#define QAR_MOD_CTRL  2
#define QAR_MOD_ALT   4
#define QAR_MOD_CAPS  8
#define QAR_KEY_BACKSPACE 8
#define QAR_KEY_TAB       9
#define QAR_KEY_ENTER     13
#define QAR_KEY_ESC       27
#define QAR_KEY_SPACE     32
#define QAR_KEY_UP        0x101
#define QAR_KEY_DOWN      0x102
#define QAR_KEY_LEFT      0x103
#define QAR_KEY_RIGHT     0x104
#define QAR_KEY_HOME      0x105
#define QAR_KEY_END       0x106
#define QAR_KEY_PGUP      0x107
#define QAR_KEY_PGDN      0x108
#define QAR_KEY_INSERT    0x109
#define QAR_KEY_DELETE    0x10A
#define QAR_KEY_F1        0x111   /* F1..F12 = 0x111..0x11C */
#define QAR_KEY_SHIFT     0x120
#define QAR_KEY_CTRL      0x121
#define QAR_KEY_ALT       0x122
#define QAR_KEY_CAPS      0x123

#endif
