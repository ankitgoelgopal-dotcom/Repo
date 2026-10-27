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
/* Reserved for the network phase; the kernel returns -ENOSYS until the stack exists. */
#define SYS_SOCKET    16
#define SYS_CONNECT   17
#define SYS_SEND      18
#define SYS_RECV      19
#define SYS_CLOSE     20
#define QAR_EPERM   1
#define QAR_EFAULT 14
#define QAR_EINVAL 22
#define QAR_ENOSYS 38
#endif
