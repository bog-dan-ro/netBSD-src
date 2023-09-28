#ifndef DP8571_H
#define DP8571_H

#include <machine/altosdma.h>

// adapted from altos include/sys/dp8571.h

/*
 *  Register/Counter/RAM Addressing for DP8571
 *
 *  The TCP registers are only one-byte each, but on the 386/1000 they
 *  reside on the even addresses starting at LIOBASE.
 *
 *  The register addresses have multiple uses based upon the setting
 *  of the page select bit (bit 7 of main status register) and the
 *  register select bit (bit 6 of main status register)
 */
#define TC_MSR	  RTC_BASE[0x00] /* main status register */

#define TC_T0CR   RTC_BASE[0x02] /* timer 0 control register */
#define TC_T1CR   RTC_BASE[0x04] /* timer 1 control register */
#define TC_PFR    RTC_BASE[0x06] /* periodic flag register */
#define TC_IRR    RTC_BASE[0x08] /* interrupt routing register */

#define TC_RTMR   RTC_BASE[0x02] /* real time mode register */
#define TC_OMR    RTC_BASE[0x04] /* output mode register */
#define TC_ICR0   RTC_BASE[0x06] /* interrupt control reg 0 */
#define TC_ICR1   RTC_BASE[0x08] /* interrupt control reg 1 */

#define TC_HUND   RTC_BASE[0x0A] /* 1/100 second counter */
#define TC_SEC    RTC_BASE[0x0C] /* second clock counter */
#define TC_MIN    RTC_BASE[0x0E] /* minutes clock counter */
#define TC_HOUR   RTC_BASE[0x10] /* hours clock counter */
#define TC_DOM    RTC_BASE[0x12] /* day of month clock counter */
#define TC_MON    RTC_BASE[0x14] /* months clock counter */
#define TC_YEAR   RTC_BASE[0x16] /* years clock counter */
#define TC_UJUL   RTC_BASE[0x18] /* units Julian clock counter */
#define TC_HJUL   RTC_BASE[0x1A] /* 100's Julian clock counter */
#define TC_DOW    RTC_BASE[0x1C] /* day of week clock counter */
#define TC_T0LO   RTC_BASE[0x1E] /* timer 0 - low byte */
#define TC_T0HI   RTC_BASE[0x20] /* timer 0 - high byte */
#define TC_T1LO   RTC_BASE[0x22] /* timer 1 - low byte */
#define TC_T1HI   RTC_BASE[0x24] /* timer 1 - high byte */
#define TC_SECCR  RTC_BASE[0x26] /* seconds compare RAM */
#define TC_MINCR  RTC_BASE[0x28] /* minutes compare RAM */
#define TC_HRCR   RTC_BASE[0x2A] /* hours compare RAM */
#define TC_DOMCR  RTC_BASE[0x2C] /* day of month compare RAM */
#define TC_MONCR  RTC_BASE[0x2E] /* months compare RAM */
#define TC_DOWCR  RTC_BASE[0x30] /* day of week compare RAM */
#define TC_SECTSR RTC_BASE[0x32] /* seconds time save RAM */
#define TC_MINTSR RTC_BASE[0x34] /* minutes time save RAM */
#define TC_HRTSR  RTC_BASE[0x36] /* hours time save RAM */
#define TC_DOMTSR RTC_BASE[0x38] /* day of month time save RAM */
#define TC_MONTSR RTC_BASE[0x3A] /* months time save RAM */
#define TC_RAM    RTC_BASE[0x3C] /* RAM */
#define TC_RAMTMR RTC_BASE[0x3E] /* RAM/Test Mode Register */

/*
 *  The general purpose static RAM is in the second page on the TCP.
 *  To access the static RAM, we set the register select bit (bit 6
 *  in the main status register).  The RAM starts at location 2
 *  and uses every other byte until location 3E.  There are 31 bytes
 *  of static RAM available.
 */
#define STATIC_RAM (LIOBASE+0x02)

/*
 *  Main Status Register bit definitions
 */
#define MSR_INT   0x01		/* interrupt status */
#define MSR_PF    0x02		/* power fail interrupt */
#define MSR_PER   0x04		/* period interrupt */
#define MSR_AL    0x08		/* alarm interrupt */
#define MSR_T0    0x10		/* timer 0 interrupt */
#define MSR_T1    0x20		/* timer 1 interrupt */
#define MSR_RS    0x40		/* register select bit */
#define MSR_PS    0x80		/* page select bit */

/*
 *  Timer 0 and 1 Control Register bit definitions
 */
#define TCR_TSS   0x01		/* timer start/stop */
#define TCR_MS    0x06		/* mode select */
#define TCR_ICS   0x38		/* input clock select */
#define TCR_RD    0x40		/* timer read */
#define TCR_CHG   0x80		/* count hold/gate */

/*
 *  Mode select definitions (for TCR_MS)
 */
#define TCR_M0    (0x00 << 1)	/* mode 0 */
#define TCR_M1    (0x01 << 1)	/* mode 1 */
#define TCR_M2    (0x02 << 1)	/* mode 2 */
#define TCR_M3    (0x03 << 1)	/* mode 3 */

/*
 *  Input clock select definitions (for TCR_ICS)
 */
#define TCR_T1O	  (0x00 << 3)	/* timer 1 output */
#define TCR_CO 	  (0x01 << 3)	/* crystal oscillator */
#define TCR_CO4	  (0x02 << 3)	/* (crystal oscillator)/4 */
#define TCR_93_5  (0x03 << 3)	/* 93.5 us (10.7 kHz) */
#define TCR_1ms   (0x04 << 3)	/* 1 ms (1 kHz) */
#define TCR_10ms  (0x05 << 3)	/* 10 ms (100 Hz) */
#define TCR_tenth (0x06 << 3)	/* 1/10 second (10 Hz) */
#define TCR_1sec  (0x07 << 3)	/* 1 second (1 Hz) */

/*
 *  Periodic Flag Register bit definitions
 */
#define PFR_1min  0x01		/* minutes flag */
#define PFR_10s   0x02		/* 10 second flag */
#define PFR_1s    0x04		/* seconds flag */
#define PFR_hm    0x08		/* 100 milliseconds flag */
#define PFR_10ms  0x10		/* 10 milliseconds flag */
#define PFR_1ms   0x20		/* milliseconds flag */
#define PFR_OSF   0x40		/* oscillator failed/battery */
#define PFR_TM    0x80		/* test mode enable */

/*
 *  Interrupt Routing Register bit definitions
 */
#define IRR_PFR   0x01		/* power fail route */
#define IRR_PRR   0x02		/* periodic route */
#define IRR_ALR   0x04		/* alarm route */
#define IRR_T0R   0x08		/* timer 0 route */
#define IRR_T1R   0x10		/* timer 1 route */
#define IRR_PFD   0x20		/* power fail delay enable */
#define IRR_LB    0x40		/* low battery flag */
#define IRR_TS    0x80		/* time save enable */

/*
 *  Real Time Mode Register bit definitions
 */
#define RTM_LY    0x03		/* leap year */
#define RTM_12H   0x04		/* 12/24 hour mode */
#define RTM_CSS   0x08		/* clock start/stop */
#define RTM_IPF   0x10		/* interrupt power fail operation */
#define RTM_TPF   0x20		/* timer power fail operation */
#define RTM_CF    0xC0		/* crystal frequency */

/*
 *  Output Mode Register bit definitions
 */
#define OMR_TH    0x01		/* T1 active hi/low (not used on DP8571) */
#define OMR_TP    0x02		/* T1 push pull/open drain (not used           \
														on DP8571) */
#define OMR_IH    0x04		/* INTR active hi/low */
#define OMR_IP    0x08		/* INTR push pull/open drain */
#define OMR_MM    0x10		/* MFO active hi/low */
#define OMR_MP    0x20		/* MFO push pull/open drain */
#define OMR_MT    0x40		/* MFO as Timer 0 output */
#define OMR_MO    0x80		/* MFO pin as oscillator */

/*
 *  Interrupt Control Register 0 bit definitions
 */
#define ICR0_MN    0x01		/* minutes enable */
#define ICR0_TS    0x02		/* 10 second enable */
#define ICR0_S     0x04		/* seconds enable */
#define ICR0_hm    0x08		/* 100 milliseconds enable */
#define ICR0_tm    0x10		/* 10 milliseconds enable */
#define ICR0_1m    0x20		/* milliseconds enable */
#define ICR0_T0    0x40		/* timer 0 enable */
#define ICR0_T1    0x80		/* timer 1 enable */

/*
 *  Interrupt Control Register 1 bit definitions
 */
#define ICR1_SC    0x01		/* second compare enable */
#define ICR1_MN    0x02		/* minute compare enable */
#define ICR1_HR    0x04		/* hour compare enable */
#define ICR1_DOM   0x08		/* day of month compare enable */
#define ICR1_MO    0x10		/* month compare enable */
#define ICR1_DOW   0x20		/* day of week compare enable */
#define ICR1_ALe   0x40		/* alarm interrupt enable */
#define ICR1_PFe   0x80		/* power fail interrupt enable */


#define	ATOBCD(x)	((x)&0xf)		/* ascii to bcd */
#define	BCDTOA(x)	(((x)&0xf) + '0')	/* bcd to ascii */


#endif // DP8571_H

