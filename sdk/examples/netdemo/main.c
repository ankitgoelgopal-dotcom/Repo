/* Network demo: shows addresses, resolves a name via DNS, then fetches http://10.0.2.2:8000/ over TCP.
 * (10.0.2.2 is the host machine when running under QEMU user-mode networking.) */
#include <stdio.h>
#include <string.h>
#include "os_api.h"

static void ip_str(uint32_t ip, char *o) { snprintf(o, 20, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }

int main(void) {
    uint32_t ni[4]; char a[20], b[20], c[20];
    if (os_netinfo(ni)) { puts("netdemo: network is down"); return 1; }
    ip_str(ni[0], a); ip_str(ni[2], b); ip_str(ni[3], c);
    printf("netdemo: my ip %s, gateway %s, dns %s\n", a, b, c);

    uint32_t ip;
    int r = os_resolve("example.com", &ip);
    if (r == 0) { ip_str(ip, a); printf("netdemo: DNS example.com -> %s\n", a); }
    else printf("netdemo: DNS example.com failed (%d)\n", r);

    int fd = os_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { printf("netdemo: socket failed (%d)\n", fd); return 1; }
    r = os_connect(fd, IP4(10, 0, 2, 2), 8000);
    if (r) { printf("netdemo: connect failed (%d)\n", r); return 1; }
    puts("netdemo: TCP connected to 10.0.2.2:8000");
    const char *req = "GET / HTTP/1.0\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n";
    os_send(fd, req, strlen(req));

    static char buf[2048]; long total = 0, n; char line[64] = ""; int have_line = 0;
    while ((n = os_recv(fd, buf, sizeof buf - 1)) > 0) {
        if (!have_line) {
            size_t i = 0; while (i < (size_t)n && i < sizeof line - 1 && buf[i] != '\r' && buf[i] != '\n') { line[i] = buf[i]; i++; }
            line[i] = 0; have_line = 1;
        }
        total += n;
    }
    printf("netdemo: HTTP status line: %s\n", line);
    printf("netdemo: received %ld bytes total (recv ended with %ld)\n", total, n);
    os_close(fd);
    puts("netdemo: done");
    return 0;
}
