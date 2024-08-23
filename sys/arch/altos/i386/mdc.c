#include <machine/mdc.h>


#include <machine/bootinfo.h>
#include <machine/altosdma.h>
#include <machine/i8259.h>
#include <uvm/uvm_prot.h>
#include <uvm/uvm_pmap.h>
#include <machine/pmap.h>

#include <sys/cpu.h>
#include <sys/tty.h>

int nmdc = 0;

int mdc_chans = 0x40;
int mdc_shift = 0;
uint8_t num_mdc_onboard = 0;

int16_t contype = IS_EFPROC;

static int comvect;

uint8_t mdcvecs[MDCVEC];

/* these are all allocated in the startup code, so we don't waste
	memory if no boards are present.  (We allocate enough for
	each board, if more than one board.
*/

mdc_ctrl mdcdev = 0;
chanctrl *mdc_chctrl = 0;
mpscrn *mpsbuf = 0;
chan_parm *chparms  = 0;
struct tty *mdc_tty = 0;
short num_mdcports = 0;
uint8_t mdcdevnum = 0x0a; /* major device number for multidrop	*/
char* mdc_opened = 0;
int* mdcstash = 0;
int *mdcintmask = 0;
uint16_t* mdcss = 0;

uint8_t mdc_attn[NETCHAN];

/* are we using netport for net, or rs232? (GLOBAL)*/
char mdcnetuse[NETCHAN];

mdc_ctrl current_mdcdev = (mdc_ctrl)0xc001fffc;
struct mdc_config mdc_conf;

static struct btinfo_altos_devs *bad;
vaddr_t mdc_space(vaddr_t addr)
{
		int chans;
		nmdc = 0;
		bad = lookup_bootinfo(BTINFO_ALTOS_DEVS);
		if (!bad)
				return 0;
		for (int i = 0; i < NSLOTS; i++) {
				switch (bad->what[i]) {
				case IS_SIO:
				default:
						bad->what[i] = IS_EMPTY;
						break;

				case IS_MDROP: // fallthrough
				case IS_ACPA:
						if (nmdc < MAXMDROPS)
								++nmdc;
						else
								bad->what[i] = IS_EMPTY;
						break;
				}
		}
		mdc_tty = 0;
		if (nmdc == 1)
				chans = 0x18;
		else
				chans = (nmdc - 1) * mdc_chans;
		int32_t size = nmdc * sizeof(struct mdcdev) + chans * 0x51;
		paddr_t start = vtophys(addr);
		paddr_t end = start + size;
		if ((start & 0xfffc0000) != (end & 0xfffc0000)) {
				mdc_tty = (struct tty *)addr;
				addr = addr + chans * sizeof(struct tty);
		}
		start = vtophys(addr);
		end = start + size;
		if ((start & 0xfffc0000) != (end & 0xfffc0000))
				addr = ((addr + 0x40000) & 0xfffc0000);
		mdcdev = (struct mdcdev *)addr;
		mdc_chctrl = (chanctrl *)(mdcdev + nmdc * sizeof(struct mdcdev));
		mpsbuf = (mpscrn *)(mdc_chctrl + chans);
		chparms = (chan_parm *)(mpsbuf + chans);
		addr = (vaddr_t)(chparms + chans);
		if (!mdc_tty) {
				mdc_tty = (struct tty *)addr;
				addr = (vaddr_t)(mdc_tty + chans);
		}
		mdc_opened = (char*)addr;
		mdcstash = (int *)(mdc_opened + chans);
		mdcintmask = mdcstash + chans;
		mdcss = (uint16_t *)(mdcintmask + chans);
		return (vaddr_t)(mdcss + chans);
}

void initboards(void)
{
		struct mdcdev *oldmdcdev;
		oldmdcdev = current_mdcdev;
		comvect = MDCVEC;
		nmdc = 0;
		for (int i = 0; i < NSLOTS; i++) {
				if (bad->what[i] != IS_MDROP && bad->what[i] != IS_ACPA)
						continue;
				mdc_attn[nmdc] = (i == 0 ? 1 : i << 4);
				current_mdcdev = (struct mdcdev *)vtophys((vaddr_t)(mdcdev + i));
				SIOBASE[mdc_attn[nmdc] + 0x4000] = 0x0;
				for (int t = 0; t < 0xfffff && mdcdev[i].mdc_ver0 == 0x0; --i)
						;
				if (mdcdev[i].mdc_ver0 != 0x0) {
						if (contype == 9) {
								contype = 4;
						}
						nmdc = nmdc + 1;
				}
		}
		current_mdcdev = oldmdcdev;
}

static void mdcintr(void)
{

}

void mdcinit(void)
{
		if (!nmdc)
				return;
		if (((mdc_chans > 0x80) || (mdc_chans < 1)) || ((mdc_chans & (mdc_chans - 1U)) != 0)) {
				mdc_chans = 0x40;
		}
		mdc_shift = 0;
		for (int i = 1; i < mdc_chans; i *= 2) {
				++mdc_shift;
		}

		int mcnt = 0;
		struct idt_vec *iv = &(cpu_info_primary.ci_idtvec);
		for (int j = 0; (mcnt < nmdc && (j < 8)); ++j) {
				if ((bad->what[j] == IS_MDROP) || (bad->what[j] == IS_ACPA)) {
						idt_vec_reserve(iv, comvect + j);
						idt_vec_set(iv, comvect + j, mdcintr);
						maxmask = maxmask & ~(uint8_t)(1 << (((char)comvect + (char)j) & 0x1fU));
						mdcvecs[j] = (uint8_t)mcnt++;
				}
		}
		num_mdc_onboard = 0x18;
		int minor = 0;
		static uint16_t mdcerrtime = 0x1e;
		for (int mdidx = 0; mdidx < nmdc; mdidx = mdidx + 1) {
				if (mdidx == 0) {
						minor = 0;
				}
				else {
						minor = (ushort)num_mdc_onboard + (short)((mdidx - 1) << ((uint8_t)mdc_shift & 0x1f));
				}
				/*  enable board */
				mdc_sendcmd(BD_ENABLE,minor,0,0);
				/* minimum # of msecs between 'normal' interrupts */
				mdc_sendcmd(BD_INT_INTVL,minor,0,0x14);
				/* minimum # of msecs between 'error' interrupts */
				mdc_sendcmd(BD_ERR_INTVL,minor,0, mdcerrtime);
		}
		mdc_conf.ttyports = 0x0;
		if (num_mdc_onboard == 0) {
				minor = (short)(0 >> ((uint8_t)mdc_shift & 0x1f)) + 1;
		}
		else {
				minor = 0;
		}
		mdc_sendcmd(BD_CONFIG, minor, vtophys((vaddr_t)&mdc_conf), sizeof(struct mdc_config));

		// tp = mdc_tty;
		mcnt = 0x3ffff;
		while ((mcnt != 0 && (mdc_conf.ttyports == 0))) {
				--mcnt;
		}
		num_mdc_onboard = mdc_conf.ttyports;
		if (mdc_conf.ttyports == 0) {
				num_mdc_onboard = 0x18;
		}
		if (nmdc == 1) {
				num_mdcports = (dev_t)num_mdc_onboard;
		}
		else {
				num_mdcports = (dev_t)((nmdc - 1) << ((uint8_t)mdc_shift & 0x1f));
		}
		// ttysperbrd = (dev_t)num_mdc_onboard;
		// tty_10 = (uint)_mdcdevnum;
		// ttys[_mdcdevnum].tp = mdc_tty;
		// cdevsw[tty_10].d_ttys = tp;
		// ttys[_mdcdevnum].maxd = num_mdcports + -1;
		// tp = mdc_tty;
		// for (i = 0; (short)i < num_mdcports; i = i + 1) {
		// 		tp->t_dev = (ushort)_mdcdevnum << 8 | i;
		// 		/* check me */
		// 		tp->t_proc = mdcproc;
		// 		tp->t_modem = '#';
		// 		tp->t_flow = '\x01';
		// 		pcVar1 = mdc_chctrl;
		// 		mdc_chctrl[(short)i].chc_out = '\0';
		// 		pcVar1[(short)i].chc_in = '\0';
		// 		tp = tp + 1;
		// }
		// if (contype == 4) {
		// 		conssw = mdc_putchar;
		// 		INT_c002a58c = (int)_mdcdevnum;
		// 		no_device_ptr = wtout;
		// 		PTR_c002a590 = wtout;
		// 		for (mcnt = 0; mcnt < 3; mcnt = mcnt + 1) {
		// 				if (cpupresent[mcnt] != 0) {
		// 						minor = (uint16_t)mcnt;
		// 						ttinit(mdc_tty + (short)minor);
		// 						conf_len = 0x1f;
		// 						conf_ptr = svirtophys((caddr_t)(mdc_chctrl + (short)minor));
		// 						/* open async channel */
		// 						mdc_sendcmd(CHANOPEN,minor,(uint32_t)conf_ptr,conf_len);
		// 						mdc_tty[(short)minor].t_state = 0x10;
		// 				}
		// 		}
		// 		/* clear board interrupt (all pending in intarray have been serviced) */
		// 		mdc_sendcmd(BD_CLRINT,0,0,0);
		// 		mdc_initted = mdc_initted + 1;
		// }

}


void mdc_sendcmd(uint8_t cmd, uint16_t md, uint32_t addr, uint16_t len)
{
		mdc_sendcmd_idx(cmd, md, addr, len, 0);
}

void mdc_sendcmd_idx(uint8_t cmd, uint16_t md, uint32_t addr, uint16_t len, int idx)
{

}
