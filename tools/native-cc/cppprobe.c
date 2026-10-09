#include <stdio.h>

#define BUMP(x) ((x)+1)
#define TWICE(x) ((x)+(x))
#define CPPVALUE 6
#define MM_STACKSEL 0x00d0
#define MM_STACKBASE 0x00d2
#define COMMON_NAME_one 17
#define COMMON_NAME_two 19
#undef COMMON_NAME_one

#if MM_STACKSEL != 0x00d0 || MM_STACKBASE != 0x00d2
invalid_mmu_macro_expansion
#endif
#if defined(COMMON_NAME_one) || !defined(COMMON_NAME_two)
invalid_long_macro_undef
#endif
#if -2 != (0-2) || ~0 != (0-1) || !0 != 1 || !1 != 0
invalid_unary_expression
#endif
#if 0x00d0 != 208 || 0x00D2 != 210 || 0xAf != 175
invalid_hex_expression
#endif
#if 0
#error inactive error must not fail preprocessing
#endif
#ifdef CPPFAIL
#error active error must fail preprocessing
#endif

#if CPPVALUE != 6
invalid_macro_expansion
#endif

main()
{
	FILE *p;
	p = stdout;
	if (fileno(p) != 1 || TWICE(BUMP(2)) != 6)
		return(1);
	if ((char)254 != -2)
		return(2);
	if (MM_STACKSEL != 0x00d0 || MM_STACKBASE != 0x00d2 || COMMON_NAME_two != 19)
		return(3);
	putc('X', p);
	putc('\n', p);
	puts("NATIVE CPP OK");
	return(0);
}
