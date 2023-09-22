/*	$NetBSD: pcio.c,v 1.30 2011/06/08 16:04:40 joerg Exp $	 */

/*
 * Copyright (c) 1996, 1997
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * console I/O
 * needs lowlevel routines from conio.S and comio.S
 */

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <sys/bootblock.h>

#include "libi386.h"
#include "bootinfo.h"

struct btinfo_console btinfo_console;

#define POLL_FREQ 10

void
delay(int us)
{
	int tgt = biosgetsystime() + us / 1000;

	while (biosgetsystime() < tgt) {
		asm ("nop");
	}
}

#ifdef SUPPORT_SERIAL
static int
getcomaddr(int idx)
{
	short addr;
#ifdef CONSADDR
	if (CONSADDR != 0)
		return CONSADDR;
#endif
	/* read in BIOS data area */
	pvbcopy((void *)(0x400 + 2 * idx), &addr, 2);
	return addr;
}
#endif

void
clear_pc_screen(void)
{
	puts( "\033[2J" );
}

char
awaitkey(int timeout, int tell)
{
	int i;
	char c = 0;

	i = timeout * POLL_FREQ;

	for (;;) {
		if (tell && (i % POLL_FREQ) == 0) {
			char numbuf[32];
			int len;

			len = snprintf(numbuf, sizeof(numbuf), "%d seconds. ",
			    i/POLL_FREQ);
			if (len > 0 && len < sizeof(numbuf)) {
				char *p = numbuf;

				printf("%s", numbuf);
				while (*p)
					*p++ = '\b';
				printf("%s", numbuf);
			}
		}
		if (iskey(1)) {
			/* flush input buffer */
			while (iskey(0))
				c = getchar();
			if (c == 0)
				c = -1;
			goto out;
		}
		if (i--)
			delay(1000000 / POLL_FREQ);
		else
			break;
	}

out:
	if (tell)
		printf("0 seconds.     \n");

	return c;
}


void
wait_sec(int sec)
{
	delay(sec * 1000000);
}
