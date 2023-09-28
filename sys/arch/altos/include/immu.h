#ifndef IMMU_H
#define IMMU_H


#define	LIOBASE		(0x04000000 + KERNBASE)

#define DBG_CTL_B	(*(uint8_t *)(LIOBASE + 0xA00))
#define DBG_CTL_A	(*(uint8_t *)(LIOBASE + 0xA02))
#define DBG_DATA_B	(*(uint8_t *)(LIOBASE + 0xA04))
#define DBG_DATA_A	(*(uint8_t *)(LIOBASE + 0xA06))

#define LIO_PORT_A	(*(uint8_t *)(LIOBASE+0x400))
#define LIO_PORT_0	(*(uint8_t *)(LIOBASE+0x400))
#define LIO_PORT_1	(*(uint8_t *)(LIOBASE+0x600))

#define LIO_ENB_CACHE	(*(uint8_t *)(LIOBASE+0x800))
#define LIO_CACHE_MISS	(*(uint8_t *)(LIOBASE+0x802))
#define LIO_CTRLPORT	(*(uint8_t *)(LIOBASE+0x804))
#define LIO_ENB_NMI	(*(uint8_t *)(LIOBASE+0x806))
#define LIO_PARITY	(*(uint8_t *)(LIOBASE+0x808))
#define LIO_OFF_UPS	(*(uint8_t *)(LIOBASE+0x80A))
#define LIO_RST_DBG	(*(uint8_t *)(LIOBASE+0x80C))
#define LIO_UPS_LED	(*(uint8_t *)(LIOBASE+0x80E))
#define LIO_CLR_NMI	(*(uint8_t *)(LIOBASE+0xC00))

#define TAG_RAM	(LIOBASE + NBPG)	/* the tag ram */
#define TAG_RAMSIZE	(2 * NBPG)	/* size of the tag ram */

#define SIOBASE (TAG_RAM + TAG_RAMSIZE) /* system I/O space */

#define SIO_END (SIOBASE + 0x10000 - LIOBASE) /* end of system I/O space */

#endif // IMMU_H
