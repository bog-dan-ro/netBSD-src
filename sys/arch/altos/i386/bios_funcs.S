#include <machine/asm.h>
#include <machine/specialreg.h>

    .text
/**************************************************************************
int altos_getchar(void)
**************************************************************************/
ENTRY(altos_getchar)
		pushl %ebx

		movl %cr0, %eax
		and	$(~CR0_PG),%eax
		movl %eax,%cr0

        movl $0x101, %ebx
        int $0x28
        and $0xff, %eax
		push %eax
		movl %cr0, %eax
		or $(CR0_PG),%eax
		movl %eax,%cr0
		pop %eax
        popl %ebx
        ret
END(altos_getchar)

/**************************************************************************
void altos_putchar(int c)
**************************************************************************/
ENTRY(altos_putchar)
		movl 4(%esp), %eax
		pushl %ebx

		pushl %eax
		// disable paging
		movl %cr0,%eax
		and	$(~CR0_PG),%eax
		movl %eax,%cr0
		popl %eax

        movl $0x102, %ebx
        int $0x28
        cmp $0x0a, %eax
        jne 1f
        movl $0x0d, %eax
        int $0x28
1:
		movl %cr0,%eax
		or $(CR0_PG),%eax
		movl %eax,%cr0
        pop %ebx
        ret
END(altos_putchar)
