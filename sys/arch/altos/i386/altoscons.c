#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: altoscons.c,v 1.63 2023/10/01 12:55:00 BogDan $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/atomic.h>
#include <sys/pserialize.h>

#include <dev/cons.h>
#include <nullcons.h>

//dev_type_open(altos_cnopen);
//dev_type_close(altos_cnclose);
//dev_type_read(altos_cnread);
//dev_type_write(altos_cnwrite);
//dev_type_ioctl(altos_cnioctl);
//dev_type_poll(altos_cnpoll);
//dev_type_kqfilter(cnkqfilter);

//const static struct cdevsw cons_cdevsw = {
//  .d_open = altos_cnopen,
//  .d_close = altos_cnclose,
//  .d_read = altos_cnread,
//  .d_write = altos_cnwrite,
//  .d_ioctl = altos_cnioctl,
//  .d_stop = nostop,
//  .d_tty = notty,
//  .d_poll = nopoll,
//  .d_mmap = nommap,
//  .d_kqfilter = cnkqfilter,
//  .d_discard = nodiscard,
//  .d_flag = D_TTY,
//};

//static void altos_cninit(struct consdev *dev);
static int altos_cngetc(dev_t dev);
static void altos_cnputc(dev_t dev, int c);

static struct consdev altos_cons = {
		//  .cn_init = altos_cninit,
		.cn_getc = altos_cngetc,
		.cn_putc = altos_cnputc,
		.cn_pollc = nullcnpollc,
		.cn_dev = NODEV,
		.cn_pri = CN_REMOTE,
};

//static struct cnm_state altos_cnm_state;

void consinit(void)
{
		cn_set_tab(&altos_cons);
}

//static void altos_cninit(struct consdev *dev)
//{
//  dev->cn_dev = makedev(cdevsw_lookup_major(&cons_cdevsw), 0);
//  cn_init_magic(&altos_cnm_state);
//  cn_set_magic("\047\001");
//}

int altos_getchar(void);
static int altos_cngetc(dev_t dev)
{
		(void)dev;
		return altos_getchar();
}

void altos_putchar(int c);
static void altos_cnputc(dev_t dev, int c)
{
		altos_putchar(c);
}

