/*	$NetBSD: machdep.c,v 1.839 2022/10/26 23:38:07 riastradh Exp $	*/

/*
 * Copyright (c) 1996, 1997, 1998, 2000, 2004, 2006, 2008, 2009, 2017
 *     The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility NASA Ames Research Center, by Julio M. Merino Vidal,
 * by Andrew Doran, and by Maxime Villard.
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
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.839 2022/10/26 23:38:07 riastradh Exp $");

#include "opt_beep.h"
#include "opt_compat_freebsd.h"
#include "opt_compat_netbsd.h"
#include "opt_cpureset_delay.h"
#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_mtrr.h"
#include "opt_modular.h"
#include "opt_physmem.h"
#include "opt_user_ldt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/ucontext.h>
#include <sys/ras.h>
#include <sys/ksyms.h>
#include <sys/device.h>
#include <sys/timevar.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>
#include <dev/mm.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <x86/efi.h>

#include <machine/cpu.h>
#include <machine/cpu_rng.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/gdt.h>
#include <machine/intr.h>
#include <machine/kcore.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <machine/mtrr.h>
#include <machine/pmap_private.h>
#include <x86/x86/tsc.h>

#include <x86/bootspace.h>
#include <x86/fpu.h>
#include <x86/dbregs.h>
#include <x86/machdep.h>

#include <machine/multiboot.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>

#include <ddb/db_active.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#include "acpica.h"

#include "ksyms.h"


/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* CPU "architecture" */
char machine_arch[] = "i386";		/* machine == machine_arch */

#ifdef CPURESET_DELAY
int cpureset_delay = CPURESET_DELAY;
#else
int cpureset_delay = 2000; /* default to 2s */
#endif

int cpu_class;
int use_pae;
int i386_fpu_fdivbug;

int i386_use_fxsave;
int i386_has_sse;
int i386_has_sse2;

vaddr_t idt_vaddr;
paddr_t idt_paddr;
vaddr_t gdt_vaddr;
paddr_t gdt_paddr;
vaddr_t ldt_vaddr;
paddr_t ldt_paddr;

struct vm_map *phys_map = NULL;

extern struct bootspace bootspace;

extern paddr_t lowmem_rsvd;
extern paddr_t avail_start, avail_end;

/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int mem_cluster_cnt = 0;

void init386(paddr_t);
void init_bootspace(void);
void initgdt(union descriptor *);

static void i386_proc0_pcb_ldt_init(void);

int *esym;
int *eblob;
extern int boothowto;

void (*delay_func)(unsigned int) = altos_delay;
void (*x86_initclock_func)(void) = altos_initclocks;

// /*
//  * Main bootinfo structure.  This is filled in by the bootstrap process
//  * done in locore.S based on the information passed by the boot loader.
//  */
struct bootinfo bootinfo;

// /* --------------------------------------------------------------------- */

bool bootmethod_efi;

// static kauth_listener_t x86_listener;

extern paddr_t lowmem_rsvd, avail_start, avail_end;

vaddr_t msgbuf_vaddr;

struct msgbuf_p_seg msgbuf_p_seg[VM_PHYSSEG_MAX];

unsigned int msgbuf_p_cnt = 0;

void
x86_startup(void)
{
}


static struct {
	int freelist;
	uint64_t limit;
} x86_freelists[VM_NFREELIST] = {
	{ VM_FREELIST_DEFAULT, 0 },
#ifdef VM_FREELIST_FIRST1T
	/* 40-bit addresses needed for modern graphics. */
	{ VM_FREELIST_FIRST1T,	1ULL * 1024 * 1024 * 1024 * 1024 },
#endif
#ifdef VM_FREELIST_FIRST64G
	/* 36-bit addresses needed for oldish graphics. */
	{ VM_FREELIST_FIRST64G, 64ULL * 1024 * 1024 * 1024 },
#endif
#ifdef VM_FREELIST_FIRST4G
	/* 32-bit addresses needed for PCI 32-bit DMA and old graphics. */
	{ VM_FREELIST_FIRST4G,  4ULL * 1024 * 1024 * 1024 },
#endif
	/* 30-bit addresses needed for ancient graphics. */
	{ VM_FREELIST_FIRST1G,	1ULL * 1024 * 1024 * 1024 },
	/* 24-bit addresses needed for ISA DMA. */
	{ VM_FREELIST_FIRST16,	16 * 1024 * 1024 },
};

/*
 * Given the type of a bootinfo entry, looks for a matching item inside
 * the bootinfo structure.  If found, returns a pointer to it (which must
 * then be casted to the appropriate bootinfo_* type); otherwise, returns
 * NULL.
 */
void *
lookup_bootinfo(int type)
{
	bool found;
	int i;
	struct btinfo_common *bic;

	bic = (struct btinfo_common *)(bootinfo.bi_data);
	found = FALSE;
	for (i = 0; i < bootinfo.bi_nentries && !found; i++) {
		if (bic->type == type)
			found = TRUE;
		else
			bic = (struct btinfo_common *)
			    ((uint8_t *)bic + bic->len);
	}

	return found ? bic : NULL;
}

/*
 * x86_load_region: load the physical memory region from seg_start to seg_end
 * into the VM system.
 */
static void
x86_load_region(uint64_t seg_start, uint64_t seg_end)
{
	unsigned int i;
	uint64_t tmp;

	i = __arraycount(x86_freelists);
	while (i--) {
		if (x86_freelists[i].limit <= seg_start)
			continue;
		if (x86_freelists[i].freelist == VM_FREELIST_DEFAULT)
			continue;
		tmp = MIN(x86_freelists[i].limit, seg_end);
		if (tmp == seg_start)
			continue;

#ifdef DEBUG_MEMLOAD
		printf("loading freelist %d 0x%"PRIx64"-0x%"PRIx64
		       " (0x%"PRIx64"-0x%"PRIx64")\n", x86_freelists[i].freelist,
		       seg_start, tmp, (uint64_t)atop(seg_start),
		       (uint64_t)atop(tmp));
#endif

		uvm_page_physload(atop(seg_start), atop(tmp), atop(seg_start),
				  atop(tmp), x86_freelists[i].freelist);
		seg_start = tmp;
	}

	if (seg_start != seg_end) {
#ifdef DEBUG_MEMLOAD
		printf("loading default 0x%"PRIx64"-0x%"PRIx64
		       " (0x%"PRIx64"-0x%"PRIx64")\n", seg_start, seg_end,
		       (uint64_t)atop(seg_start), (uint64_t)atop(seg_end));
#endif
		uvm_page_physload(atop(seg_start), atop(seg_end),
				  atop(seg_start), atop(seg_end), VM_FREELIST_DEFAULT);
	}
}

/*
 * init_x86_vm: initialize the VM system on x86. We basically internalize as
 * many physical pages as we can, starting at lowmem_rsvd, but we don't
 * internalize the kernel physical pages (from pa_kstart to pa_kend).
 */
int
init_x86_vm(paddr_t pa_kend)
{
	extern struct bootspace bootspace;
	paddr_t pa_kstart = bootspace.head.pa;
	uint64_t seg_start, seg_end;
	uint64_t seg_start1, seg_end1;
	int x;
	unsigned i;

	for (i = 0; i < __arraycount(x86_freelists); i++) {
		if (avail_end < x86_freelists[i].limit)
			x86_freelists[i].freelist = VM_FREELIST_DEFAULT;
	}

	/*
	 * Now, load the memory clusters (which have already been rounded and
	 * truncated) into the VM system.
	 * 	 * NOTE: we assume that memory starts at 0.
	 */
	for (x = 0; x < mem_cluster_cnt; x++) {
		const phys_ram_seg_t *cluster = &mem_clusters[x];

		seg_start = cluster->start;
		seg_end = cluster->start + cluster->size;
		seg_start1 = 0;
		seg_end1 = 0;

#ifdef DEBUG_MEMLOAD
		printf("segment %" PRIx64 " - %" PRIx64 "\n",
		       seg_start, seg_end);
#endif

		/* Skip memory before our available starting point. */
		if (seg_end <= lowmem_rsvd) {
#ifdef DEBUG_MEMLOAD
			printf("discard segment below starting point "
			       "%" PRIx64 " - %" PRIx64 "\n", seg_start, seg_end);
#endif
			continue;
		}

		if (seg_start <= lowmem_rsvd && lowmem_rsvd < seg_end) {
			seg_start = lowmem_rsvd;
			if (seg_start == seg_end) {
#ifdef DEBUG_MEMLOAD
				printf("discard segment below starting point "
				       "%" PRIx64 " - %" PRIx64 "\n",
				       seg_start, seg_end);


#endif
				continue;
			}
		}

		/*
		 * If this segment contains the kernel, split it in two, around
		 * the kernel.
		 *  [seg_start                       seg_end]
		 *             [pa_kstart  pa_kend]
		 */
		if (seg_start <= pa_kstart && pa_kend <= seg_end) {
#ifdef DEBUG_MEMLOAD
			printf("split kernel overlapping to "
			       "%" PRIx64 " - %" PRIxPADDR " and "
			       "%" PRIxPADDR " - %" PRIx64 "\n",
			       seg_start, pa_kstart, pa_kend, seg_end);
#endif
			seg_start1 = pa_kend;
			seg_end1 = seg_end;
			seg_end = pa_kstart;
			KASSERT(seg_end < seg_end1);
		}

		/*
		 * Discard a segment inside the kernel
		 *  [pa_kstart                       pa_kend]
		 *             [seg_start  seg_end]
		 */
		if (pa_kstart < seg_start && seg_end < pa_kend) {
#ifdef DEBUG_MEMLOAD
			printf("discard complete kernel overlap "
			       "%" PRIx64 " - %" PRIx64 "\n", seg_start, seg_end);
#endif
			continue;
		}

		/*
		 * Discard leading hunk that overlaps the kernel
		 *  [pa_kstart             pa_kend]
		 *            [seg_start            seg_end]
		 */
		if (pa_kstart < seg_start &&
		    seg_start < pa_kend &&
		    pa_kend < seg_end) {
#ifdef DEBUG_MEMLOAD
			printf("discard leading kernel overlap "
			       "%" PRIx64 " - %" PRIxPADDR "\n",
			       seg_start, pa_kend);
#endif
			seg_start = pa_kend;
		}

		/*
		 * Discard trailing hunk that overlaps the kernel
		 *             [pa_kstart            pa_kend]
		 *  [seg_start              seg_end]
		 */
		if (seg_start < pa_kstart &&
		    pa_kstart < seg_end &&
		    seg_end < pa_kend) {
#ifdef DEBUG_MEMLOAD
			printf("discard trailing kernel overlap "
			       "%" PRIxPADDR " - %" PRIx64 "\n",
			       pa_kstart, seg_end);
#endif
			seg_end = pa_kstart;
		}

		/* First hunk */
		if (seg_start != seg_end) {
			x86_load_region(seg_start, seg_end);
		}

		/* Second hunk */
		if (seg_start1 != seg_end1) {
			x86_load_region(seg_start1, seg_end1);
		}
	}

	return 0;
}

void
init_x86_msgbuf(void)
{
	/* Message buffer is located at end of core. */
	psize_t sz = round_page(MSGBUFSIZE);
	psize_t reqsz = sz;
	uvm_physseg_t x;

search_again:
	for (x = uvm_physseg_get_first();
	     uvm_physseg_valid_p(x);
	     x = uvm_physseg_get_next(x)) {

		if (ctob(uvm_physseg_get_avail_end(x)) == avail_end)
			break;
	}

	if (uvm_physseg_valid_p(x) == false)
		panic("init_x86_msgbuf: can't find end of memory");

	/* Shrink so it'll fit in the last segment. */
	if (uvm_physseg_get_avail_end(x) - uvm_physseg_get_avail_start(x) < atop(sz))
		sz = ctob(uvm_physseg_get_avail_end(x) - uvm_physseg_get_avail_start(x));

	msgbuf_p_seg[msgbuf_p_cnt].sz = sz;
	msgbuf_p_seg[msgbuf_p_cnt++].paddr = ctob(uvm_physseg_get_avail_end(x)) - sz;
	uvm_physseg_unplug(uvm_physseg_get_end(x) - atop(sz), atop(sz));

	/* Now find where the new avail_end is. */
	avail_end = ctob(uvm_physseg_get_highest_frame());

	if (sz == reqsz)
		return;

	reqsz -= sz;
	if (msgbuf_p_cnt == VM_PHYSSEG_MAX) {
		/* No more segments available, bail out. */
		printf("WARNING: MSGBUFSIZE (%zu) too large, using %zu.\n",
		    (size_t)MSGBUFSIZE, (size_t)(MSGBUFSIZE - reqsz));
		return;
	}

	sz = reqsz;
	goto search_again;
}

bool
cpu_intr_p(void)
{
	uint64_t ncsw;
	int idepth;
	lwp_t *l;

	l = curlwp;
	if (__predict_false(l->l_cpu == NULL)) {
		KASSERT(l == &lwp0);
		return false;
	}
	do {
		ncsw = l->l_ncsw;
		__insn_barrier();
		idepth = l->l_cpu->ci_idepth;
		__insn_barrier();
	} while (__predict_false(ncsw != l->l_ncsw));

	return idepth >= 0;
}


void (*x86_cpu_idle)(void);
static bool x86_cpu_idle_ipi;
static char x86_cpu_idle_text[16];

SYSCTL_SETUP(sysctl_machdep_cpu_idle, "sysctl machdep cpu_idle")
{
  const struct sysctlnode	*mnode, *node;

  sysctl_createv(NULL, 0, NULL, &mnode,
                 CTLFLAG_PERMANENT, CTLTYPE_NODE, "machdep", NULL,
                 NULL, 0, NULL, 0, CTL_MACHDEP, CTL_EOL);

  sysctl_createv(NULL, 0, &mnode, &node,
                 CTLFLAG_PERMANENT, CTLTYPE_STRING, "idle-mechanism",
                 SYSCTL_DESCR("Mechanism used for the idle loop."),
                 NULL, 0, x86_cpu_idle_text, 0,
                 CTL_CREATE, CTL_EOL);
}

void
x86_cpu_idle_init(void)
{
	if ((cpu_feature[1] & CPUID2_MONITOR) == 0)
		x86_cpu_idle_set(x86_cpu_idle_halt, "halt", true);
	else
		x86_cpu_idle_set(x86_cpu_idle_mwait, "mwait", false);
}

void
x86_cpu_idle_get(void (**func)(void), char *text, size_t len)
{

	*func = x86_cpu_idle;

	(void)strlcpy(text, x86_cpu_idle_text, len);
}

void
x86_cpu_idle_set(void (*func)(void), const char *text, bool ipi)
{

  x86_cpu_idle = func;
  x86_cpu_idle_ipi = ipi;

  (void)strlcpy(x86_cpu_idle_text, text, sizeof(x86_cpu_idle_text));
}

/*
 * Representation of the bootinfo structure constructed by a NetBSD native
 * boot loader.  Only be used by native_loader().
 */
struct bootinfo_source {
	uint32_t bs_naddrs;
	void *bs_addrs[1]; /* Actually longer. */
};

/* Only called by locore.S; no need to be in a header file. */
void native_loader(int, int, struct bootinfo_source *, paddr_t);

/*
 * Called as one of the very first things during system startup (just after
 * the boot loader gave control to the kernel image), this routine is in
 * charge of retrieving the parameters passed in by the boot loader and
 * storing them in the appropriate kernel variables.
 *
 * WARNING: Because the kernel has not yet relocated itself to KERNBASE,
 * special care has to be taken when accessing memory because absolute
 * addresses (referring to kernel symbols) do not work.  So:
 *
 *     1) Avoid jumps to absolute addresses (such as gotos and switches).
 *     2) To access global variables use their physical address, which
 *        can be obtained using the RELOC macro.
 */
void
native_loader(int bl_boothowto, int bl_bootdev,
    struct bootinfo_source *bl_bootinfo, paddr_t bl_esym)
{
#define RELOC(type, x) ((type)((vaddr_t)(x) - KERNBASE))

	*RELOC(int *, &boothowto) = bl_boothowto;

	/*
	 * The boot loader provides a physical, non-relocated address
	 * for the symbols table's end.  We need to convert it to a
	 * virtual address.
	 */
	if (bl_esym != 0)
		*RELOC(int **, &esym) = (int *)((vaddr_t)bl_esym + KERNBASE);
	else
		*RELOC(int **, &esym) = 0;

	/*
	 * Copy bootinfo entries (if any) from the boot loader's
	 * representation to the kernel's bootinfo space.
	 */
	if (bl_bootinfo != NULL) {
		size_t i;
		uint8_t *data;
		struct bootinfo *bidest;
		struct btinfo_modulelist *bi;

		bidest = RELOC(struct bootinfo *, &bootinfo);

		data = &bidest->bi_data[0];

		for (i = 0; i < bl_bootinfo->bs_naddrs; i++) {
			struct btinfo_common *bc;

			bc = bl_bootinfo->bs_addrs[i];

			if ((data + bc->len) >
			    (&bidest->bi_data[0] + BOOTINFO_MAXSIZE))
				break;

			memcpy(data, bc, bc->len);
			/*
			 * If any modules were loaded, record where they
			 * end.  We'll need to skip over them.
			 */
			bi = (struct btinfo_modulelist *)data;
			if (bi->common.type == BTINFO_MODULELIST) {
				*RELOC(int **, &eblob) =
				    (int *)(bi->endpa + KERNBASE);
			}
			data += bc->len;
		}
		bidest->bi_nentries = i;
	}
#undef RELOC
}

/*
 * Machine-dependent startup code
 */
void
cpu_startup(void)
{
	int x, y;
	vaddr_t minaddr, maxaddr;
	psize_t sz;

	/*
	 * For console drivers that require uvm and pmap to be initialized,
	 * we'll give them one more chance here...
	 */
	consinit();

	/*
	 * Initialize error message buffer (et end of core).
	 */
	if (msgbuf_p_cnt == 0)
		panic("msgbuf paddr map has not been set up");
	for (x = 0, sz = 0; x < msgbuf_p_cnt; sz += msgbuf_p_seg[x++].sz)
		continue;

	msgbuf_vaddr = uvm_km_alloc(kernel_map, sz, 0, UVM_KMF_VAONLY);
	if (msgbuf_vaddr == 0)
		panic("failed to valloc msgbuf_vaddr");

	for (y = 0, sz = 0; y < msgbuf_p_cnt; y++) {
		for (x = 0; x < btoc(msgbuf_p_seg[y].sz); x++, sz += PAGE_SIZE)
			pmap_kenter_pa((vaddr_t)msgbuf_vaddr + sz,
			    msgbuf_p_seg[y].paddr + x * PAGE_SIZE,
			    VM_PROT_READ|VM_PROT_WRITE, 0);
	}

	pmap_update(pmap_kernel());

	initmsgbuf((void *)msgbuf_vaddr, sz);

#ifdef MULTIBOOT
	multiboot1_print_info();
	multiboot2_print_info();
#endif

#if NCARDBUS > 0
	/* Tell RBUS how much RAM we have, so it can use heuristics. */
	rbus_min_start_hint(ctob((psize_t)physmem));
#endif

	minaddr = 0;

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, false, NULL);

	/* Say hello. */
	banner();

	/* Safe for i/o port / memory space allocation to use malloc now. */
#if NISA > 0 || NPCI > 0
	x86_bus_space_mallocok();
#endif

	gdt_init();
	i386_proc0_pcb_ldt_init();

	cpu_init_tss(&cpu_info_primary);
	ltr(cpu_info_primary.ci_tss_sel);

	x86_startup();
}

/*
 * Set up proc0's PCB and LDT.
 */
static void
i386_proc0_pcb_ldt_init(void)
{
	struct lwp *l = &lwp0;
	struct pcb *pcb = lwp_getpcb(l);

	pcb->pcb_cr0 = rcr0() & ~CR0_TS;
	pcb->pcb_esp0 = uvm_lwp_getuarea(l) + USPACE - 16;
	pcb->pcb_iopl = IOPL_KPL;
	l->l_md.md_regs = (struct trapframe *)pcb->pcb_esp0 - 1;
	memcpy(&pcb->pcb_fsd, &gdtstore[GUDATA_SEL], sizeof(pcb->pcb_fsd));
	memcpy(&pcb->pcb_gsd, &gdtstore[GUDATA_SEL], sizeof(pcb->pcb_gsd));
	pcb->pcb_dbregs = NULL;

	lldt(GSEL(GLDT_SEL, SEL_KPL));
}

/* XXX */
#define IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);

static void	tss_init(struct i386tss *, void *, void *);

static void
tss_init(struct i386tss *tss, void *stack, void *func)
{
	KASSERT(curcpu()->ci_pmap == pmap_kernel());

	memset(tss, 0, sizeof *tss);
	tss->tss_esp0 = tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	tss->__tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	tss->tss_fs = GSEL(GCPU_SEL, SEL_KPL);
	tss->tss_gs = tss->__tss_es = tss->__tss_ds =
	    tss->__tss_ss = GSEL(GDATA_SEL, SEL_KPL);
	/* %cr3 contains the value associated to pmap_kernel */
	tss->tss_cr3 = rcr3();
	tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	tss->__tss_eflags = PSL_MBO | PSL_NT;	/* XXX not needed? */
	tss->__tss_eip = (int)func;
}

extern vector IDTVEC(tss_trap08);
#if defined(DDB) && defined(MULTIPROCESSOR)
extern vector Xintr_ddbipi, Xintr_x2apic_ddbipi;
extern int ddb_vec;
#endif

void
cpu_set_tss_gates(struct cpu_info *ci)
{
	struct segment_descriptor sd;
	void *doubleflt_stack;
	idt_descriptor_t *idt;

	doubleflt_stack = (void *)uvm_km_alloc(kernel_map, USPACE, 0,
	    UVM_KMF_WIRED);
	tss_init(&ci->ci_tss->dblflt_tss, doubleflt_stack, IDTVEC(tss_trap08));

	setsegment(&sd, &ci->ci_tss->dblflt_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GTRAPTSS_SEL].sd = sd;

	idt = cpu_info_primary.ci_idtvec.iv_idt;
	set_idtgate(&idt[8], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GTRAPTSS_SEL, SEL_KPL));

#if defined(DDB) && defined(MULTIPROCESSOR)
	/*
	 * Set up separate handler for the DDB IPI, so that it doesn't
	 * stomp on a possibly corrupted stack.
	 *
	 * XXX overwriting the gate set in db_machine_init.
	 * Should rearrange the code so that it's set only once.
	 */
	void *ddbipi_stack;

	ddbipi_stack = (void *)uvm_km_alloc(kernel_map, USPACE, 0,
	    UVM_KMF_WIRED);
	tss_init(&ci->ci_tss->ddbipi_tss, ddbipi_stack,
	    x2apic_mode ? Xintr_x2apic_ddbipi : Xintr_ddbipi);

	setsegment(&sd, &ci->ci_tss->ddbipi_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GIPITSS_SEL].sd = sd;

	set_idtgate(&idt[ddb_vec], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GIPITSS_SEL, SEL_KPL));
#endif
}

/*
 * Set up TSS and I/O bitmap.
 */
void
cpu_init_tss(struct cpu_info *ci)
{
	struct cpu_tss *cputss;

	cputss = (struct cpu_tss *)uvm_km_alloc(kernel_map,
	    sizeof(struct cpu_tss), 0, UVM_KMF_WIRED|UVM_KMF_ZERO);

	cputss->tss.tss_iobase = IOMAP_INVALOFF << 16;
	cputss->tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	cputss->tss.tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	cputss->tss.tss_cr3 = rcr3();

	ci->ci_tss = cputss;
	ci->ci_tss_sel = tss_alloc(&cputss->tss);
}

void *
getframe(struct lwp *l, int sig, int *onstack)
{
	struct proc *p = l->l_proc;
	struct trapframe *tf = l->l_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	*onstack = (l->l_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0
	    && (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;
	if (*onstack)
		return (char *)l->l_sigstk.ss_sp + l->l_sigstk.ss_size;
	return (void *)tf->tf_esp;
}

/*
 * Build context to run handler in.  We invoke the handler
 * directly, only returning via the trampoline.  Note the
 * trampoline version numbers are coordinated with machine-
 * dependent code in libc.
 */
void
buildcontext(struct lwp *l, int sel, void *catcher, void *fp)
{
	struct trapframe *tf = l->l_md.md_regs;

	tf->tf_gs = GSEL(GUGS_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = (int)catcher;
	tf->tf_cs = GSEL(sel, SEL_UPL);
	tf->tf_eflags &= ~PSL_CLEARSIG;
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

	/* Ensure FP state is reset. */
	fpu_sigreset(l);
}

void
sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct pmap *pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	int sel = pmap->pm_hiexec > I386_MAX_EXE_ADDR ?
	    GUCODEBIG_SEL : GUCODE_SEL;
	struct sigacts *ps = p->p_sigacts;
	int onstack, error;
	int sig = ksi->ksi_signo;
	struct sigframe_siginfo *fp = getframe(l, sig, &onstack), frame;
	sig_t catcher = SIGACTION(p, sig).sa_handler;

	KASSERT(mutex_owned(p->p_lock));

	fp--;

	memset(&frame, 0, sizeof(frame));
	frame.sf_ra = (int)ps->sa_sigdesc[sig].sd_tramp;
	frame.sf_signum = sig;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_si._info = ksi->ksi_info;
	frame.sf_uc.uc_flags = _UC_SIGMASK|_UC_VM;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = l->l_ctxlink;
	frame.sf_uc.uc_flags |= (l->l_sigstk.ss_flags & SS_ONSTACK)
	    ? _UC_SETSTACK : _UC_CLRSTACK;

	sendsig_reset(l, sig);

	mutex_exit(p->p_lock);
	cpu_getmcontext(l, &frame.sf_uc.uc_mcontext, &frame.sf_uc.uc_flags);
	error = copyout(&frame, fp, sizeof(frame));
	mutex_enter(p->p_lock);

	if (error != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	buildcontext(l, sel, catcher, fp);

	/* Remember that we're now on the signal stack. */
	if (onstack)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
}

static void
maybe_dump(int howto)
{
	int s;

	/* Disable interrupts. */
	s = splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

	splx(s);
}

void
cpu_reboot(int howto, char *bootstr)
{
	static bool syncdone = false;
	int s = IPL_NONE;

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;

	/* XXX used to dump after vfs_shutdown() and before
	 * detaching devices / shutdown hooks / pmf_system_shutdown().
	 */
	maybe_dump(howto);

	/*
	 * If we've panic'd, don't make the situation potentially
	 * worse by syncing or unmounting the file systems.
	 */
	if ((howto & RB_NOSYNC) == 0 && panicstr == NULL) {
		if (!syncdone) {
			syncdone = true;
			/* XXX used to force unmount as well, here */
			vfs_sync_all(curlwp);
			/*
			 * If we've been adjusting the clock, the todr
			 * will be out of synch; adjust it now.
			 *
			 * XXX used to do this after unmounting all
			 * filesystems with vfs_shutdown().
			 */
			if (time_adjusted != 0)
				resettodr();
		}

		while (vfs_unmountall1(curlwp, false, false) ||
		       config_detach_all(boothowto) ||
		       vfs_unmount_forceone(curlwp))
			;	/* do nothing */
	} else {
		if (!db_active)
			suspendsched();
	}

	pmf_system_shutdown(boothowto);

	s = splhigh();

	/* amd64 maybe_dump() */

haltsys:
	doshutdownhooks();

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NACPICA > 0
		if (s != IPL_NONE)
			splx(s);

		acpi_enter_sleep_state(ACPI_STATE_S5);
#else
		__USE(s);
#endif
	}

#ifdef MULTIPROCESSOR
	cpu_broadcast_halt();
#endif /* MULTIPROCESSOR */

	if (howto & RB_HALT) {
#if NACPICA > 0
		acpi_disable();
#endif

		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");

#ifdef BEEP_ONHALT
		{
			int c;
			for (c = BEEP_ONHALT_COUNT; c > 0; c--) {
				sysbeep(BEEP_ONHALT_PITCH,
					BEEP_ONHALT_PERIOD * hz / 1000);
				delay(BEEP_ONHALT_PERIOD * 1000);
				sysbeep(0, BEEP_ONHALT_PERIOD * hz / 1000);
				delay(BEEP_ONHALT_PERIOD * 1000);
			}
		}
#endif

		cnpollc(1);	/* for proper keyboard command handling */
		if (cngetc() == 0) {
			/* no console attached, so just hlt */
			printf("No keyboard - cannot reboot after all.\n");
			for(;;) {
				x86_hlt();
			}
		}
		cnpollc(0);
	}

	printf("rebooting...\n");
	if (cpureset_delay > 0)
		delay(cpureset_delay * 1000);
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

/*
 * Clear registers on exec
 */
void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
	struct pmap *pmap = vm_map_pmap(&l->l_proc->p_vmspace->vm_map);
	struct pcb *pcb = lwp_getpcb(l);
	struct trapframe *tf;

#ifdef USER_LDT
	pmap_ldt_cleanup(l);
#endif

	fpu_clear(l, pack->ep_osversion >= 699002600
	    ? __INITIAL_NPXCW__ : __NetBSD_COMPAT_NPXCW__);

	memcpy(&pcb->pcb_fsd, &gdtstore[GUDATA_SEL], sizeof(pcb->pcb_fsd));
	memcpy(&pcb->pcb_gsd, &gdtstore[GUDATA_SEL], sizeof(pcb->pcb_gsd));

	x86_dbregs_clear(l);

	tf = l->l_md.md_regs;
	tf->tf_gs = GSEL(GUGS_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_edi = 0;
	tf->tf_esi = 0;
	tf->tf_ebp = 0;
	tf->tf_ebx = l->l_proc->p_psstrp;
	tf->tf_edx = 0;
	tf->tf_ecx = 0;
	tf->tf_eax = 0;
	tf->tf_eip = pack->ep_entry;
	tf->tf_cs = pmap->pm_hiexec > I386_MAX_EXE_ADDR ?
	    LSEL(LUCODEBIG_SEL, SEL_UPL) : LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_eflags = PSL_USERSET;
	tf->tf_esp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);
}

/*
 * Initialize segments and descriptor tables
 */

union descriptor *gdtstore, *ldtstore;
union descriptor *pentium_idt;
extern vaddr_t lwp0uarea;

void
setgate(struct gate_descriptor *gd, void *func, int args, int type, int dpl,
    int sel)
{

	gd->gd_looffset = (int)func;
	gd->gd_selector = sel;
	gd->gd_stkcpy = args;
	gd->gd_xx = 0;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (int)func >> 16;
}

void
unsetgate(struct gate_descriptor *gd)
{

	gd->gd_p = 0;
	gd->gd_hioffset = 0;
	gd->gd_looffset = 0;
	gd->gd_selector = 0;
	gd->gd_xx = 0;
	gd->gd_stkcpy = 0;
	gd->gd_type = 0;
	gd->gd_dpl = 0;
}

void
setregion(struct region_descriptor *rd, void *base, size_t limit)
{

	rd->rd_limit = (int)limit;
	rd->rd_base = (int)base;
}

void
setsegment(struct segment_descriptor *sd, const void *base, size_t limit,
    int type, int dpl, int def32, int gran)
{

	sd->sd_lolimit = (int)limit;
	sd->sd_lobase = (int)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (int)limit >> 16;
	sd->sd_xx = 0;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (int)base >> 24;
}

/* XXX */
extern vector IDTVEC(syscall);
extern vector *IDTVEC(exceptions)[];

void
cpu_init_idt(struct cpu_info *ci)
{
	struct region_descriptor region;
	struct idt_vec *iv;
	idt_descriptor_t *idt;

	iv = &ci->ci_idtvec;
	idt = iv->iv_idt_pentium;
	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region);
}

void
init_x86_clusters(void)
{

}

void
initgdt(union descriptor *tgdt)
{
	KASSERT(tgdt != NULL);

	gdtstore = tgdt;
	struct region_descriptor region;
	memset(gdtstore, 0, NGDT * sizeof(*gdtstore));

	/* make gdt gates and memory segments */
	setsegment(&gdtstore[GCODE_SEL].sd, 0, 0xfffff,
	    SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&gdtstore[GDATA_SEL].sd, 0, 0xfffff,
	    SDT_MEMRWA, SEL_KPL, 1, 1);
	setsegment(&gdtstore[GUCODE_SEL].sd, 0, x86_btop(I386_MAX_EXE_ADDR) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdtstore[GUCODEBIG_SEL].sd, 0, 0xfffff,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdtstore[GUDATA_SEL].sd, 0, 0xfffff,
	    SDT_MEMRWA, SEL_UPL, 1, 1);
	// setsegment(&gdtstore[GLIOBASE_SEL].sd, (const void*)0x2200000UL, 0xfff,
	// 	   SDT_MEMRWA, SEL_UPL, 1, 0);
	// setsegment(&gdtstore[GTAG_RAM_SEL].sd, (const void*)0x2100000UL, 0x1fff,
	// 	   SDT_MEMRWA, SEL_UPL, 1, 0);
	// setsegment(&gdtstore[GSIOBASE_SEL].sd, (const void*)0xfe000000UL, 0xffff,
	// 	   SDT_MEMRWA, SEL_UPL, 1, 0);
	setsegment(&gdtstore[GCPU_SEL].sd, &cpu_info_primary,
	    sizeof(struct cpu_info) - 1, SDT_MEMRWA, SEL_KPL, 1, 0);

	setregion(&region, gdtstore, NGDT * sizeof(gdtstore[0]) - 1);
	lgdt(&region);
}

static void
init386_ksyms(void)
{
#if NKSYMS || defined(DDB) || defined(MODULAR)
	extern int end;
	struct btinfo_symtab *symtab;

#ifdef DDB
	db_machine_init();
#endif

#if defined(MULTIBOOT)
	if (multiboot1_ksyms_addsyms_elf())
		return;

	if (multiboot2_ksyms_addsyms_elf())
		return;
#endif

	if ((symtab = lookup_bootinfo(BTINFO_SYMTAB)) == NULL) {
		ksyms_addsyms_elf(*(int *)&end, ((int *)&end) + 1, esym);
		return;
	}

	symtab->ssym += KERNBASE;
	symtab->esym += KERNBASE;
	ksyms_addsyms_elf(symtab->nsym, (int *)symtab->ssym, (int *)symtab->esym);
#endif
}

void
init_bootspace(void)
{
	extern char __rodata_start;
	extern char __data_start;
	extern char __kernel_end;
	size_t i = 0;

	memset(&bootspace, 0, sizeof(bootspace));

	bootspace.head.va = KERNTEXTOFF;
	bootspace.head.pa = KERNTEXTOFF - KERNBASE;
	bootspace.head.sz = 0;

	bootspace.segs[i].type = BTSEG_TEXT;
	bootspace.segs[i].va = KERNTEXTOFF;
	bootspace.segs[i].pa = KERNTEXTOFF - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__rodata_start - KERNTEXTOFF;
	i++;

	bootspace.segs[i].type = BTSEG_RODATA;
	bootspace.segs[i].va = (vaddr_t)&__rodata_start;
	bootspace.segs[i].pa = (paddr_t)(vaddr_t)&__rodata_start - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__data_start - (size_t)&__rodata_start;
	i++;

	bootspace.segs[i].type = BTSEG_DATA;
	bootspace.segs[i].va = (vaddr_t)&__data_start;
	bootspace.segs[i].pa = (paddr_t)(vaddr_t)&__data_start - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__kernel_end - (size_t)&__data_start;
	i++;

	bootspace.boot.va = (vaddr_t)&__kernel_end;
	bootspace.boot.pa = (paddr_t)(vaddr_t)&__kernel_end - KERNBASE;
	bootspace.boot.sz = (size_t)(atdevbase + IOM_SIZE) -
	    (size_t)&__kernel_end;

	/* Virtual address of the top level page */
	bootspace.pdir = (vaddr_t)(PDPpaddr + KERNBASE);
}

void
init386(paddr_t first_avail)
{
	extern void consinit(void);
	int x;
	union descriptor *tgdt;
	struct region_descriptor region;

	struct pcb *pcb;
	struct idt_vec *iv;
	idt_descriptor_t *idt;

	KASSERT(first_avail % PAGE_SIZE == 0);

	uvm_lwp_setuarea(&lwp0, lwp0uarea);

	cpu_probe(&cpu_info_primary);
	cpu_init_msrs(&cpu_info_primary, true);

	use_pae = 0;

	pcb = lwp_getpcb(&lwp0);

	uvm_md_init();

	/*
	 * Start with 2 color bins -- this is just a guess to get us
	 * started.  We'll recolor when we determine the largest cache
	 * sizes on the system.
	 */
	uvmexp.ncolors = 2;

	avail_start = first_avail;

	/*
	 * Low memory reservations:
	 * Page 0:	BIOS data
	 * Page 1:	BIOS callback
	 * Page 2:	MP bootstrap code (MP_TRAMPOLINE)
	 * Page 3:	ACPI wakeup code (ACPI_WAKEUP_ADDR)
	 * Page 4:	Temporary page table for 0MB-4MB
	 * Page 5:	Temporary page directory
	 */

	// On Altos we don't have any reserved memory
	lowmem_rsvd = 0;

#if NISA > 0 || NPCI > 0
	x86_bus_space_init();
#endif

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	printf("Hello there");
	/*
	 * Initialize RNG to get entropy ASAP either from CPU
	 * RDRAND/RDSEED or from seed on disk.  Must happen after
	 * cpu_init_msrs.  Prefer to happen after consinit so we have
	 * the opportunity to print useful feedback.
	 */
	cpu_rng_init();
	// FIXME !!!
	// x86_rndseed();

#ifdef DEBUG_MEMLOAD
	printf("mem_cluster_count: %d\n", mem_cluster_cnt);
#endif

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	pmap_bootstrap((vaddr_t)atdevbase + IOM_SIZE);

	// /* Initialize the memory clusters. */
	init_x86_clusters();

	/* Internalize the physical pages into the VM system. */
	init_x86_vm(avail_start);
	init_x86_msgbuf();

#if !defined(XENPV) && NBIOSCALL > 0
	/*
	 * XXX Remove this
	 *
	 * Setup a temporary Page Table Entry to allow identity mappings of
	 * the real mode address. This is required by bioscall.
	 */
	init386_pte0();

	KASSERT(biostramp_image_size <= PAGE_SIZE);
	pmap_kenter_pa((vaddr_t)BIOSTRAMP_BASE, (paddr_t)BIOSTRAMP_BASE,
	    VM_PROT_ALL, 0);
	pmap_update(pmap_kernel());
	memcpy((void *)BIOSTRAMP_BASE, biostramp_image, biostramp_image_size);

	/* Needed early, for bioscall() */
	cpu_info_primary.ci_pmap = pmap_kernel();
#endif

	pmap_update(pmap_kernel());

	pmap_kenter_pa(idt_vaddr, idt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_kenter_pa(gdt_vaddr, gdt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_kenter_pa(ldt_vaddr, ldt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_update(pmap_kernel());
	memset((void *)idt_vaddr, 0, PAGE_SIZE);
	memset((void *)gdt_vaddr, 0, PAGE_SIZE);
	memset((void *)ldt_vaddr, 0, PAGE_SIZE);

	pmap_update(pmap_kernel());
	iv = &(cpu_info_primary.ci_idtvec);
	idt_vec_init_cpu_md(iv, cpu_index(&cpu_info_primary));
	idt = (idt_descriptor_t *)iv->iv_idt;

	tgdt = gdtstore;
	gdtstore = (union descriptor *)gdt_vaddr;
	ldtstore = (union descriptor *)ldt_vaddr;

	memcpy(gdtstore, tgdt, NGDT * sizeof(*gdtstore));

	setsegment(&gdtstore[GLDT_SEL].sd, ldtstore,
	    NLDT * sizeof(ldtstore[0]) - 1, SDT_SYSLDT, SEL_KPL, 0, 0);

	/* make ldt gates and memory segments */
	ldtstore[LUCODE_SEL] = gdtstore[GUCODE_SEL];
	ldtstore[LUCODEBIG_SEL] = gdtstore[GUCODEBIG_SEL];
	ldtstore[LUDATA_SEL] = gdtstore[GUDATA_SEL];

	/* exceptions */
	for (x = 0; x < 32; x++) {
		/* Reset to default. Special cases below */
		int sel;
		sel = SEL_KPL;

		idt_vec_reserve(iv, x);

 		switch (x) {
		case 3:
		case 4:
			sel = SEL_UPL;
			break;
		default:
			break;
		}
		set_idtgate(&idt[x], IDTVEC(exceptions)[x], 0, SDT_SYS386IGT,
		    sel, GSEL(GCODE_SEL, SEL_KPL));
	}
	//#error fix IDT table !!!
	/* new-style interrupt gate for syscalls */
	idt_vec_reserve(iv, 128);
	set_idtgate(&idt[128], &IDTVEC(syscall), 0, SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	setregion(&region, gdtstore, NGDT * sizeof(gdtstore[0]) - 1);
	lgdt(&region);

	lldt(GSEL(GLDT_SEL, SEL_KPL));
	cpu_init_idt(&cpu_info_primary);

	init386_ksyms();

	intr_default_setup();

	splraise(IPL_HIGH);
	x86_enable_intr();

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; "
		       "have %lu bytes, want %lu bytes\n"
		       "running in degraded mode\n"
		       "press a key to confirm\n\n",
		       (unsigned long)ptoa(physmem), 2*1024*1024UL);
		cngetc();
	}

	pcb->pcb_dbregs = NULL;
	x86_dbregs_init();
}

void
cpu_reset(void)
{
	// FIXME !!!

// 	struct region_descriptor region;
// 	idt_descriptor_t *idt;

// 	idt = (idt_descriptor_t *)cpu_info_primary.ci_idtvec.iv_idt;
// 	x86_disable_intr();

// 	/*
// 	 * Ensure the NVRAM reset byte contains something vaguely sane.
// 	 */

// 	outb(IO_RTC, NVRAM_RESET);
// 	outb(IO_RTC+1, NVRAM_RESET_RST);

// 	/*
// 	 * Reset AMD Geode SC1100.
// 	 *
// 	 * 1) Write PCI Configuration Address Register (0xcf8) to
// 	 *    select Function 0, Register 0x44: Bridge Configuration,
// 	 *    GPIO and LPC Configuration Register Space, Reset
// 	 *    Control Register.
// 	 *
// 	 * 2) Write 0xf to PCI Configuration Data Register (0xcfc)
// 	 *    to reset IDE controller, IDE bus, and PCI bus, and
// 	 *    to trigger a system-wide reset.
// 	 *
// 	 * See AMD Geode SC1100 Processor Data Book, Revision 2.0,
// 	 * sections 6.3.1, 6.3.2, and 6.4.1.
// 	 */
// 	if (cpu_info_primary.ci_signature == 0x540) {
// 		outl(0xcf8, 0x80009044);
// 		outl(0xcfc, 0xf);
// 	}

// 	x86_reset();

// 	/*
// 	 * Try to cause a triple fault and watchdog reset by making the IDT
// 	 * invalid and causing a fault.
// 	 */
// 	memset((void *)idt, 0, NIDT * sizeof(idt[0]));
// 	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
// 	lidt(&region);
// 	breakpoint();

// #if 0
// 	/*
// 	 * Try to cause a triple fault and watchdog reset by unmapping the
// 	 * entire address space and doing a TLB flush.
// 	 */
// 	memset((void *)PTD, 0, PAGE_SIZE);
// 	tlbflush();
// #endif
	for (;;);
}

void
cpu_getmcontext(struct lwp *l, mcontext_t *mcp, unsigned int *flags)
{
	const struct trapframe *tf = l->l_md.md_regs;
	__greg_t *gr = mcp->__gregs;
	__greg_t ras_eip;

	/* Save register context. */
	gr[_REG_GS]  = tf->tf_gs;
	gr[_REG_FS]  = tf->tf_fs;
	gr[_REG_ES]  = tf->tf_es;
	gr[_REG_DS]  = tf->tf_ds;
	gr[_REG_EFL] = tf->tf_eflags;

	gr[_REG_EDI]    = tf->tf_edi;
	gr[_REG_ESI]    = tf->tf_esi;
	gr[_REG_EBP]    = tf->tf_ebp;
	gr[_REG_EBX]    = tf->tf_ebx;
	gr[_REG_EDX]    = tf->tf_edx;
	gr[_REG_ECX]    = tf->tf_ecx;
	gr[_REG_EAX]    = tf->tf_eax;
	gr[_REG_EIP]    = tf->tf_eip;
	gr[_REG_CS]     = tf->tf_cs;
	gr[_REG_ESP]    = tf->tf_esp;
	gr[_REG_UESP]   = tf->tf_esp;
	gr[_REG_SS]     = tf->tf_ss;
	gr[_REG_TRAPNO] = tf->tf_trapno;
	gr[_REG_ERR]    = tf->tf_err;

	if ((ras_eip = (__greg_t)ras_lookup(l->l_proc,
	    (void *) gr[_REG_EIP])) != -1)
		gr[_REG_EIP] = ras_eip;

	*flags |= _UC_CPU;

	mcp->_mc_tlsbase = (uintptr_t)l->l_private;
	*flags |= _UC_TLSBASE;

	/*
	 * Save floating point register context.
	 *
	 * If the cpu doesn't support fxsave we must still write to
	 * the entire 512 byte area - otherwise we leak kernel memory
	 * contents to userspace.
	 * It wouldn't matter if we were doing the copyout here.
	 * So we might as well convert to fxsave format.
	 */
	__CTASSERT(sizeof (struct fxsave) ==
	    sizeof mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
	process_read_fpregs_xmm(l, (struct fxsave *)
	    &mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
	memset(&mcp->__fpregs.__fp_pad, 0, sizeof mcp->__fpregs.__fp_pad);
	*flags |= _UC_FXSAVE | _UC_FPU;
}

int
cpu_mcontext_validate(struct lwp *l, const mcontext_t *mcp)
{
	const __greg_t *gr = mcp->__gregs;
	struct trapframe *tf = l->l_md.md_regs;

	/*
	 * Check for security violations.  If we're returning
	 * to protected mode, the CPU will validate the segment
	 * registers automatically and generate a trap on
	 * violations.  We handle the trap, rather than doing
	 * all of the checking here.
	 */
	if (((gr[_REG_EFL] ^ tf->tf_eflags) & PSL_USERSTATIC) ||
	    !USERMODE(gr[_REG_CS]))
		return EINVAL;

	return 0;
}

int
cpu_setmcontext(struct lwp *l, const mcontext_t *mcp, unsigned int flags)
{
	struct trapframe *tf = l->l_md.md_regs;
	const __greg_t *gr = mcp->__gregs;
	struct proc *p = l->l_proc;
	int error;

	/* Restore register context, if any. */
	if ((flags & _UC_CPU) != 0) {
		error = cpu_mcontext_validate(l, mcp);
		if (error)
			return error;

		tf->tf_gs = gr[_REG_GS];
		tf->tf_fs = gr[_REG_FS];
		tf->tf_es = gr[_REG_ES];
		tf->tf_ds = gr[_REG_DS];
		/* Only change the user-alterable part of eflags */
		tf->tf_eflags &= ~PSL_USER;
		tf->tf_eflags |= (gr[_REG_EFL] & PSL_USER);

		tf->tf_edi    = gr[_REG_EDI];
		tf->tf_esi    = gr[_REG_ESI];
		tf->tf_ebp    = gr[_REG_EBP];
		tf->tf_ebx    = gr[_REG_EBX];
		tf->tf_edx    = gr[_REG_EDX];
		tf->tf_ecx    = gr[_REG_ECX];
		tf->tf_eax    = gr[_REG_EAX];
		tf->tf_eip    = gr[_REG_EIP];
		tf->tf_cs     = gr[_REG_CS];
		tf->tf_esp    = gr[_REG_UESP];
		tf->tf_ss     = gr[_REG_SS];
	}

	if ((flags & _UC_TLSBASE) != 0)
		lwp_setprivate(l, (void *)(uintptr_t)mcp->_mc_tlsbase);

	/* Restore floating point register context, if given. */
	if ((flags & _UC_FPU) != 0) {
		__CTASSERT(sizeof (struct fxsave) ==
		    sizeof mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
		__CTASSERT(sizeof (struct save87) ==
		    sizeof mcp->__fpregs.__fp_reg_set.__fpchip_state);

		if (flags & _UC_FXSAVE) {
			process_write_fpregs_xmm(l, (const struct fxsave *)
				    &mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
		} else {
			process_write_fpregs_s87(l, (const struct save87 *)
				    &mcp->__fpregs.__fp_reg_set.__fpchip_state);
		}
	}

	mutex_enter(p->p_lock);
	if (flags & _UC_SETSTACK)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	mutex_exit(p->p_lock);
	return (0);
}

#define	DEV_IO 14		/* iopl for compat_10 */

int
mm_md_open(dev_t dev, int flag, int mode, struct lwp *l)
{

	switch (minor(dev)) {
	case DEV_IO:
		/*
		 * This is done by i386_iopl(3) now.
		 *
		 * #if defined(COMPAT_10) || defined(COMPAT_FREEBSD)
		 */
		if (flag & FWRITE) {
			struct trapframe *fp;
			int error;

			error = kauth_authorize_machdep(l->l_cred,
			    KAUTH_MACHDEP_IOPL, NULL, NULL, NULL, NULL);
			if (error)
				return (error);
			fp = curlwp->l_md.md_regs;
			fp->tf_eflags |= PSL_IOPL;
		}
		break;
	default:
		break;
	}
	return 0;
}

#ifdef PAE
void
cpu_alloc_l3_page(struct cpu_info *ci)
{
	int ret;
	struct pglist pg;
	struct vm_page *vmap;

	KASSERT(ci != NULL);
	/*
	 * Allocate a page for the per-CPU L3 PD. cr3 being 32 bits, PA musts
	 * resides below the 4GB boundary.
	 */
	ret = uvm_pglistalloc(PAGE_SIZE, 0, 0x100000000ULL, 32, 0, &pg, 1, 0);
	vmap = TAILQ_FIRST(&pg);

	if (ret != 0 || vmap == NULL)
		panic("%s: failed to allocate L3 pglist for CPU %d (ret %d)\n",
			__func__, cpu_index(ci), ret);

	ci->ci_pae_l3_pdirpa = VM_PAGE_TO_PHYS(vmap);

	ci->ci_pae_l3_pdir = (paddr_t *)uvm_km_alloc(kernel_map, PAGE_SIZE, 0,
		UVM_KMF_VAONLY | UVM_KMF_NOWAIT);
	if (ci->ci_pae_l3_pdir == NULL)
		panic("%s: failed to allocate L3 PD for CPU %d\n",
			__func__, cpu_index(ci));

	pmap_kenter_pa((vaddr_t)ci->ci_pae_l3_pdir, ci->ci_pae_l3_pdirpa,
		VM_PROT_READ | VM_PROT_WRITE, 0);

	pmap_update(pmap_kernel());
}
#endif /* PAE */

static void
idt_vec_copy(struct idt_vec *dst, struct idt_vec *src)
{
	idt_descriptor_t *idt_dst;

	idt_dst = dst->iv_idt;
	memcpy(idt_dst, src->iv_idt, PAGE_SIZE);
	memcpy(dst->iv_allocmap, src->iv_allocmap, sizeof(dst->iv_allocmap));
}

void
idt_vec_init_cpu_md(struct idt_vec *iv, cpuid_t cid)
{
	vaddr_t va_idt, va_pentium_idt;
	struct vm_page *pg;

	if (idt_vec_is_pcpu() &&
	    cid != cpu_index(&cpu_info_primary)) {
		va_idt = uvm_km_alloc(kernel_map, PAGE_SIZE,
		    0, UVM_KMF_VAONLY);
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL) {
			panic("failed to allocate pcpu idt PA");
		}
		pmap_kenter_pa(va_idt, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ|VM_PROT_WRITE, 0);
		pmap_update(pmap_kernel());

		memset((void *)va_idt, 0, PAGE_SIZE);

		/* pentium f00f bug stuff */
		va_pentium_idt = uvm_km_alloc(kernel_map, PAGE_SIZE,
		    0, UVM_KMF_VAONLY);
		pmap_kenter_pa(va_pentium_idt, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ, 0);
		pmap_update(pmap_kernel());

		iv->iv_idt = (void *)va_idt;
		iv->iv_idt_pentium = (void *)va_pentium_idt;

		idt_vec_copy(iv, &(cpu_info_primary.ci_idtvec));
	} else {
		iv->iv_idt = (void *)idt_vaddr;
	}
}

void
cpu_initclocks(void)
{

  /*
         * Re-calibrate TSC on boot CPU using most accurate time source,
         * thus making accurate TSC available for x86_initclock_func().
         */
  cpu_get_tsc_freq(curcpu());

  /* Now start the clocks on this CPU (the boot CPU). */
  (*x86_initclock_func)();
}

void
cpu_need_resched(struct cpu_info *ci, struct lwp *l, int flags)
{
	KASSERT(kpreempt_disabled());

	if ((flags & RESCHED_IDLE) != 0) {
		if ((flags & RESCHED_REMOTE) != 0 &&
		    x86_cpu_idle_ipi != false) {
			cpu_kick(ci);
		}
		return;
	}

#ifdef __HAVE_PREEMPTION
	if ((flags & RESCHED_KPREEMPT) != 0) {
		if ((flags & RESCHED_REMOTE) != 0) {
			x86_send_ipi(ci, X86_IPI_KPREEMPT);
		} else {
			softint_trigger(1 << SIR_PREEMPT);
		}
		return;
	}
#endif

	KASSERT((flags & RESCHED_UPREEMPT) != 0);
	if ((flags & RESCHED_REMOTE) != 0) {
		cpu_kick(ci);
	} else {
		aston(l);
	}
}


/*
 * mm_md_physacc: check if given pa is accessible.
 */
int
mm_md_physacc(paddr_t pa, vm_prot_t prot)
{
	extern phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
	extern int mem_cluster_cnt;
	int i;

	for (i = 0; i < mem_cluster_cnt; i++) {
		const phys_ram_seg_t *seg = &mem_clusters[i];
		paddr_t lstart = seg->start;

		if (lstart <= pa && pa - lstart <= seg->size) {
			return 0;
		}
	}
	return kauth_authorize_machdep(kauth_cred_get(),
				       KAUTH_MACHDEP_UNMANAGEDMEM, NULL, NULL, NULL, NULL);
}

void
cpu_signotify(struct lwp *l)
{

	KASSERT(kpreempt_disabled());

	if (l->l_cpu != curcpu()) {
		cpu_kick(l->l_cpu);
	} else {
		aston(l);
	}
}

void
cpu_need_proftick(struct lwp *l)
{

	KASSERT(kpreempt_disabled());
	KASSERT(l->l_cpu == curcpu());

	l->l_pflag |= LP_OWEUPC;
	aston(l);
}
