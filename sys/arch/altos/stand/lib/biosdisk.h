/*	$NetBSD: biosdisk.h,v 1.13 2021/05/30 05:59:23 mlelstv Exp $	*/

/*
 * Copyright (c) 1996
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
 */

extern int boot_biosdev;

enum Drive {
		Drive_HardDiskOld     = 0x0000,
		Drive_HardDisk        = 0x0002,
		Drive_Tape            = 0x0003,
		Drive_FloppyLowSpeed  = 0x0005,
		Drive_FloppyHighSpeed = 0x0006,
};

enum Command {
		Command_FloppyWrite = 0x0001,
		Command_FloppyRead  = 0x0002,
		Command_ReadSCSI    = 0x0008,
		Command_WriteSCSI   = 0x000A,
		Command_WriteTo186  = 0x000B,
		Command_CheckBoard  = 0x0011,
};


enum Status {
		Status_NoError                 = 0x0000,
		Status_DeviceError             = 0x0001,
		Status_DeviceNotImplemented    = 0x0002,
		Status_DeviceNotPresent        = 0x0003,
		Status_InvalidCommand          = 0x0004,
		Status_CommandTimeOut          = 0x0005,
		Status_CommandUnaccepted       = 0x0006,
};

struct DiskIoDMA {
		// 0x00
		uint16_t drive;
		// 0x02
		uint16_t command;
		// 0x04
		uint16_t status; // filled by int_28_1e
							// 0x00 - No Error
							// 0x01 - Device error, status [0123]
							// 0x02 - Device %x not implemented.
							// 0x03 - Device %x not present.
							// 0x04 - Invalid command %x.
							// 0x05 - Timeout error on command %x.
							// 0x06 - Device would not accept command %x.
		// 0x06
		union {
				uint8_t response[4]; // filled by int_28_1e
				uint32_t errorDetail; // filled by int_28_1e
		}__packed;
		// 0x0a
		uint32_t addr;
		// 0x0e
		struct {
				uint8_t id[2];
				uint8_t blk_id[3];
				uint8_t blk_count;
				uint8_t fill[2]; // fill with 0x0
		} hdd;
		// 0x16
		uint8_t fill[8]; // fill up to 0x1e
}__packed;

extern struct DiskIoDMA g_disk_io;

uint16_t get_hdd_id(void);
void set_hdd_id(uint16_t id);

#define BIOSDISK_PART_NAME_LEN 36

struct biosdisk_partition {
		daddr_t offset;
		daddr_t size;
		int     fstype;
#ifndef NO_GPT
		const struct gpt_part {
				const struct uuid *guid;
				const char *name;
		} *guid;
		uint64_t attr;
		char *part_name; /* maximum BIOSDISK_PART_NAME_LEN */
#endif
};

extern struct btinfo_bootdisk bi_disk; 
extern struct btinfo_bootwedge bi_wedge;
extern struct btinfo_rootdevice bi_root;

int biosdisk_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int biosdisk_open(struct open_file *, ...);
int biosdisk_open_name(struct open_file *, const char *);
int biosdisk_close(struct open_file *);
int biosdisk_ioctl(struct open_file *, u_long, void *);
int biosdisk_findpartition(int, int *, const char **);
int biosdisk_readpartition(int, daddr_t, daddr_t,
					   struct biosdisk_partition **, int *);

struct RF_ComponentLabel_s;
int biosdisk_read_raidframe(int, daddr_t, struct RF_ComponentLabel_s *);

#if !defined(NO_GPT)
struct uuid;
bool guid_is_nil(const struct uuid *);
bool guid_is_equal(const struct uuid *, const struct uuid *);
#endif
