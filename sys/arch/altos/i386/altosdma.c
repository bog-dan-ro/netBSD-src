#include <sys/param.h>
#include <uvm/uvm.h>

#include <machine/altosdma.h>


uint8_t *liobase = 0;
int newboard;

boolean_t
altosdma_init(void)
{
/*
		map the following Altos DMAs
		liobase				: 0x02200000 // RTC, PIC, LIO & DBG
		liobase + 0x1000	: 0x02100000 // TAG RAM PG1
		liobase + 0x2000	: 0x02101000 // TAG RAM PG2
		liobase + 0x3000	: 0xFE000000 // SIOBASE
		....
		liobase + 0x12000	: 0xFE00F000
*/
		liobase = (uint8_t*) uvm_km_alloc(kernel_map, 0x12000, 0, UVM_KMF_VAONLY);
		if (liobase == 0) {
				return FALSE;
		}
		pmap_kenter_pa((vaddr_t)liobase,		  0x02200000, VM_PROT_READ|VM_PROT_WRITE, 0);
		pmap_kenter_pa((vaddr_t)liobase + 0x1000, 0x02100000, VM_PROT_READ|VM_PROT_WRITE, 0);
		pmap_kenter_pa((vaddr_t)liobase + 0x2000, 0x02101000, VM_PROT_READ|VM_PROT_WRITE, 0);
		for (int i = 0; i < 0x1000; i += PAGE_SIZE) {
				pmap_kenter_pa((vaddr_t)liobase + 0x3000 + i, 0xFE000000 + i, VM_PROT_READ|VM_PROT_WRITE, 0);
		}
		pmap_update(pmap_kernel());

		return TRUE;
}
