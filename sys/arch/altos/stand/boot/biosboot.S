/*	$NetBSD: biosboot.S,v 1.8 2011/01/05 23:13:01 jakllsch Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <sys/bootblock.h>

	.text
ENTRY(boot_start)
	cld
	jmp boot_start_1
ENTRY(boot_magic)
	// altos magic number
	.byte 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
.balign	4
#include <boot_params.S>
.balign	4

boot_start_1:
	// test the shit
	movl %cs, %eax
	cmp $0x168, %eax
	jz boot_start_2
	mov $0xf000, %esp
	movw %cs, %ax
	subw $8, %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es

	movl $8, %ecx
	movl %ecx, %fs

	// init kernel segments
	// setup data (SS, DS, ES, FS, GS) selector
	movl $0x190, %ecx
	movl $0, %eax
	movw %ax, %fs:0x2(%ecx)
	shrl $0x10, %eax
	movb %al, %fs:0x4(%ecx)
	movb %ah, %fs:0x7(%ecx)
	movl $0xfffff, %eax
	movw %ax, %fs:(%ecx)
	shrl $0x10, %eax
	orb $0xc0, %al
	movb %al, %fs:0x6(%ecx)
	movb $0x92, %fs:0x5(%ecx)

	// setup CS selector
	movl $0x198, %ecx
	movl $0, %eax
	movw %ax, %fs:0x2(%ecx)
	shrl $0x10, %eax
	movb %al, %fs:0x4(%ecx)
	movb %ah, %fs:0x7(%ecx)
	movl $0xfffff, %eax // the kernel shall have less than 2MB
	movw %ax, %fs:(%ecx)
	shrl $0x10, %eax
	orb $0xc0, %al
	movb %al, %fs:0x6(%ecx)
	movb $0x9a, %fs:0x5(%ecx)

	// init bootloader segment
	// setup data (SS, DS, ES, FS, GS) selector
	movl $0x160, %ecx
	movl $PHY_ADDR, %eax
	movw %ax, %fs:0x2(%ecx)
	shrl $0x10, %eax
	movb %al, %fs:0x4(%ecx)
	movb %ah, %fs:0x7(%ecx)
	movl $0xfffff, %eax
	movw %ax, %fs:(%ecx)
	shrl $0x10, %eax
	orb $0x40, %al
	movb %al, %fs:0x6(%ecx)
	movb $0x92, %fs:0x5(%ecx)

	// setup CS selector
	movl $0x168, %ecx
	movl $PHY_ADDR, %eax
	movw %ax, %fs:0x2(%ecx)
	shrl $0x10, %eax
	movb %al, %fs:0x4(%ecx)
	movb %ah, %fs:0x7(%ecx)
	movl $__bss_start, %eax
	movw %ax, %fs:(%ecx)
	shrl $0x10, %eax
	orb $0x40, %al
	movb %al, %fs:0x6(%ecx)
	movb $0x9a, %fs:0x5(%ecx)

	movw %cs, %ax
	subl $8, %eax
	movw %ax, %ds

	// get current disk
	movl $0x1000, %ecx
	movb $0x0d, %bl
	int  $0x28
	xor %ah, %ah
	cmp $2, %ax
	jne not_hdd // TODO Handle Floppy & Tape boot ?
	movw %ax, DRIVE
	movl $PHY_ADDR, ADDR

	xchg %dl, %dh
	movw %dx, ID_BIG_ENDIAN
	movw $0x0000, BLK_ADDR_BIG_ENDIAN
	movb $0x08, BLK_ADDR_BIG_ENDIAN + 2
	movb $0x08, BLK_COUNT

	movl $__bss_start, %eax
	movl $0x1000, %ecx
	xor %edx, %edx
	div %ecx
	inc %eax
	mov %eax, %ecx
1:
	call bios_do_disk_io
	cmpw $0x0000, STATUS
	jne read_error
	addl $0x1000, ADDR
	addb $0x08, BLK_ADDR_BIG_ENDIAN + 2
	loop 1b

	ljmp $0x168, $0

boot_start_2:
	movl $0x160, %eax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %gs
	mov %ax, %fs
	movl $(0xff000-PHY_ADDR), %esp
	pushl $greetings_msg
	call puts
	addl $4, %esp

	// at this point we're complete in memory 
	movl	$_end, %ecx		/* zero bss */
	movl	$__bss_start, %edi
	subl	%edi, %ecx
	shr	$2, %ecx		/* _end and __bss_start are aligned */
	xor	%eax, %eax
	rep
	stosl

	call	_C_LABEL(boot2)		/* C bootstrap code */
	jmp	_rtt

ENTRY(getcr0)
        movl	%cr0,%eax
        ret

ENTRY(andcr0)
        movl	%cr0,%eax
        andl	4(%esp),%eax
        movl	%eax,%cr0
        ret

ENTRY(orcr0)
        movl	%cr0,%eax
        orl	4(%esp),%eax
        movl	%eax,%cr0
        ret

// c: void puts(const char *str)
ENTRY(puts)
	movl 4(%esp), %eax
puts_no_stack:
	push %ebx
	mov $0x103, %bx
	int $0x28
	pop %ebx
	ret

// c: void bios_do_disk_io()
ENTRY(bios_do_disk_io)
	movl $g_disk_io, %eax
	pushal
	movl $0x1e, %ebx
	int $0x28
	popal
	ret

	.code32
ENTRY(_rtt)
_rtt:
	sti
	hlt
	jmp _rtt

ENTRY(boot_data_addr)
	.long	PHY_ADDR

not_hdd:
	pushl $not_hdd_msg
	call puts
	addl $4, %esp
	jmp	_rtt

read_error:
	pushl $error_msg
	call puts
	addl $4, %esp
	jmp	_rtt

greetings_msg:
	.asciz "\n\r\033[32mLoading netBSD bootloader...\033[0m\n\r"

not_hdd_msg:
	.asciz	"\n\r\033[31mNot booting from HDD\033[0m\n\r"

error_msg:
	.asciz	"\n\r\033[31mCan't read the rest of the boot\033[0m\n\r"

	.text
	.align  16
ENTRY(flatcodeseg)
	.long	0x00000198
ENTRY(flatdataseg)
	.long	0x00000190
