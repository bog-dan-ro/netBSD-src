#	$NetBSD: Makefile,v 1.43 2012/10/23 16:08:12 dyoung Exp $

# Makefile for i386 tags file and boot blocks

TALTOS=	${SYSDIR}/arch/altos/tags
SALTOS=	${SYSDIR}/arch/altos/acpi/*.[ch] \
	${SYSDIR}/arch/altos/eisa/*.[ch] \
	${SYSDIR}/arch/altos/i386/*.[ch] \
	${SYSDIR}/arch/altos/include/*.h \
	${SYSDIR}/arch/altos/isa/*.[ch] \
	${SYSDIR}/arch/altos/mca/*.[ch] \
	${SYSDIR}/arch/altos/pci/*.[ch] \
	${SYSDIR}/arch/altos/pnpbios/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/dist/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/dist/*/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/ic/*.[ch]
SALTOS+=${SYSDIR}/arch/x86/x86/*.[ch] \
	${SYSDIR}/arch/x86/acpi/*.[ch] \
	${SYSDIR}/arch/x86/include/*.h \
	${SYSDIR}/arch/x86/isa/*.[ch] \
	${SYSDIR}/arch/x86/pci/*.[ch] \
	${SYSDIR}/arch/xen/include/*.h \
	${SYSDIR}/arch/xen/i386/*.[ch] \
	${SYSDIR}/arch/xen/x86/*.[ch] \
	${SYSDIR}/arch/xen/xen/*.[ch] \
	${SYSDIR}/arch/xen/xenbus/*.[ch]
AALTOS=	${SYSDIR}/arch/altos/i386/*.S \
	${SYSDIR}/../common/lib/libc/arch/i386/*/*.S

# Directories in which to place tags links
DALTOS=	i386 eisa isa include pci

.include "../../kern/Make.tags.inc"

tags:
	-rm -f ${TALTOS}
	-echo ${SALTOS} | xargs $(TOOL_CTAGS) -wadtf ${TALTOS}
	-find -H ${SYSDIR}/external/bsd/acpica/dist/ -name '*.[ch]' | \
	    sort -t / -u | xargs $(TOOL_CTAGS) -wadtf ${TALTOS}
	-${FINDCOMM} | xargs $(TOOL_CTAGS) -wadtf ${TALTOS}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AALTOS} | \
	    ${TOOL_SED} -e \
	"s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TALTOS}
	sort -o ${TALTOS} ${TALTOS}

links:
	-for i in ${DALTOS}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done


SUBDIR=	compile include stand ../x86/include ../xen/include

.include <bsd.subdir.mk>
