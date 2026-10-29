/* QAR-OS system call ABI (shared by the kernel and the SDK). Invoke with `int 0x80`:
 *   rax = number, rdi/rsi/rdx = args 1..3, return value in rax (negative = -errno). */
#ifndef QAR_OS_ABI_H
#define QAR_OS_ABI_H
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
#endif
