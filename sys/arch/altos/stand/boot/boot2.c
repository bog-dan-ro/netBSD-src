/*	$NetBSD: boot2.c,v 1.79 2022/06/08 21:43:45 wiz Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
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

/*
 * Copyright (c) 2003
 *	David Laight.  All rights reserved
 * Copyright (c) 1996, 1997, 1999
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 1997
 *	Jason R. Thorpe.  All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/* Based on stand/biosboot/main.c */

#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/bootblock.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/bootcfg.h>
#include <lib/libsa/ufs.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>
#include <bootmod.h>
#include <bootmenu.h>
#include <biosdisk.h>
#include "devopen.h"

#ifdef _STANDALONE
#include <bootinfo.h>
#endif

// extern struct x86_boot_params boot_params;

extern	const char bootprog_name[], bootprog_rev[], bootprog_kernrev[];

int errno;

int boot_biosdev;

static const char * const names[][2] = {
	{ "netbsd", "netbsd.gz" },
	{ "onetbsd", "onetbsd.gz" },
	{ "netbsd.old", "netbsd.old.gz" },
};

#define NUMNAMES (sizeof(names)/sizeof(names[0]))
#define DEFFILENAME names[0][0]

#ifndef NO_GPT
#define MAXDEVNAME 39 /* "NAME=" + 34 char part_name */
#else
#define MAXDEVNAME 16
#endif

static char *default_devname;
static int default_unit, default_partition;
static const char *default_filename;
static const char *default_part_name;

char *sprint_bootsel(const char *);
static void bootit(const char *, int);
void boot2(void);

void	command_help(char *);
#if LIBSA_ENABLE_LS_OP
void	command_ls(char *);
#endif
void	command_quit(char *);
void	command_boot(char *);
void	command_pkboot(char *);
void	command_dev(char *);
void	command_root(char *);
void	command_fill1(char *);
void	command_fill2(char *);
#ifndef SMALL
void	command_menu(char *);
#endif
void	command_modules(char *);

const struct bootblk_command commands[] = {
	{ "help",	command_help },
	{ "?",		command_help },
#if LIBSA_ENABLE_LS_OP
	{ "ls",		command_ls },
#endif
	{ "quit",	command_quit },
	{ "boot",	command_boot },
	{ "pkboot",	command_pkboot },
	{ "dev",	command_dev },
	{ "fill1",	command_fill1 },
	{ "fill2",	command_fill2 },
	{ "root",	command_root },
#ifndef SMALL
	{ "menu",	command_menu },
#endif
	{ "modules",	command_modules },
	{ "load",	module_add },
	{ "fs",		fs_add },
	{ "userconf",	userconf_add },
	{ NULL,		NULL },
};

int
parsebootfile(const char *fname, char **fsname, char **devname,
	      int *unit, int *partition, const char **file)
{
	const char *col;
	static char savedevname[MAXDEVNAME+1];

	*fsname = "ufs";
	if (default_part_name == NULL) {
		*devname = default_devname;
	} else {
		snprintf(savedevname, sizeof(savedevname),
		    "NAME=%s", default_part_name);
		*devname = savedevname;
	}
	*unit = default_unit;
	*partition = default_partition;
	*file = default_filename;

	if (fname == NULL)
		return 0;

	if ((col = strchr(fname, ':')) != NULL) {	/* device given */
		int devlen;
		int u = 0, p = 0;
		int i = 0;

		devlen = col - fname;
		if (devlen > MAXDEVNAME)
			return EINVAL;

#ifndef NO_GPT
		if (strstr(fname, "NAME=") == fname) {
			strlcpy(savedevname, fname, devlen + 1);
			*devname = savedevname;
			*unit = -1;
			*partition = -1;
			fname = col + 1;
			goto out;
		}
#endif

#define isvalidname(c) ((c) >= 'a' && (c) <= 'z')
		if (!isvalidname(fname[i]))
			return EINVAL;
		do {
			savedevname[i] = fname[i];
			i++;
		} while (isvalidname(fname[i]));
		savedevname[i] = '\0';

#define isnum(c) ((c) >= '0' && (c) <= '9')
		if (i < devlen) {
			if (!isnum(fname[i]))
				return EUNIT;
			do {
				u *= 10;
				u += fname[i++] - '0';
			} while (isnum(fname[i]));
		}

#define isvalidpart(c) ((c) >= 'a' && (c) <= 'z')
		if (i < devlen) {
			if (!isvalidpart(fname[i]))
				return EPART;
			p = fname[i++] - 'a';
		}

		if (i != devlen)
			return ENXIO;

		*devname = savedevname;
		*unit = u;
		*partition = p;
		fname = col + 1;
	}

out:
	if (*fname)
		*file = fname;

	return 0;
}

char *
sprint_bootsel(const char *filename)
{
	char *fsname, *devname;
	int unit, partition;
	const char *file;
	static char buf[80];

	if (parsebootfile(filename, &fsname, &devname, &unit,
			  &partition, &file) == 0) {
		if (strstr(devname, "NAME=") == devname)
			snprintf(buf, sizeof(buf), "%s:%s", devname, file);
		else
			snprintf(buf, sizeof(buf), "%s%d%c:%s", devname, unit,
			    'a' + partition, file);
		return buf;
	}
	return "(invalid)";
}

static void
clearit(void)
{

	if (bootcfg_info.clear)
		clear_pc_screen();
}

static void
bootit(const char *filename, int howto)
{
	if (howto & AB_VERBOSE)
		printf("booting %s (howto 0x%x)\n", sprint_bootsel(filename),
		    howto);
#ifdef KERNEL_DIR
	char path[512];
	strcpy(path, filename);
	strcat(path, "/kernel");
	(void)exec_netbsd(path, 0, howto, boot_biosdev < 0x80, clearit);
#endif

	if (exec_netbsd(filename, 0, howto, boot_biosdev < 0x80, clearit) < 0)
		printf("boot: %s: %s\n", sprint_bootsel(filename),
		       strerror(errno));
	else
		printf("boot returned\n");
}

/*
 * Called from the initial entry point boot_start in biosboot.S
 *
 * biosdev: BIOS drive number the system booted from
 * biossector: Sector number of the NetBSD partition
 */
void
boot2(void)
{
	extern char __bss_start[];
	extern uint32_t g_crc32;
	uint32_t crc = crc32(0, (void*)&g_crc32 + 4, (uint32_t)(__bss_start - (char*)&g_crc32 - 4));
	if (crc != g_crc32) {
		printf("\n\033[31mCRC32[%x:%x] mismatch: %x != %x, press any key to continue\033[0m\n", (uint32_t)&g_crc32 + 4, (uint32_t)(__bss_start - (char*)&g_crc32 - 4), crc, g_crc32);
		getchar();
	} else {
		printf("\n\033[32mCRC32[%x:%x] match: %x == %x\033[0m\n", (uint32_t)&g_crc32 + 4, (uint32_t)(__bss_start - (char*)&g_crc32 - 4), crc, g_crc32);
	}

	extern char end[];
	setheap((void *)ALIGN(end), (void *)MEMSIZE - ALIGN(end) - 0x10000); // keep 0x10000 for stack
	extern char twiddle_toggle;
	int currname;
	char c;

	twiddle_toggle = 1;	/* no twiddling until we're ready */

	boot_modules_enabled = false/*!(boot_params.bp_flags
				 & X86_BP_FLAGS_NOMODULES)*/;


	/* need to remember these */
	boot_biosdev = get_hdd_id();

	/* try to set default device to what BIOS tells us */
	bios2dev(boot_biosdev, &default_devname, &default_unit,
		 &default_partition, &default_part_name);

	/* if the user types "boot" without filename */
	default_filename = DEFFILENAME;

#ifndef SMALL
	// if (!(boot_params.bp_flags & X86_BP_FLAGS_NOBOOTCONF)) {
		parsebootconf(BOOTCFG_FILENAME);
	// } else {
	// 	bootcfg_info.timeout = boot_params.bp_timeout;
	// }


	clearit();
	print_bootcfg_banner(bootprog_name, bootprog_rev);

	/* Display the menu, if applicable */
	twiddle_toggle = 0;
	if (bootcfg_info.nummenu > 0) {
		/* Does not return */
		doboottypemenu();
	}

#else
	twiddle_toggle = 0;
	clearit();
	print_bootcfg_banner(bootprog_name, bootprog_rev);
#endif

	printf("Press return to boot now, any other key for boot menu\n");
	for (currname = 0; currname < NUMNAMES; currname++) {
		printf("booting %s - starting in ",
		       sprint_bootsel(names[currname][0]));

#ifdef SMALL
#error "SMALL not supported"
		c = awaitkey(boot_params.bp_timeout, 1);
#else
		c = awaitkey((bootcfg_info.timeout < 0) ? 0
		    : bootcfg_info.timeout, 1);
#endif
		if ((c != '\r') && (c != '\n') && (c != '\0')) {
		    // if ((boot_params.bp_flags & X86_BP_FLAGS_PASSWORD) == 0) {
			/* do NOT ask for password */
			bootmenu(); /* does not return */
		    // } else {
			// /* DO ask for password */
			// if (check_password((char *)boot_params.bp_password)) {
			//     /* password ok */
			//     printf("type \"?\" or \"help\" for help.\n");
			//     bootmenu(); /* does not return */
			// } else {
			//     /* bad password */
			//     printf("Wrong password.\n");
			//     currname = 0;
			//     continue;
			// }
		    // }
		}

		/*
		 * try pairs of names[] entries, foo and foo.gz
		 */
		/* don't print "booting..." again */
		bootit(names[currname][0], 0);
		/* since it failed, try compressed bootfile. */
		bootit(names[currname][1], AB_VERBOSE);
	}

	bootmenu();	/* does not return */
}

/* ARGSUSED */
void
command_help(char *arg)
{

	printf("commands are:\n"
	       "boot [dev:][filename] [-12acdqsvxz]\n"
#ifndef NO_RAIDFRAME
	       "     dev syntax is (hd|fd|cd|raid)[N[x]]\n"
#else
	       "     dev syntax is (hd|fd|cd)[N[x]]n"
#endif
#ifndef NO_GPT
	       "                or NAME=gpt_label\n"
#endif
	       "     (ex. \"hd0a:netbsd.old -s\")\n"
	       "pkboot [dev:][filename] [-12acdqsvxz]\n"
#if LIBSA_ENABLE_LS_OP
	       "ls [dev:][path]\n"
#endif
		   "fill1 {value}\n"
		   "fill2 {value}\n"
	       "dev [dev:]\n"
	       "root    {spec}\n"
	       "     spec can be disk, e.g. wd0, sd0\n"
	       "     or string like wedge:name\n"
#ifndef SMALL
	       "menu (reenters boot menu, if defined in boot.cfg)\n"
#endif
	       "modules {on|off|enabled|disabled}\n"
	       "load {path_to_module}\n"
	       "userconf {command}\n"
	       "help|?\n"
	       "quit\n");
}

#if LIBSA_ENABLE_LS_OP
void
command_ls(char *arg)
{
	const char *save = default_filename;

	default_filename = "/";
	ls(arg);
	default_filename = save;
}
#endif

/* ARGSUSED */
void
command_quit(char *arg)
{

	printf("Exiting...\n");
	wait_sec(1);
	reboot();
	/* Note: we shouldn't get to this point! */
	panic("Could not reboot!");
}

void
command_boot(char *arg)
{
	char *filename;
	int howto;

	if (!parseboot(arg, &filename, &howto))
		return;

	if (filename != NULL) {
		bootit(filename, howto);
	} else {
		int i;

		for (i = 0; i < NUMNAMES; i++) {
			bootit(names[i][0], howto);
			bootit(names[i][1], howto);
		}
	}
}

void
command_pkboot(char *arg)
{
	extern int has_prekern;
	has_prekern = 1;
	command_boot(arg);
	has_prekern = 0;
}

void
command_dev(char *arg)
{
	static char savedevname[MAXDEVNAME + 1];
	char *fsname, *devname;
	const char *file; /* dummy */

	if (*arg == '\0') {
		biosdisk_probe();

#ifndef NO_GPT
		if (default_part_name)
			printf("default NAME=%s on %s%d\n", default_part_name,
			       default_devname, default_unit);
		else
#endif
			printf("default %s%d%c\n",
			       default_devname, default_unit,
			       'a' + default_partition);
		return;
	}

	if (strchr(arg, ':') == NULL ||
	    parsebootfile(arg, &fsname, &devname, &default_unit,
			  &default_partition, &file)) {
		command_help(NULL);
		return;
	}

	/* put to own static storage */
	strncpy(savedevname, devname, MAXDEVNAME + 1);
	default_devname = savedevname;

	/* +5 to skip leading NAME= */
	if (strstr(devname, "NAME=") == devname)
		default_part_name = default_devname + 5;
}

void
command_root(char *arg)
{
	struct btinfo_rootdevice *biv = &bi_root;

	strncpy(biv->devname, arg, sizeof(biv->devname));
	if (biv->devname[sizeof(biv->devname)-1] != '\0') {
		biv->devname[sizeof(biv->devname)-1] = '\0';
		printf("truncated to %s\n",biv->devname);
	}
}

void command_fill1(char *arg)
{
	g_disk_io.hdd.fill[0] = atoi(arg);
	printf("Set fill[0] = 0x%x\n", g_disk_io.hdd.fill[0]);
}

void command_fill2(char *arg)
{
	g_disk_io.hdd.fill[1] = atoi(arg);
	printf("Set fill[1] = 0x%x\n", g_disk_io.hdd.fill[1]);
}

#ifndef SMALL
/* ARGSUSED */
void
command_menu(char *arg)
{

	if (bootcfg_info.nummenu > 0) {
		/* Does not return */
		doboottypemenu();
	} else {
		printf("No menu defined in boot.cfg\n");
	}
}
#endif /* !SMALL */

void
command_modules(char *arg)
{

	if (strcmp(arg, "enabled") == 0 ||
	    strcmp(arg, "on") == 0)
		boot_modules_enabled = true;
	else if (strcmp(arg, "disabled") == 0 ||
	    strcmp(arg, "off") == 0)
		boot_modules_enabled = false;
	else
		printf("invalid flag, must be 'enabled' or 'disabled'.\n");
}
