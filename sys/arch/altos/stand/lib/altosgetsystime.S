#include <machine/asm.h>

/*
returns
	0 sucess
	1 fail
*/

ENTRY(altosgetsystime)
	push %fs
	push %es
	mov $0x40, %ax
	mov %ax, %fs
	push %ds
	pop %es
time_okay_1:
	movw $0x0, %fs:(0)
	mov $0x0a, %esi
	mov $g_altos_time, %edi
	movl $0x0a, %ecx
store_clock_data:
	lodsb %fs:(%esi)
	stosb %es:(%edi)
	inc %esi
	loop store_clock_data
	testb $4, %fs:(6)
	jnz time_okay_1
	orb $0x80, %fs:(0)
	xor %eax, %eax
	movb %fs:(0x3e),%al
	pop %es
	pop %fs
	ret
