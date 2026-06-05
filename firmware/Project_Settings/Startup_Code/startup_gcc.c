#include "derivative.h"

extern unsigned long _sdata[], _edata[], ___ROM_AT[];
extern unsigned long __START_BSS[], __END_BSS[];
extern void __init_hardware(void);
extern int main(void);

void __thumb_startup(void)
{
    unsigned long *src, *dst;

    __init_hardware();

    src = ___ROM_AT;
    dst = _sdata;
    while (dst < _edata) {
        *dst++ = *src++;
    }

    dst = __START_BSS;
    while (dst < __END_BSS) {
        *dst++ = 0;
    }

    main();

    while (1);
}
