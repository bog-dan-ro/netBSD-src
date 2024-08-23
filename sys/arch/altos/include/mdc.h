#ifndef MDC_H
#define MDC_H

#include <sys/types.h>

extern int nmdc;
extern int16_t contype;

vaddr_t mdc_space(vaddr_t addr);
void initboards(void);
void mdcinit(void);
void mdc_sendcmd(uint8_t cmd, uint16_t md, uint32_t addr, uint16_t len);
void mdc_sendcmd_idx(uint8_t cmd, uint16_t md, uint32_t addr, uint16_t len, int idx);

// Adapted from altos include/sys/mdc.h

/*
 *	@(#)mdc.h	2.9	11/15/89 14:30:34
 *
 *	multi-drop (RS232/RS422 comm board) driver
 *  Original author: Dave Olson
 */

#undef NETOPEN	/* something is including net.h, and we don't want
	their definition */

#define CONSOLE		0	/* minor device # for console		*/
#define MDC_RCVSIZ	32	/* receive buffer size -- must be power */
				/*	of 2 				*/
#define M_RCVMSK	(MDC_RCVSIZ-1)	/* mask for keeping index in 	*/
					/*	chc_rbuf in range 	*/

#define RS232PORTS	5	/* RS 232 ports per board (chan 0-4)	*/
#define NETCHAN 	4	/* chan 4 can be jumpered as worknet or */
				/*	RS 232 on 386-2000 		*/
#define NCHANS 256

#define MAXMDROPS 3	/* max number of multidrop boards we support	*/
			/*	(on 1000, 2 and 3 have to be ACPA-E, 	*/
			/*	first is the IOC) i			*/

#define STDCHANS 64	/* standard number of channels per board.	*/
#define MAXCHANS 128	/* max number of channels supported per board.	*/

extern int mdc_chans;       /* channels/board (from master file)	*/
extern int mdc_shift;		/* based on mdc_chans (set in mdcinit)	*/

extern uint8_t num_mdc_onboard;	/* actual # of onboard ports 		*/

/* what board to send cmd to */
# define BDNUM(mdev) ((mdev)<num_mdc_onboard ? 0 : \
	1 + (((mdev)-num_mdc_onboard)>>mdc_shift))

/* usually used to fake a minor # for mdc_sendcmd so that the cmd goes  */
/*	to a pre-determined board */
# define BDTOMDEV(board) ((board)==0?0:num_mdc_onboard+(((board)-1)<<mdc_shift))
# define MAX_ONBOARD 24		/* max on-board for 1000 		*/


/* get the board's attention */
# define MDC_ATTN(board) int_ioproc(mdc_attn[board])

typedef struct cmdentry {
	uint8_t q_cmd;		/* cmd identifier 			*/
	uint8_t q_chan;		/* chan identifier			*/
	uint8_t q_addr[4]; 	/* parm block address (lo is byte 0)	*/
	uint8_t q_len0;		/* buffer length (low byte) 		*/
	uint8_t q_len1;		/* buffer length (high byte) 		*/
} cmdentry;

struct mdc_config	{ 	/* structure must be 32 bytes total 	*/
	uint8_t ttyports;
	uint8_t for_future[31];
};	/* for BD_CONFIG command */

/* value for BD_INT_INTVL, used in both mdc.c and mdcwnet.c */
#define MD_INT_INTVL 20

/* values for q_cmd */
/* general board commands */
#define BD_RESET	0x00	/* reset board 			 	*/
#define BD_ENABLE	0x01	/* enable board				*/
#define BD_CLRINT	0x02	/* clear board interrupt (all pending	*/
				/*	in intarray have been serviced)	*/
#define BD_INT_INTVL	0x03	/* minimum # of msecs between 'normal'	*/
				/*	interrupts.			*/
#define BD_ERR_INTVL	0x04	/* minimum # of seconds between 'error'	*/
				/*	interrupts			*/
#define BD_JMPMON	0x05	/* tell board to jump to monitor prom	*/
				/*	(functionally same as BD_RESET,	*/
				/*	except during board debug)	*/
#define BD_CONFIG	0x06	/* report 'board' configuration 	*/
				/*	(supported only on the 386/1000 */
				/*	for now)			*/
/* 6 - 0xf reserved for future */

#define ERR_INT_INTVL	30	/* error interrupt interval (tuneable)  */

#define	ISCHANCMD(cmd)	(((cmd) & 0xf0) == 0x10)

/* 422 terminal commands */
#define CHANOPEN	0x10	/* open async channel			*/
#define CHANCLOS	0x11	/* close async channel			*/
#define CHANPARM	0x12	/* set channel parameters		*/
#define CHANXMIT	0x13	/* transmit to channel			*/
#define CHANABRT	0x14	/* abort transmit on channel		*/
#define CHANFRGN	0x15	/* start foreign code			*/
#define CHANPOLL	0x16	/* Transmit one byte in 'polled mode',	*/
				/*	normally used only for kernel 	*/
				/*	printf's.			*/
#define CHANXID		0x17	/* tell download code to send XID frame */
				/*	to firmware			*/
#define CHANTEST	0x18	/* XMIT a test frame wth or w/o I-field */
#define CHANDRAIN	0x19	/* Wait for XMIT buffer to drain	*/
#define CHANDRABRT	0x1A	/* Cancel the previous DRAIN command	*/
/* 0x1b - 0x1f reserved for future */

/* net cmds */
#define MNETOPEN	0x20	/* open network				*/
#define MNETXMIT	0x21	/* transmit packet (low speed)		*/
#define MNETFAST	0x22	/* transmit packet (high speed)		*/
#define MNETCLS		0x23	/* close network			*/

#ifdef WSENDLIM
/* so we don't have to include net.h for everything that wants to
	include mdc.h */

/* net xmit buffer structure */
typedef struct {
	uint8_t nx_lenlo, nx_lenhi;	/* packet length		*/
	uint8_t nx_data[WSENDLIM];	/* data to transmit		*/
} mdnetXbuf;

/* net receive buffer structure */
typedef struct {
	uint8_t nr_lenlo, nr_lenhi;	/* board sets to actual packet length */
	uint8_t nr_data[WRECLIM];	/* data buffer filled in by board */
} mdnetRbuf;

/* net control block (same function as chanctrl, but for net ports) */
typedef struct {
	uint8_t n_errs[10];		/* error counts			*/
	uint8_t n_maxlo, n_maxhi;	/* length of n_rcvbuf		*/
	uint8_t n_nbuf;			/* number of net receive buffrs	*/
	uint8_t n_in, n_out; 		/* indices into n_nbuf, n_in 	*/
					/* 	set by board, n_out set */
					/*	by driver 		*/
	uint8_t n_addr;			/* network address */
	mdnetRbuf n_rbuf[NUMWRBUFS];
} mdnetctrl;
#endif /* WSENDLIM */


typedef struct intentry {
	uint8_t int_type;		/* type of interrupt		*/
	uint8_t int_chan;		/* channel requiring service	*/
	uint8_t ch_stat0;
	uint8_t ch_stat1;
	uint8_t unused[4];
} intentry;

/* interrupt types (int_type) */
#define RX_INT 		0x01	/* char(s) received			*/
#define TX_INT		0x02	/* finished copying chars from host	*/
#define	XACK_INT	0x03	/* getxid, exchange test frames int	*/
#define OPEN_ACK_INT	0x04	/* command acknowlegement interrupt	*/
#define INTNETX 	0x05	/* net transmit completed 		*/
#define INTNETR 	0x06	/* net receive completed 		*/
#define INTFRGN 	0x07	/* foreign code interrupt (future) 	*/
#define INT_ERROR 	0x08	/* board is reporting channel errors 	*/
#define INT_BDERR 	0x09	/* board is reporting a fw/hw error	*/
#define INT_NETERR	0x0A	/* board is reporting net errors	*/
#define SSTAT_INT	0x0B	/* serial status interrupt		*/
#define PSTAT_INT	0x0C	/* parallel status interrupt 		*/
#define BAD_LINK_INT	0x0D	/* bad link interrupt (confused state)	*/
#define CMD_ACK_INT	0x0E	/* command acknowlegement interrupt	*/
#define PARM_ACK_INT	0x0F	/* parm acknowlegement interrupt	*/
#define INTQ_FULL_INT	0xFF	/* no more pending interrupts		*/

#define	SERIAL		0	/* possible channel types		*/
#define	IN		1	/* parallel modes			*/
#define	OUT		2
#define INOUT		3

#define OFF		0
#define	ON		1

/* ch_stat0 values returned by dlcode */
#define RESPONDING	0x01	/* channel is responding to poll	*/
#define CMD_SUCCEEDED	0x08	/* command completed successfully	*/

/* ch_stat1 values for serial port */
#define CHDTR		0x01	/* DTR present (always 1 for 422 lines)	*/
#define CHRTS		0x02	/* RTS present (always 1 for 422 lines)	*/
#define CHBREAK		0x04	/* detect start of break		*/
#define CHPARITY	0x08	/* receive parity error			*/
#define CHOVRRUN	0x10	/* receiver overrun (data lost)		*/
#define CHFRAME		0x20	/* framing error			*/
#define CHLOST		0x40	/* lost receive chars from buf ovflow	*/
#define R_ERRS 		(CHPARITY|CHOVRRUN|CHFRAME|CHLOST)

#pragma pack(2)
struct mdcdev {
	uint8_t mdc_ver0, mdc_ver1;
	uint8_t mdc_index;	/* command q input ptr			*/
	uint8_t mdc_out;		/* command q output ptr (set by brd)	*/
	cmdentry mdc_cmds[NCHANS];
	uint8_t head;		/* head pointer advanced by dlcode	*/
	uint8_t tail;		/* tail pointer advanced by host	*/
	intentry mdc_intqueue[NCHANS];
};
#pragma pack()

/* channel control block structure */
typedef struct chanctrl{
	uint8_t unused0, unused1;
	uint8_t chc_errs[16];	/* array with counts of assorted errors	*/
	uint8_t chc_in;		/* receive buffer input index 		*/
	uint8_t chc_out;		/* receive buffer output index 		*/
	uint8_t chc_rbuf[MDC_RCVSIZ];	/* receive buffer 		*/
} chanctrl;

/* channel parameter block */
typedef struct chan_parm {
	uint8_t ch_parm0, ch_parm1;	/* channel parameters 		*/
	uint8_t ch_flow;			/* type of flow control 	*/
	uint8_t ch_xoffchar;		/* suspend output 		*/
	uint8_t ch_xonchar;		/* rstrt output (any chr if FF)	*/
	uint8_t ch_enables;		/* mask of when to interrupt	*/
	uint8_t ch_options;		/* contains ixany bit		*/
} chan_parm;

/* for 422 terminals, ch_parmX are ignored */
/* ch_parm0 */
#define NOPARITY	0x00
#define EVNPAR		0x01
#define ODDPAR		0x02
#define MARKPAR		0x03	/* mark parity ( bit 7 high )		*/
#define SPCPAR		0x04	/* space parity ( bit 7 low )		*/
#define STOPONE 	0x08	/* 1 stop bits				*/
#define STOPONE5 	0x10	/* 1.5 stop bits			*/
#define STOPTWO 	0x18	/* 2 stop bits				*/
#define BIT5 		0x00	/* 5 bits/char				*/
#define BIT6 		0x40	/* 6 bits/char				*/
#define BIT7 		0x20	/* 7 bits/char				*/
#define BIT8 		0x60	/* 8 bits/char				*/
#define BRKENAB		0x80	/* break enable				*/

/* ch_parm0 and ch_stat1 as defined for parallel port */
#define AUTOFEED	0x01
#define ERROR		0x02	/* normally set to 0			*/
#define INIT		0x04
#define SELECT		0x08
#define SELECTED	0x10	/* normally set to 0			*/
#define PAPEROUT	0x20	/* normally set to 0			*/
#define PBUSY		0x40	/* normally set to 0			*/
#define ACK		0x80	/* normally set to 0			*/

/* ch_parm1 */
/*  baud rate goes in bits 0-3 */
#define WANGFLOW	0x20	/* enable special Wang flow control	*/
#define CTSON		0x40	/* assert CTS				*/
#define DSRON		0x80	/* assert DSR 				*/

/* ch_parm1 as defined for parallel port */
/* 	(bit 5 not used) */
#define PLL_MODE	0x1F	/* turn on bits 0-4 for parallel port	*/
#define PLL_IN		0x5F	/* enable Centronics parallel input	*/
#define PLL_OUT		0x9F	/* enable Centronics parallel output	*/
#define PLL_BOTH	0xDF	/* enable both parallel input & output	*/

/* ch_flow */
#define FLODTR		0x01	/* xmit dtr flow control		*/
#define FLORTS		0x02	/* xmit rts flow control		*/
#define	FLOXSFT		0x04	/* xmit software flw control (xon/xoff)	*/
#define FLODSR		0x08	/* rcv dsr flow control	 		*/
#define FLOCTS		0x10	/* rcv cts flow control 		*/
#define	FLORSFT		0x20	/* rcv software flow control (xon/xoff) */
#define KEEPALIVE	0x40	/* if set secondary will NOT go offline */
				/*	even if not polled for >2 mins 	*/
#define INPUTPREFERRED	0x80	/* TCU only: use 'small' buffer for	*/
				/*	output and 'large' buffer for 	*/
				/*	input (i.e. for hi speed input) */

/* ch_enables */
#define E_INTXMIT	0x01	/* interrupt on transmits		*/
#define E_INTRCV	0x02	/* interrupt on receive			*/
#define E_INTDTR	0x04	/* interrupt on dtr change		*/
#define E_INTRTS	0x08	/* interrupt on rts change		*/
#define E_INTBRK	0x10	/* interrupt on break			*/
/* bits 5 -7 reserved */

/* ch_enables as defined for parallel port */
/* bits 0-1 same as above */
/* Parallel out mode */
#define E_INTPAPEROUT	0x04	/* change in paper out status		*/
#define E_INTSELECTED	0x08	/* change in selected status		*/
#define E_INTPLLERROR	0x10	/* change in parallel port error status */
/* Parallel input mode */
#define E_INTSELECT	0x20	/* change in select line       		*/
#define E_INTINIT       0x40    /* change in INIT status                */
#define E_INTAUTOFEED   0x80	/* change in autofeed status            */


/*
 *  structure for handling the print-screen function
 *
 *  There is a copy of this structure for each serial channel.
 *
 *  This structure resides outside of kernel's data space in the clist
 *  segment.
 *  pskey is first in struct for faster dereferencing
 */
typedef struct mpscrn {
	uint8_t pskey[10];		/* print-screen key sequence	*/
	uint8_t  psindx;			/* index of current char in key	*/
					/*	sequence 		*/
	/* number of chars in print-screen key and esc sequences */
	uint8_t pskeylen:4, psesclen:4;
	uint8_t psesc[10];		/* print-screen escape sequence */
} mpscrn;


/* first multidrop interrupt vector.  It is the same as SIOVEC
in sc.h, since mdrop and sio boards share same RANGE of interrupt
vectors. */
# define MDCVEC	1

typedef struct mdcdev *mdc_ctrl;

#define INT_FC(x) (0x10+x)	/* 0x10 to 0x1F are foreign code ints	*/
#define INT_MEM	   0x20		/* Memory command complete		*/
#define INT_FCE(x) 0x30 	/* 0x30 thru 0x3F are frgn code exits	*/
#define INT_FCMASK 0xF0 	/* mask to get frgn code interrupt type */

/* MDC memory manipulation commands					*/
#define	MEM_READ	0x40
#define	MEM_WRITE	0x41
#define	MEM_CNTL	0x42

/* Foreign code commands: 0x50 to 0x5F */
#define FC_CMD(x)	(0x50 + x)


extern uint8_t mdcvecs[];

/* these are all allocated in the startup code, so we don't waste
	memory if no boards are present.  (We allocate enough for
	each board, if more than one board.
*/
extern mdc_ctrl mdcdev;
extern chanctrl *mdc_chctrl;
extern mpscrn *mpsbuf;
extern chan_parm *chparms;
extern struct tty *mdc_tty;
extern short num_mdcports;
extern uint8_t mdcdevnum;	/* major device number for multidrop	*/
extern char* mdc_opened;
extern int* mdcstash;
extern int* mdcintmask;
extern uint16_t* mdcss;
extern mdc_ctrl current_mdcdev;
extern struct mdc_config mdc_conf;
/* defined in mdc.c */
extern uint8_t mdc_attn[];

/* are we using netport for net, or rs232? (GLOBAL)*/
extern char mdcnetuse[];

#define	MDCVER(cont)	(cont->mdc_ver >> 3)
/* multidrop boards have version #'s of MIN_MDC_VERS and above,
	version #'s lower are assigned to sio boards.  (Actually the
	firmware downloaded to the board.)
*/
#define MIN_MDC_VERS 0x10	/* temporary, fix later. 2/86 */

#endif // MDC_H
