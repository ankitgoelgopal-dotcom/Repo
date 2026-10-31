/* Hardware square root (correctly rounded by the CPU). */
double sqrt(double x)  { double r;  __asm__("sqrtsd %1,%0" : "=x"(r) : "x"(x)); return r; }
float  sqrtf(float x)  { float r;   __asm__("sqrtss %1,%0" : "=x"(r) : "x"(x)); return r; }
