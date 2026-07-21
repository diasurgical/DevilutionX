/*
 * The m68k-amigaos hard-float libgcc multilib omits the software
 * double-precision comparison routine `__cmpdf2` (doubles are compared in
 * hardware), but its software extended-precision comparison routines
 * (xfpgnulib `__cmpxf2`, `__eqxf2`, ...) still call it.
 *
 * libstdc++'s <format> machinery references the extended-precision routines
 * through its `long double` code paths, so linking fails with an undefined
 * `__cmpdf2` even though it is never actually called.
 */
int __cmpdf2(double a, double b);

int __cmpdf2(double a, double b)
{
	if (a < b) return -1;
	if (a > b) return 1;
	return 0;
}
