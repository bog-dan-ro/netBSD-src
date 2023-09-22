#	$NetBSD: Makefile,v 1.48 2020/09/06 07:20:29 mrg Exp $

S?=	${.CURDIR}/../../../..

LIB= i386
NOPIC=# defined
NOPROFILE=# defined

I386_INCLUDE_DISK?= yes
I386_INCLUDE_PS2?= yes

AFLAGS.realprot.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}

CPPFLAGS= -I$S/lib/libsa ${I386CPPFLAGS} ${I386MISCCPPFLAGS}
#CPPFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DNO_GPT
CPPFLAGS+= -DNO_RAIDFRAME
CPPFLAGS+= -DNO_MULTIBOOT2
#CPPFLAGS+= -DSAVE_MEMORY

SRCS= pcio.c conio.S
SRCS+= getsecs.c biosgetrtc.S biosreboot.S
SRCS+= biosmem.S getextmemx.c biosmemx.S printmemlist.c
SRCS+= pread.c menuutils.c parseutils.c
SRCS+= bootinfo.c bootinfo_memmap.c
SRCS+= startprog.S
SRCS+= biosgetsystime.c altosgetsystime.S cpufunc.S bootmenu.c
#SRCS+= realprot.S message.S message32.S dump_eax.S pvcopy.S putstr.S putstr32.S
SRCS+=  dump_eax.S pvcopy.S
SRCS+= biosdisk.c biosdisk_ll.c
CWARNFLAGS.gcc+=	${GCC_NO_ADDR_OF_PACKED_MEMBER}
SRCS+= diskbuf.c

.include <bsd.own.mk>
.undef DESTDIR
.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS:O}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`
