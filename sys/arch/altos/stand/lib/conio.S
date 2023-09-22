/*	$NetBSD: conio.S,v 1.7 2011/06/16 13:27:59 joerg Exp $	*/

/* PC console handling
  originally from: FreeBSD:sys/i386/boot/netboot/start2.S
 */

#include <machine/asm.h>

	.text

/**************************************************************************
int getchar(void)
**************************************************************************/
ENTRY(getchar)
	pushl %ebx
	movl $0x101, %ebx
	int $0x28
	and $0xff, %eax
	popl %ebx
	ret

/**************************************************************************
void putchar(int c)
**************************************************************************/
ENTRY(putchar)
	movl 4(%esp), %eax
	pushl %ebx
	movl $0x102, %ebx
	int $0x28
	cmp $0x0a, %eax
	jne no_cr
	movl $0x0d, %eax
	int $0x28
no_cr:
	pop %ebx
	ret


/**************************************************************************
int iskey() - Check for keyboard input
**************************************************************************/
ENTRY(iskey)
	pushl %ebx
	movl $0x0b, %ebx
	int $0x28
	and $0xff, %eax
	popl %ebx
	ret
