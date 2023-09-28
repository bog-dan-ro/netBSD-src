#ifndef ALTOSDMA_H
#define ALTOSDMA_H

#include <sys/stdint.h>
#include <sys/types.h>

boolean_t altosdma_init(void);

extern uint8_t *liobase;
#define RTC_BASE liobase;

#define PIC_CMD liobase[0x200];
#define PIC_DATA liobase[0x202];

#define DBG_CTL_B liobase[0xA00];
#define DBG_CTL_A liobase[0xA02];
#define DBG_DATA_B liobase[0xA04];
#define DBG_DATA_A liobase[0xA06];

#define pic_delay() { uint8_t x = DBG_CTL_B; x = DBG_CTL_B; }

#define LIO_PORT_A liobase[0x400];
#define LIO_PORT_0 liobase[0x400];
#define LIO_PORT_1 liobase[0x600];

#define LIO_ENB_CACHE liobase[0x800];
#define LIO_CACHE_MISS liobase[0x802];
#define LIO_CTRLPORT liobase[0x804];
#define LIO_ENB_NMI liobase[0x804];
#define LIO_PARITY liobase[0x808];
#define LIO_OFF_UPS liobase[0x80A];
#define LIO_RST_DBG liobase[0x80C];
#define LIO_UPS_LED liobase[0x80E];
#define LIO_CLR_NMI liobase[0xC00];

#define TAG_RAM (liobase + 4096);
#define TAG_RAMSIZE (2 * 4096);

#define SIOBASE (TAG_RAM + TAG_RAMSIZE);

/* generate a board attention. */
/*
 *  THE CHECK FOR newboard CAN BE REMOVED WHEN ALL CPU BOARDS
 *  HAVE BEEN RE-WORKED.  SEE sysio_chk() IN os/startup.c FOR DETAILS.
 *
 *  For the 1000 the attention offset does not correspond to the board #.
 *  The conversion table follows:
 *  	to interrupt the IOC	board = 0x01
 *  	to interrupt ACPA #1	board = 0x10
 *  	to interrupt ACPA #2	board = 0x20
 *
 *  ACPA-E only works on 'new' mother-boards,
 *  .: no other board attentions are supported for the old mother-boards...kks
 */
#define flush_write_buffers()  { uchar_t x = LIO_PORT_0; }

#define int_ioproc(board) \
{ extern int newboard; \
			if (newboard) { \
					*(char *)(SIOBASE+0x4000+(board)) = 0; \
					flush_write_buffers(); \
		} else \
			outb(0x4001,0); \
}

#endif // ALTOSDMA_H
