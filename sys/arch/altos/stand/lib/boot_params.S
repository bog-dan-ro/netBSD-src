/*	$NetBSD: boot_params.S,v 1.6 2010/01/17 14:54:44 drochner Exp $	*/

/* Default boot parameters - must match struct x86_boot_params in bootblock.h */

	.globl _C_LABEL(g_disk_io)
_C_LABEL(g_disk_io):
DRIVE:
	.word 0x0002 			// drive
	.word 0x0008 			// command, Read SCSI
STATUS:
	.word 0xffff			// status
	.long 0xffffffff  		// errno
ADDR:
	.long 0x00000000 		// addr
ID_BIG_ENDIAN:
	.byte 0x00, 0x00
BLK_ADDR_BIG_ENDIAN:
	.byte 0x00, 0x00, 0x08 // on HDD, altos reads 8th sectors stating with the 8th sector 
BLK_COUNT:
	.byte 0x08
	.byte 0x00, 0x00 // fill 0 ?
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // fill 0
.balign	4
.globl _C_LABEL(g_crc32)
_C_LABEL(g_crc32):
	.long	0xbadb00b4		/* crc32 of boot_params */

