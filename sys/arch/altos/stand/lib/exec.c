/*	$NetBSD: exec.c,v 1.78.4.1 2023/05/13 13:26:57 martin Exp $	 */

/*
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
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Starts a NetBSD ELF kernel. The low level startup is done in startprog.S.
 * This is a special version of exec.c to support use of XMS.
 */

#include <sys/param.h>
#include <sys/reboot.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <biosdisk_ll.h>

#include "loadfile.h"
#include "libi386.h"
#include "bootinfo.h"
#include "bootmod.h"
#include "biosdisk.h"

#ifndef	PAGE_SIZE
#define	PAGE_SIZE	4096
#endif

#define MODULE_WARNING_SEC	5

#define MAXMODNAME	32	/* from <sys/module.h> */
extern struct btinfo_console btinfo_console;
extern struct btinfo_rootdevice bi_root;

boot_module_t *boot_modules;
bool boot_modules_enabled = true;
bool kernel_loaded;

typedef struct userconf_command {
		char *uc_text;
		size_t uc_len;
		struct userconf_command *uc_next;
} userconf_command_t;
userconf_command_t *userconf_commands = NULL;

struct btinfo_modulelist *btinfo_modulelist;
static size_t btinfo_modulelist_size;
static uint32_t image_end;
static char module_base[64] = "/";
static int howto;

static struct btinfo_userconfcommands *btinfo_userconfcommands = NULL;
static size_t btinfo_userconfcommands_size = 0;

static void	module_init(const char *);
static void	module_add_common(const char *, uint8_t);

static void	userconf_init(void);

static void	extract_device(const char *, char *, size_t);
static void	module_base_path(char *, size_t, const char *);
static int	module_open(boot_module_t *, int, const char *, const char *,
			bool);

void
module_add(char *name)
{
		return module_add_common(name, BM_TYPE_KMOD);
}

void
splash_add(char *name)
{
		return module_add_common(name, BM_TYPE_IMAGE);
}

void
rnd_add(char *name)
{
		return module_add_common(name, BM_TYPE_RND);
}

void
fs_add(char *name)
{
		return module_add_common(name, BM_TYPE_FS);
}

/*
 * Add a /-separated list of module names to the boot list
 */
void
module_add_split(const char *name, uint8_t type)
{
		char mod_name[MAXMODNAME];
		int i;
		const char *mp = name;
		char *ep;

		while (*mp) {				/* scan list of module names */
				i = MAXMODNAME;
				ep = mod_name;
				while (--i) {			/* scan for end of first name */
						*ep = *mp;
						if (*ep == '/')		/* NUL-terminate the name */
								*ep = '\0';

						if (*ep == 0 ) {	/* add non-empty name */
								if (ep != mod_name)
										module_add_common(mod_name, type);
								break;
						}
						ep++; mp++;
				}
				if (*ep != 0) {
						printf("module name too long\n");
						return;
				}
				if  (*mp == '/') {		/* skip separator if more */
						mp++;
				}
		}
}

static void
module_add_common(const char *name, uint8_t type)
{
		boot_module_t *bm, *bmp;
		size_t len;
		char *str;

		while (*name == ' ' || *name == '\t')
				++name;

		for (bm = boot_modules; bm != NULL; bm = bm->bm_next)
				if (bm->bm_type == type && strcmp(bm->bm_path, name) == 0)
						return;

		bm = alloc(sizeof(boot_module_t));
		len = strlen(name) + 1;
		str = alloc(len);
		if (bm == NULL || str == NULL) {
				printf("couldn't allocate module\n");
				return;
		}
		memcpy(str, name, len);
		bm->bm_path = str;
		bm->bm_next = NULL;
		bm->bm_type = type;
		if (boot_modules == NULL)
				boot_modules = bm;
		else {
				for (bmp = boot_modules; bmp->bm_next;
					 bmp = bmp->bm_next)
						;
				bmp->bm_next = bm;
		}
}

void
userconf_add(char *cmd)
{
		userconf_command_t *uc;
		size_t len;
		char *text;

		while (*cmd == ' ' || *cmd == '\t')
				++cmd;

		uc = alloc(sizeof(*uc));
		if (uc == NULL) {
				printf("couldn't allocate command\n");
				return;
		}

		len = strlen(cmd) + 1;
		text = alloc(len);
		if (text == NULL) {
				dealloc(uc, sizeof(*uc));
				printf("couldn't allocate command\n");
				return;
		}
		memcpy(text, cmd, len);

		uc->uc_text = text;
		uc->uc_len = len;
		uc->uc_next = NULL;

		if (userconf_commands == NULL)
				userconf_commands = uc;
		else {
				userconf_command_t *ucp;
				for (ucp = userconf_commands; ucp->uc_next != NULL;
					 ucp = ucp->uc_next)
						;
				ucp->uc_next = uc;
		}
}

struct btinfo_prekern bi_prekern;
int has_prekern = 0;

static int
common_load_prekern(const char *file,
					physaddr_t loadaddr, int floppy, u_long marks[MARK_MAX])
{
		paddr_t kernpa_start, kernpa_end;
		char prekernpath[] = "/prekern";
		u_long prekern_start;
		int fd, flags;

		marks[MARK_START] = loadaddr;

		/* Load the prekern (static) */
		flags = LOAD_KERNEL & ~(LOAD_HDR|LOAD_SYM);
		if ((fd = loadfile(prekernpath, marks, flags)) == -1)
				return errno;
		close(fd);

		prekern_start = marks[MARK_START];

		/* The kernel starts at 2MB. */
		marks[MARK_START] = loadaddr;
		marks[MARK_END] = loadaddr + (1UL << 21);
		kernpa_start = (1UL << 21);

		/* Load the kernel (dynamic) */
		flags = (LOAD_KERNEL | LOAD_DYN) & ~(floppy ? LOAD_BACKWARDS : 0);
		if ((fd = loadfile(file, marks, flags)) == -1)
				return errno;
		close(fd);

		kernpa_end = marks[MARK_END] - loadaddr;

		/* If the root fs type is unusual, load its module. */
		if (fsmod != NULL)
				module_add_split(fsmod, BM_TYPE_KMOD);

		bi_prekern.kernpa_start = kernpa_start;
		bi_prekern.kernpa_end = kernpa_end;
		BI_ADD(&bi_prekern, BTINFO_PREKERN, sizeof(struct btinfo_prekern));

		/*
		 * Gather some information for the kernel. Do this after the
		 * "point of no return" to avoid memory leaks.
		 * (but before DOS might be trashed in the XMS case)
		 */
#ifdef PASS_BIOSGEOM
		bi_getbiosgeom();
#endif
#ifdef PASS_MEMMAP
		bi_getmemmap();
#endif

		marks[MARK_START] = prekern_start;
		marks[MARK_END] = (((u_long)marks[MARK_END] + sizeof(int) - 1)) &
						  (-sizeof(int));
		image_end = marks[MARK_END];
		kernel_loaded = true;

		return 0;
}

static int
common_load_kernel(const char *file,
				   physaddr_t loadaddr, int floppy, u_long marks[MARK_MAX])
{
		int fd;

		marks[MARK_START] = loadaddr;
		if ((fd = loadfile(file, marks,
						   LOAD_KERNEL & ~(floppy ? LOAD_BACKWARDS : 0))) == -1)
				return errno;

		close(fd);

		/* If the root fs type is unusual, load its module. */
		if (fsmod != NULL)
				module_add_split(fsmod, BM_TYPE_KMOD);

		marks[MARK_END] = (((u_long) marks[MARK_END] + sizeof(int) - 1)) &
						  (-sizeof(int));
		image_end = marks[MARK_END];
		kernel_loaded = true;

		return 0;
}

struct dlcode_header
{
		uint16_t id;
		// 0x02
		uint16_t size; // 0 -> 0x20 !=0 -> 0x40
		// 0x04
		uint32_t addr1;
		// 0x08
		uint32_t addr2;
		// 0x0c
		uint32_t count;
		// 10
		uint8_t bla[12];
		uint8_t id_0x40; // 0x40
		uint8_t id_unk;
		uint16_t id_last; // 2 or 0x800
};

struct boot_info {
		uint8_t what[8];
		uint16_t version[8];
};

#define BOOT_NARGS	(4 + sizeof(struct boot_info) / 4)

static uint32_t swapCS_IP(uint32_t addr)
{
		return addr << 10 | addr >> 10;
}

#define BUFFER_SIZE 0x800
#define MAX_MEMORY_ADDRESS 0x10000

static int fill0(uint16_t drive, uint8_t *buffer, int count, uint32_t addr_186)
{
		int status = (int)buffer;
		for (int i = 0; i < BUFFER_SIZE; ++i)
				buffer[i] = 0;

		while (count > 0) {
				int bytes_to_write = (count > BUFFER_SIZE) ? BUFFER_SIZE : count;
				uint16_t remaining_space = MAX_MEMORY_ADDRESS - (addr_186 & 0xFFFF);
				if (bytes_to_write > remaining_space)
						bytes_to_write = remaining_space;
				status = uploadDLCode(drive, buffer, (uint16_t)bytes_to_write, addr_186);
				if (status < 0)
						return -1;  // Error occurred during upload
				addr_186 += bytes_to_write;
				count -= bytes_to_write;
		}
		return status;
}

char *dlcode_path = 0;
bool is_dbg = FALSE;

enum DLCODE_RESULT {
		DLC_OK,
		DLC_IO_ERROR,
		DLC_UPLOAD_ERROR
};
static enum DLCODE_RESULT do_dlcode(uint16_t drive, char *file, uint32_t *execPtr)
{
		int sz;
		int chunkSize;
		int sectionSize;
		int pos;
		int firstSectionPos = 0;
		int secondSectionPos = 0;
		char path[256];
		strcpy(path, dlcode_path);
		strcat(path, file);
		if (is_dbg)
				strcat(path, ".dbg");

		int inFile = open(path, 0);
		if (inFile == -1) {
				printf("Can't open %s\n", path);
				return DLC_IO_ERROR;
		}
		printf("Download code from %s\n", path);
		char *buffer = alloc(0x1000);
		if (buffer == NULL) {
				printf("Can't alloc memory\n");
				goto io_error;
		}

		struct dlcode_header dlCodeHeader;
		int res = read(inFile, &dlCodeHeader, 0x20);
		if (res != 0x20) {
				printf("Invalid size\n");
				goto io_error;
		}

		uint32_t addr_186 = 0x3000;
		uint32_t pos_186 = addr_186;

		if (dlCodeHeader.id == 0x206) {
				if ((dlCodeHeader.id_last & 2) == 0) {
						if ((dlCodeHeader.id_0x40 & 0x40) == 0) {
								dlCodeHeader.addr1 = swapCS_IP(dlCodeHeader.addr1);
								dlCodeHeader.addr2 = swapCS_IP(dlCodeHeader.addr2);
								dlCodeHeader.count = swapCS_IP(dlCodeHeader.count);
						}
						sectionSize = (dlCodeHeader.addr1) + dlCodeHeader.addr2;
						if (dlCodeHeader.size == 0) {
								pos = 0x20;
						} else {
								pos = 0x80;
						}
						lseek(inFile, pos, SEEK_SET);
						pos_186 = addr_186;
						*execPtr = pos_186;
						for (; 0 < sectionSize; sectionSize = sectionSize - sz) {
								sz = sectionSize;
								if (0x7ff < sectionSize) {
										sz = 0x800;
								}
								const int rsz = read(inFile, buffer, sz);
								if (rsz < 0)
										goto io_error;

								int res = uploadDLCode(drive, buffer, (uint16_t) sz, pos_186);
								if (res < 0)
										goto upload_error;
								pos_186 = pos_186 + sz;
						}
						sz = fill0(drive, buffer, dlCodeHeader.count, pos_186);
						if (sz < 0)
								goto upload_error;
				} else {
						if ((dlCodeHeader.id_0x40 & 0x40) == 0) {
								dlCodeHeader.addr1 = swapCS_IP(dlCodeHeader.addr1);
								dlCodeHeader.addr2 = swapCS_IP(dlCodeHeader.addr2);
								dlCodeHeader.count = swapCS_IP(dlCodeHeader.count);
						}
						if ((dlCodeHeader.id_last & 0x800) == 0) {
								secondSectionPos = dlCodeHeader.size + 0x20;
								firstSectionPos = dlCodeHeader.size + dlCodeHeader.addr1 + 0x20;
						} else {
								lseek(inFile, 0x20, SEEK_SET);
								read(inFile, buffer, (uint) dlCodeHeader.size);
								lseek(inFile, *(uint32_t *) (buffer + 0x14), SEEK_SET);
								uint32_t sz = *(uint32_t *) (buffer + 0x18);
								while (sz) {
										read(inFile, buffer, 0x20);
										sz -= 0x20;
										uint16_t val = *(uint16_t *) (buffer);
										uint32_t secPos = *(uint32_t *) (buffer + 8);
										if ((val & 1) == 0) {
												if ((val & 2) != 0) {
														firstSectionPos = secPos;
												}
										}
										else {
												secondSectionPos = secPos;
										}
								}
						}
						sectionSize = dlCodeHeader.addr2;
						lseek(inFile,firstSectionPos,SEEK_SET);
						pos_186 = addr_186;
						for (; 0 < sectionSize; sectionSize = sectionSize - chunkSize) {
								chunkSize = sectionSize;
								if (0x7ff < sectionSize) {
										chunkSize = 0x800;
								}
								if (0x10000 < (uint)((int)pos_186 % 0x10000 + chunkSize)) {
										chunkSize = -((int)pos_186 % 0x10000 + -0x10000);
								}
								sz = read(inFile,buffer,chunkSize);
								if (sz < 0)
										goto io_error;
								res = uploadDLCode(drive, buffer,(uint16_t)chunkSize,pos_186);
								if (res < 0)
										goto upload_error;
								pos_186 = pos_186 + chunkSize;
						}
						sz = fill0(drive, buffer, dlCodeHeader.count, pos_186);
						if (sz < 0)
								goto upload_error;
						sectionSize = dlCodeHeader.addr1;
						pos_186 = pos_186 + dlCodeHeader.count;
						if ((pos_186 & 0xf) != 0) {
								pos_186 = (pos_186 + 0x10) & 0xfffffff0;
						}
						*execPtr = addr_186 + dlCodeHeader.count + dlCodeHeader.addr2;
						if ((*execPtr & 0xf) != 0) {
								*execPtr += 0x10;
								*(uint8_t *)execPtr &= 0xf0;
						}
						lseek(inFile,secondSectionPos,SEEK_SET);
						for (; 0 < sectionSize; sectionSize = sectionSize - chunkSize) {
								chunkSize = sectionSize;
								if (0x7ff < sectionSize) {
										chunkSize = 0x800;
								}
								if (0x10000 < (uint)((int)pos_186 % 0x10000 + chunkSize)) {
										chunkSize = -((int)pos_186 % 0x10000 + -0x10000);
								}
								sz = read(inFile,buffer,chunkSize);
								if (sz < 0)
										goto io_error;
								int wsz = uploadDLCode(drive, buffer,(uint16_t)chunkSize,pos_186);
								if (wsz < 0)
										goto upload_error;
								pos_186 = pos_186 + chunkSize;
						}
				}
		} else {
				lseek(inFile,0,SEEK_SET);
				while (sz = read(inFile,buffer,0x800), sz != 0) {
						int res = uploadDLCode(drive, buffer,(uint16_t)sz, pos_186);
						if (res < 0)
								goto upload_error;
						pos_186 = pos_186 + sz;
				}
				*execPtr = addr_186;
		}
		close(inFile);
		dealloc(buffer, 0x1000);
		return DLC_OK;
io_error:
		close(inFile);
		dealloc(buffer, 0x1000);
		return DLC_IO_ERROR;
upload_error:
		close(inFile);
		dealloc(buffer, 0x1000);
		return DLC_UPLOAD_ERROR;
}

//#define version  ((uint16_t)g_disk_io.response[1] << 8 | g_disk_io.response[2])

int
exec_netbsd(const char *file, physaddr_t loadaddr, int boothowto, int floppy,
			void (*callback)(void))
{
		uint32_t boot_argv[BOOT_NARGS];
		u_long marks[MARK_MAX];
		struct btinfo_symtab btinfo_symtab;
		u_long entry;
		int error;

		struct boot_info * boot_info = (struct boot_info *)&boot_argv[BOOT_NARGS - sizeof(struct boot_info) / 4];
		memset(boot_info, 0, sizeof(struct boot_info));

		printf("exec: file=%s loadaddr=0x%lx\n", file ? file : "NULL",
			   loadaddr);

		BI_ALLOC(BTINFO_MAX);

		BI_ADD(&btinfo_console, BTINFO_CONSOLE, sizeof(struct btinfo_console));
		if (bi_root.devname[0]) {
				printf("Adding BTINFO_ROOTDEVICE %s", bi_root.devname);
				BI_ADD(&bi_root, BTINFO_ROOTDEVICE, sizeof(struct btinfo_rootdevice));
		}

		howto = boothowto;

		memset(marks, 0, sizeof(marks));

		if (has_prekern) {
				error = common_load_prekern(file, loadaddr,
											floppy, marks);
		} else {
				error = common_load_kernel(file, loadaddr,
										   floppy, marks);
		}
		if (error) {
				errno = error;
				goto out;
		}

		boot_argv[0] = boothowto;
		boot_argv[1] = 0;
		boot_argv[2] = (uint32_t)(boot_data_addr + (char*)bootinfo);	/* old cyl offset */
		boot_argv[3] = marks[MARK_END];

		/* pull in any modules if necessary */
		if (boot_modules_enabled) {
				module_init(file);
				if (btinfo_modulelist) {
						BI_ADD(btinfo_modulelist, BTINFO_MODULELIST,
							   btinfo_modulelist_size);
				}
		}

		userconf_init();
		if (btinfo_userconfcommands != NULL)
				BI_ADD(btinfo_userconfcommands, BTINFO_USERCONFCOMMANDS,
					   btinfo_userconfcommands_size);

		printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n", marks[MARK_ENTRY],
			   marks[MARK_NSYM], marks[MARK_SYM], marks[MARK_END]);

		btinfo_symtab.nsym = marks[MARK_NSYM];
		btinfo_symtab.ssym = marks[MARK_SYM];
		btinfo_symtab.esym = marks[MARK_END];
		BI_ADD(&btinfo_symtab, BTINFO_SYMTAB, sizeof(struct btinfo_symtab));

		if (callback != NULL)
				(*callback)();

#define STACK_PTR (uint32_t)0x100000
		entry = marks[MARK_ENTRY];
		printf("args: ");
		for (int i = 0; i < BOOT_NARGS; ++i)
				printf("arg%d=0x%x, ", i, boot_argv[i]);
		printf("stack at 0x%x...\n", STACK_PTR);
		printf("bootinfo @ %p bootinfo.nentries=%d\n", bootinfo, bootinfo->nentries);
		uint32_t execPtr[3] = {0, 0, 0};
		bool loaded[3] = {FALSE, FALSE, FALSE};
		if (do_dlcode(0xd, "ioc", &execPtr[0]) != DLC_OK) {
				panic("Can't load \"ioc\" file");
				goto out;
		}
		loaded[0] = TRUE;
		boot_info->what[0] = 4;
		boot_info->version[0] = ((uint16_t)g_disk_io.response[1] << 8 | g_disk_io.response[2]);
		printf("Ioc version %x \n", (int)boot_info->version[0]);

		int print_status = 0;
		for (int i = 1; i < 3; ++i) {
				char *board_type = 0;
				uint32_t ver = checkBoard(i + 0xd);
				printf("board %d version %d\n", i, ver);
				printf("g_disk_io.hdd.id[1] == %d\n", (int)g_disk_io.hdd.id[1]);
				if (ver == 1) {
						board_type = "mdc";
						if (boot_info->what[g_disk_io.hdd.id[1]] == 0)
								boot_info->what[g_disk_io.hdd.id[1]] = 4; /* IS_MDROP */
				} else if (ver == 2) {
						board_type = "acpa";
						boot_info->what[g_disk_io.hdd.id[1]] = 6; /* IS_ACPA */
				} else {
						continue;
				}
				boot_info->version[g_disk_io.hdd.id[1]] = ((uint16_t)g_disk_io.response[1] << 8 | g_disk_io.response[2]);
				if (!print_status) {
						printf("Downloading comm board code...");
						++print_status;
				}
				enum DLCODE_RESULT res = do_dlcode(i + 0xd, board_type, &execPtr[i]);
				if (res != DLC_OK) {
						boot_info->what[i] = 0;
						continue;
				}
				loaded[i] = TRUE;
				printf(" [%d:%s]", i, board_type);
		}
		printf("\n");
		for (int i = 3; i > 0; --i) {
				if (loaded[i -1]) {
						execDLCode(0x0c + i, execPtr[i - 1]);
				}
		}

		startprog(entry, BOOT_NARGS, boot_argv, STACK_PTR);
		panic("exec returned");

out:
		BI_FREE();
		bootinfo = NULL;
		return -1;
}

static void
extract_device(const char *path, char *buf, size_t buflen)
{
		size_t i;

		if (strchr(path, ':') != NULL) {
				for (i = 0; i < buflen - 2 && path[i] != ':'; i++)
						buf[i] = path[i];
				buf[i++] = ':';
				buf[i] = '\0';
		} else
				buf[0] = '\0';
}

static const char *
module_path(boot_module_t *bm, const char *kdev, const char *base_path)
{
		static char buf[256];
		char name_buf[256], dev_buf[64];
		const char *name, *name2, *p;

		name = bm->bm_path;
		for (name2 = name; *name2; ++name2) {
				if (*name2 == ' ' || *name2 == '\t') {
						strlcpy(name_buf, name, sizeof(name_buf));
						if ((uintptr_t)name2 - (uintptr_t)name < sizeof(name_buf))
								name_buf[name2 - name] = '\0';
						name = name_buf;
						break;
				}
		}
		if ((p = strchr(name, ':')) != NULL) {
				/* device specified, use it */
				if (p[1] == '/')
						snprintf(buf, sizeof(buf), "%s", name);
				else {
						p++;
						extract_device(name, dev_buf, sizeof(dev_buf));
						snprintf(buf, sizeof(buf), "%s%s/%s/%s.kmod",
								 dev_buf, base_path, p, p);
				}
		} else {
				/* device not specified; load from kernel device if known */
				if (name[0] == '/')
						snprintf(buf, sizeof(buf), "%s%s", kdev, name);
				else
						snprintf(buf, sizeof(buf), "%s%s/%s/%s.kmod",
								 kdev, base_path, name, name);
		}

		return buf;
}

static int
module_open(boot_module_t *bm, int mode, const char *kdev,
			const char *base_path, bool doload)
{
		int fd;
		const char *path;

		/* check the expanded path first */
		path = module_path(bm, kdev, base_path);
		fd = open(path, mode);
		if (fd != -1) {
				if ((howto & AB_SILENT) == 0 && doload)
						printf("Loading %s ", path);
		} else {
				/* now attempt the raw path provided */
				fd = open(bm->bm_path, mode);
				if (fd != -1 && (howto & AB_SILENT) == 0 && doload)
						printf("Loading %s ", bm->bm_path);
		}
		if (!doload && fd == -1) {
				printf("WARNING: couldn't open %s", bm->bm_path);
				if (strcmp(bm->bm_path, path) != 0)
						printf(" (%s)", path);
				printf("\n");
		}
		return fd;
}

static void
module_base_path(char *buf, size_t bufsize, const char *kernel_path)
{
#ifdef KERNEL_DIR
		/* we cheat here, because %.* does not work with the mini printf */
		char *ptr = strrchr(kernel_path, '/');
		if (ptr) *ptr = '\0';
		snprintf(buf, bufsize, "%s/modules", kernel_path);
		if (ptr) *ptr = '/';
#else
		const char *machine;

		switch (netbsd_elf_class) {
		case ELFCLASS32:
				machine = "i386";
				break;
		case ELFCLASS64:
				machine = "amd64";
				break;
		default:
				machine = "generic";
				break;
		}
		if (netbsd_version / 1000000 % 100 == 99) {
				/* -current */
				snprintf(buf, bufsize,
						 "/stand/%s/%d.%d.%d/modules", machine,
						 netbsd_version / 100000000,
						 netbsd_version / 1000000 % 100,
						 netbsd_version / 100 % 10000);
		} else if (netbsd_version != 0) {
				/* release */
				snprintf(buf, bufsize,
						 "/stand/%s/%d.%d/modules", machine,
						 netbsd_version / 100000000,
						 netbsd_version / 1000000 % 100);
		}
#endif
}

static void
module_init(const char *kernel_path)
{
		struct bi_modulelist_entry *bi;
		struct stat st;
		char kdev[64];
		char *buf;
		boot_module_t *bm;
		ssize_t len;
		off_t off;
		int err, fd, nfail = 0;

		extract_device(kernel_path, kdev, sizeof(kdev));
		module_base_path(module_base, sizeof(module_base), kernel_path);

		/* First, see which modules are valid and calculate btinfo size */
		len = sizeof(struct btinfo_modulelist);
		for (bm = boot_modules; bm; bm = bm->bm_next) {
				fd = module_open(bm, 0, kdev, module_base, false);
				if (fd == -1) {
						bm->bm_len = -1;
						++nfail;
						continue;
				}
				err = fstat(fd, &st);
				if (err == -1 || st.st_size == -1) {
						printf("WARNING: couldn't stat %s\n", bm->bm_path);
						close(fd);
						bm->bm_len = -1;
						++nfail;
						continue;
				}
				bm->bm_len = st.st_size;
				close(fd);
				len += sizeof(struct bi_modulelist_entry);
		}

		/* Allocate the module list */
		btinfo_modulelist = alloc(len);
		if (btinfo_modulelist == NULL) {
				printf("WARNING: couldn't allocate module list\n");
				wait_sec(MODULE_WARNING_SEC);
				return;
		}
		memset(btinfo_modulelist, 0, len);
		btinfo_modulelist_size = len;

		/* Fill in btinfo structure */
		buf = (char *)btinfo_modulelist;
		btinfo_modulelist->num = 0;
		off = sizeof(struct btinfo_modulelist);

		for (bm = boot_modules; bm; bm = bm->bm_next) {
				if (bm->bm_len == -1)
						continue;
				fd = module_open(bm, 0, kdev, module_base, true);
				if (fd == -1)
						continue;
				image_end = (image_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
				len = pread(fd, (void *)(uintptr_t)image_end, SSIZE_MAX);
				if (len < bm->bm_len) {
						if ((howto & AB_SILENT) != 0)
								printf("Loading %s ", bm->bm_path);
						printf(" FAILED\n");
				} else {
						btinfo_modulelist->num++;
						bi = (struct bi_modulelist_entry *)(buf + off);
						off += sizeof(struct bi_modulelist_entry);
						strncpy(bi->path, bm->bm_path, sizeof(bi->path) - 1);
						bi->base = image_end;
						bi->len = len;
						switch (bm->bm_type) {
						case BM_TYPE_KMOD:
								bi->type = BI_MODULE_ELF;
								break;
						case BM_TYPE_IMAGE:
								bi->type = BI_MODULE_IMAGE;
								break;
						case BM_TYPE_FS:
								bi->type = BI_MODULE_FS;
								break;
						case BM_TYPE_RND:
						default:
								/* safest -- rnd checks the sha1 */
								bi->type = BI_MODULE_RND;
								break;
						}
						if ((howto & AB_SILENT) == 0)
								printf(" \n");
				}
				if (len > 0)
						image_end += len;
				close(fd);
		}
		btinfo_modulelist->endpa = image_end;

		if (nfail > 0) {
				printf("WARNING: %d module%s failed to load\n",
					   nfail, nfail == 1 ? "" : "s");
#if notyet
				wait_sec(MODULE_WARNING_SEC);
#endif
		}
}

static void
userconf_init(void)
{
		size_t count, len;
		userconf_command_t *uc;
		char *buf;
		off_t off;

		/* Calculate the userconf commands list size */
		count = 0;
		for (uc = userconf_commands; uc != NULL; uc = uc->uc_next)
				count++;
		len = sizeof(*btinfo_userconfcommands) +
			  count * sizeof(struct bi_userconfcommand);

		/* Allocate the userconf commands list */
		btinfo_userconfcommands = alloc(len);
		if (btinfo_userconfcommands == NULL) {
				printf("WARNING: couldn't allocate userconf commands list\n");
				return;
		}
		memset(btinfo_userconfcommands, 0, len);
		btinfo_userconfcommands_size = len;

		/* Fill in btinfo structure */
		buf = (char *)btinfo_userconfcommands;
		off = sizeof(*btinfo_userconfcommands);
		btinfo_userconfcommands->num = 0;
		for (uc = userconf_commands; uc != NULL; uc = uc->uc_next) {
				struct bi_userconfcommand *bi;
				bi = (struct bi_userconfcommand *)(buf + off);
				strncpy(bi->text, uc->uc_text, sizeof(bi->text) - 1);

				off += sizeof(*bi);
				btinfo_userconfcommands->num++;
		}
}

void
x86_progress(const char *fmt, ...)
{
		va_list ap;

		if ((howto & AB_SILENT) != 0)
				return;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
}
