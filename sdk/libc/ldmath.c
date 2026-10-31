/* Minimal long double (x87) helpers needed by strtod's exact scanner. */
long double fabsl(long double x) { return __builtin_fabsl(x); }
long double copysignl(long double x, long double y) { return __builtin_copysignl(x, y); }
