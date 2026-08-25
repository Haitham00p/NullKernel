/* MIXED project — C main calling n64a-assembled routines.
 *
 * Built automatically by the self-host builder: every .C and .ASM inside
 * /BIN/MIXED is compiled/assembled into objects and linked into
 * /BIN/MIXED.EXE by nld.
 */
#include <stdio.h>

int asm_add(int a, int b);
int asm_mul(int a, int b);
void asm_banner(void);

int main(void)
{
    int s = asm_add(20, 22);
    int p = asm_mul(6, 7);

    printf("asm_add(20, 22) = %d\n", s);
    printf("asm_mul(6, 7)   = %d\n", p);
    asm_banner();
    return 0;
}
