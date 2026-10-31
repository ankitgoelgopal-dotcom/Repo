/* QAR-OS stdio: printf family. Integer/string formatting is written for this SDK; the floating-point
 * conversions (%f %e %g %a) use musl's exact algorithm (arbitrary-precision decimal expansion), so
 * output matches glibc/musl digit-for-digit. See third_party/musl-math/COPYRIGHT for musl's license. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "os_api.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ALT_FORM   (1U << ('#' - ' '))
#define ZERO_PAD   (1U << ('0' - ' '))
#define LEFT_ADJ   (1U << ('-' - ' '))
#define PAD_POS    (1U << (' ' - ' '))
#define MARK_POS   (1U << ('+' - ' '))
#define GROUPED    (1U << ('\'' - ' '))

/* Output sink: either a bounded memory buffer (snprintf) or stdout with 512-byte chunked flushing (printf). */
struct out { char *buf; size_t cap, len; int fd; size_t total; };
static void out_flush(struct out *o) { if (o->fd >= 0 && o->len) { os_write(o->fd, o->buf, o->len); o->len = 0; } }
static void put1(struct out *o, char c) {
    o->total++;
    if (o->fd >= 0) { o->buf[o->len++] = c; if (o->len == o->cap) out_flush(o); }
    else if (o->len + 1 < o->cap) o->buf[o->len++] = c;
}
static void out(struct out *o, const char *s, size_t l) { while (l--) put1(o, *s++); }
static void pad(struct out *o, char c, int w, int l, int fl) {
    if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w) return;
    for (l = w - l; l > 0; l--) put1(o, c);
}

/* ---- floating-point conversions: adapted from musl libc src/stdio/vfprintf.c (MIT) ---- */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wparentheses"
static const char xdigits[16] = { "0123456789ABCDEF" };
static char *fmt_u(uintmax_t x, char *s) {
    unsigned long y;
    for (   ; x > ULONG_MAX; x /= 10) *--s = '0' + x % 10;
    for (y = x;       y >= 10; y /= 10) *--s = '0' + y % 10;
    if (y) *--s = '0' + y;
    return s;
}
static int fmt_fp(struct out *f, double y, int w, int p, int fl, int t)
{
	int max_mant_dig = DBL_MANT_DIG;
	int max_exp = DBL_MAX_EXP;
	/* One slot for 29 bits left of radix point, a slot for every 29-21=8
	 * bits right of the radix point, and one final zero slot. */
	int max_mant_slots = 1 + (max_mant_dig-29+7)/8 + 1;
	int max_exp_slots = (max_exp+max_mant_dig+28+8)/9;
	int bufsize = max_mant_slots + max_exp_slots;
	uint32_t big[bufsize];
	uint32_t *a, *d, *r, *z;
	int e2=0, e, i, j, l;
	char buf[9+DBL_MANT_DIG/4], *s;
	const char *prefix="-0X+0X 0X-0x+0x 0x";
	int pl;
	char ebuf0[3*sizeof(int)], *ebuf=&ebuf0[3*sizeof(int)], *estr;

	pl=1;
	if (signbit(y)) {
		y=-y;
	} else if (fl & MARK_POS) {
		prefix+=3;
	} else if (fl & PAD_POS) {
		prefix+=6;
	} else prefix++, pl=0;

	if (!isfinite(y)) {
		char *s = (t&32)?"inf":"INF";
		if (y!=y) s=(t&32)?"nan":"NAN";
		pad(f, ' ', w, 3+pl, fl&~ZERO_PAD);
		out(f, prefix, pl);
		out(f, s, 3);
		pad(f, ' ', w, 3+pl, fl^LEFT_ADJ);
		return MAX(w, 3+pl);
	}

	y = frexp(y, &e2) * 2;
	if (y) e2--;

	if ((t|32)=='a') {
		if (t&32) prefix += 9;
		pl += 2;

		if (p>=0 && p<(DBL_MANT_DIG-1+3)/4) {
			double round = scalbn(1, DBL_MANT_DIG-1-(p*4));
			if (*prefix=='-') {
				y=-y;
				y-=round;
				y+=round;
				y=-y;
			} else {
				y+=round;
				y-=round;
			}
		}

		estr=fmt_u(e2<0 ? -e2 : e2, ebuf);
		if (estr==ebuf) *--estr='0';
		*--estr = (e2<0 ? '-' : '+');
		*--estr = t+('p'-'a');

		s=buf;
		do {
			int x=y;
			*s++=xdigits[x]|(t&32);
			y=16*(y-x);
			if (s-buf==1 && (y||p>0||(fl&ALT_FORM))) *s++='.';
		} while (y);

		if (p > INT_MAX-2-(ebuf-estr)-pl)
			return -1;
		if (p && s-buf-2 < p)
			l = (p+2) + (ebuf-estr);
		else
			l = (s-buf) + (ebuf-estr);

		pad(f, ' ', w, pl+l, fl);
		out(f, prefix, pl);
		pad(f, '0', w, pl+l, fl^ZERO_PAD);
		out(f, buf, s-buf);
		pad(f, '0', l-(ebuf-estr)-(s-buf), 0, 0);
		out(f, estr, ebuf-estr);
		pad(f, ' ', w, pl+l, fl^LEFT_ADJ);
		return MAX(w, pl+l);
	}
	if (p<0) p=6;

	if (y) y *= 0x1p28, e2-=28;

	if (e2<0) a=r=z=big;
	else a=r=z=big+sizeof(big)/sizeof(*big) - max_mant_slots - 1;

	do {
		*z = y;
		y = 1000000000*(y-*z++);
	} while (y);

	while (e2>0) {
		uint32_t carry=0;
		int sh=MIN(29,e2);
		for (d=z-1; d>=a; d--) {
			uint64_t x = ((uint64_t)*d<<sh)+carry;
			*d = x % 1000000000;
			carry = x / 1000000000;
		}
		if (carry) *--a = carry;
		while (z>a && !z[-1]) z--;
		e2-=sh;
	}
	while (e2<0) {
		uint32_t carry=0, *b;
		int sh=MIN(9,-e2), need=1+(p+DBL_MANT_DIG/3U+8)/9;
		for (d=a; d<z; d++) {
			uint32_t rm = *d & (1<<sh)-1;
			*d = (*d>>sh) + carry;
			carry = (1000000000>>sh) * rm;
		}
		if (!*a) a++;
		if (carry) *z++ = carry;
		/* Avoid (slow!) computation past requested precision */
		b = (t|32)=='f' ? r : a;
		if (z-b > need) z = b+need;
		e2+=sh;
	}

	if (a<z) for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
	else e=0;

	/* Perform rounding: j is precision after the radix (possibly neg) */
	j = p - ((t|32)!='f')*e - ((t|32)=='g' && p);
	if (j < 9*(z-r-1)) {
		uint32_t x;
		/* We avoid C's broken division of negative numbers */
		d = r + 1 + ((j+9*DBL_MAX_EXP)/9 - DBL_MAX_EXP);
		j += 9*DBL_MAX_EXP;
		j %= 9;
		for (i=10, j++; j<9; i*=10, j++);
		x = *d % i;
		/* Are there any significant digits past j? */
		if (x || d+1!=z) {
			double round = 2/DBL_EPSILON;
			double small;
			if ((*d/i & 1) || (i==1000000000 && d>a && (d[-1]&1)))
				round += 2;
			if (x<i/2) small=0x0.8p0;
			else if (x==i/2 && d+1==z) small=0x1.0p0;
			else small=0x1.8p0;
			if (pl && *prefix=='-') round*=-1, small*=-1;
			*d -= x;
			/* Decide whether to round by probing round+small */
			if (round+small != round) {
				*d = *d + i;
				while (*d > 999999999) {
					*d--=0;
					if (d<a) *--a=0;
					(*d)++;
				}
				for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
			}
		}
		if (z>d+1) z=d+1;
	}
	for (; z>a && !z[-1]; z--);
	
	if ((t|32)=='g') {
		if (!p) p++;
		if (p>e && e>=-4) {
			t--;
			p-=e+1;
		} else {
			t-=2;
			p--;
		}
		if (!(fl&ALT_FORM)) {
			/* Count trailing zeros in last place */
			if (z>a && z[-1]) for (i=10, j=0; z[-1]%i==0; i*=10, j++);
			else j=9;
			if ((t|32)=='f')
				p = MIN(p,MAX(0,9*(z-r-1)-j));
			else
				p = MIN(p,MAX(0,9*(z-r-1)+e-j));
		}
	}
	if (p > INT_MAX-1-(p || (fl&ALT_FORM)))
		return -1;
	l = 1 + p + (p || (fl&ALT_FORM));
	if ((t|32)=='f') {
		if (e > INT_MAX-l) return -1;
		if (e>0) l+=e;
	} else {
		estr=fmt_u(e<0 ? -e : e, ebuf);
		while(ebuf-estr<2) *--estr='0';
		*--estr = (e<0 ? '-' : '+');
		*--estr = t;
		if (ebuf-estr > INT_MAX-l) return -1;
		l += ebuf-estr;
	}

	if (l > INT_MAX-pl) return -1;
	pad(f, ' ', w, pl+l, fl);
	out(f, prefix, pl);
	pad(f, '0', w, pl+l, fl^ZERO_PAD);

	if ((t|32)=='f') {
		if (a>r) a=r;
		for (d=a; d<=r; d++) {
			char *s = fmt_u(*d, buf+9);
			if (d!=a) while (s>buf) *--s='0';
			else if (s==buf+9) *--s='0';
			out(f, s, buf+9-s);
		}
		if (p || (fl&ALT_FORM)) out(f, ".", 1);
		for (; d<z && p>0; d++, p-=9) {
			char *s = fmt_u(*d, buf+9);
			while (s>buf) *--s='0';
			out(f, s, MIN(9,p));
		}
		pad(f, '0', p+9, 9, 0);
	} else {
		if (z<=a) z=a+1;
		for (d=a; d<z && p>=0; d++) {
			char *s = fmt_u(*d, buf+9);
			if (s==buf+9) *--s='0';
			if (d!=a) while (s>buf) *--s='0';
			else {
				out(f, s++, 1);
				if (p>0||(fl&ALT_FORM)) out(f, ".", 1);
			}
			out(f, s, MIN(buf+9-s, p));
			p -= buf+9-s;
		}
		pad(f, '0', p+18, 18, 0);
		out(f, estr, ebuf-estr);
	}

	pad(f, ' ', w, pl+l, fl^LEFT_ADJ);

	return MAX(w, pl+l);
}

#pragma GCC diagnostic pop

static const char xdig[] = "0123456789ABCDEF";

/* integer conversions: d i u o x X p */
static void fmt_int(struct out *o, unsigned long long v, int neg, int base, int upper, int w, int p, int fl, int alt_prefix) {
    char buf[72], *s = buf + sizeof buf;
    if (!v) *--s = '0';
    else { unsigned long long x = v; while (x) { *--s = upper ? xdig[x % base] : (char)(xdig[x % base] | 32); x /= base; } }
    int nd = (int)(buf + sizeof buf - s);
    if (p == 0 && v == 0) nd = 0, s = buf + sizeof buf;                 /* "%.0d" of 0 prints nothing */
    int zeros = p > nd ? p - nd : 0;
    char pre[3]; int pl = 0;
    if (neg) pre[pl++] = '-'; else if (fl & MARK_POS) pre[pl++] = '+'; else if (fl & PAD_POS) pre[pl++] = ' ';
    if (alt_prefix && v) { if (base == 16) { pre[pl++] = '0'; pre[pl++] = upper ? 'X' : 'x'; } }
    if (alt_prefix && base == 8 && zeros == 0 && (nd == 0 || s[0] != '0')) zeros = 1;
    int len = pl + zeros + nd;
    if (p < 0 && (fl & ZERO_PAD) && !(fl & LEFT_ADJ) && w > len) zeros += w - len, len = w;
    pad(o, ' ', w, len, fl);
    out(o, pre, pl);
    for (int i = 0; i < zeros; i++) put1(o, '0');
    out(o, s, nd);
    pad(o, ' ', w, len, fl ^ LEFT_ADJ);
}


static void printf_core(struct out *o, const char *f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') { put1(o, *f); continue; }
        f++;
        if (*f == '%') { put1(o, '%'); continue; }
        int fl = 0, w = 0, p = -1;
        while (*f == '-' || *f == '+' || *f == ' ' || *f == '#' || *f == '0' || *f == '\'') fl |= 1U << (*f++ - ' ');
        if (*f == '*') { w = va_arg(ap, int); f++; if (w < 0) { fl |= LEFT_ADJ; w = -w; } }
        else while (*f >= '0' && *f <= '9') w = w * 10 + (*f++ - '0');
        if (*f == '.') {
            f++;
            if (*f == '*') { p = va_arg(ap, int); f++; if (p < 0) p = -1; }
            else { p = 0; while (*f >= '0' && *f <= '9') p = p * 10 + (*f++ - '0'); }
        }
        int lm = 0;                                                    /* 0 int, 1 long, 2 long long, -1 short, -2 char */
        for (;; f++) {
            if (*f == 'l') lm = lm == 1 ? 2 : 1;
            else if (*f == 'h') lm = lm == -1 ? -2 : -1;
            else if (*f == 'z' || *f == 'j' || *f == 't') lm = 2;
            else if (*f == 'L' || *f == 'q') lm = 2;
            else break;
        }
        int t = *f;
        switch (t) {
        case 'c': { char c = (char)va_arg(ap, int); pad(o, ' ', w, 1, fl); put1(o, c); pad(o, ' ', w, 1, fl ^ LEFT_ADJ); break; }
        case 's': {
            const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            int l = 0; while ((p < 0 || l < p) && s[l]) l++;
            pad(o, ' ', w, l, fl); out(o, s, (size_t)l); pad(o, ' ', w, l, fl ^ LEFT_ADJ); break; }
        case 'd': case 'i': {
            long long v = lm >= 2 ? va_arg(ap, long long) : lm == 1 ? va_arg(ap, long) : va_arg(ap, int);
            if (lm == -1) v = (short)v; else if (lm == -2) v = (signed char)v;
            int neg = v < 0; unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
            fmt_int(o, u, neg, 10, 0, w, p, fl, 0); break; }
        case 'u': case 'o': case 'x': case 'X': {
            unsigned long long v = lm >= 2 ? va_arg(ap, unsigned long long) : lm == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            if (lm == -1) v = (unsigned short)v; else if (lm == -2) v = (unsigned char)v;
            fmt_int(o, v, 0, t == 'u' ? 10 : t == 'o' ? 8 : 16, t == 'X', w, p, fl & ~(MARK_POS | PAD_POS), t != 'u' && (fl & ALT_FORM)); break; }
        case 'p': { unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            if (!v) { pad(o, ' ', w, 5, fl); out(o, "(nil)", 5); pad(o, ' ', w, 5, fl ^ LEFT_ADJ); }
            else fmt_int(o, v, 0, 16, 0, w, p, fl & ~(MARK_POS | PAD_POS), 1);
            break; }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
            fmt_fp(o, va_arg(ap, double), w, p, fl, t); break;
        case 'n': (void)va_arg(ap, int *); break;
        case 0: return;
        default: put1(o, '%'); put1(o, (char)t); break;
        }
    }
}

int vsnprintf(char *buf, size_t cap, const char *f, va_list ap) {
    struct out o = { buf, cap, 0, -1, 0 };
    printf_core(&o, f, ap);
    if (cap) buf[o.len] = 0;
    return (int)o.total;
}
int snprintf(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(b, n, f, ap); va_end(ap); return r; }
int vprintf(const char *f, va_list ap) {
    char buf[512]; struct out o = { buf, sizeof buf, 0, 1, 0 };
    printf_core(&o, f, ap); out_flush(&o); return (int)o.total;
}
int printf(const char *f, ...) { va_list ap; va_start(ap, f); int r = vprintf(f, ap); va_end(ap); return r; }
int puts(const char *s) { os_write(1, s, strlen(s)); os_write(1, "\n", 1); return 0; }
int putchar(int c) { char ch = (char)c; os_write(1, &ch, 1); return c; }
