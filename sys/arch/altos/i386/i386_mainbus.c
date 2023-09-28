/*	$NetBSD: i386_mainbus.c,v 1.6 2021/08/07 16:18:55 thorpej Exp $	*/
/*	NetBSD: mainbus.c,v 1.104 2018/12/02 08:19:44 cherry Exp 	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i386_mainbus.c,v 1.6 2021/08/07 16:18:55 thorpej Exp $");

#include <sys/param.h>
#include <machine/autoconf.h>

int	i386_mainbus_match(device_t, cfdata_t, void *);
void	i386_mainbus_attach(device_t, device_t, void *);
int	i386_mainbus_rescan(device_t, const char *, const int *);
/*
 * Probe for the mainbus; always succeeds.
 */
int
i386_mainbus_match(device_t parent, cfdata_t match, void *aux)
{

		return 1;
}

/*
 * Attach the mainbus.
 */
void
i386_mainbus_attach(device_t parent, device_t self, void *aux)
{
		struct mainbus_softc *sc = device_private(self);

		sc->sc_dev = self;

		if (!pmf_device_register(self, NULL, NULL))
				aprint_error_dev(self, "couldn't establish power handler\n");
}


int	mainbus_rescan(device_t, const char *, const int *);
void	mainbus_childdetached(device_t, device_t);
void	mainbus_attach(device_t, device_t, void *);
int	mainbus_match(device_t, cfdata_t, void *);

CFATTACH_DECL2_NEW(mainbus, sizeof(struct mainbus_softc),
				   mainbus_match, mainbus_attach,
				   NULL, NULL,
				   mainbus_rescan, mainbus_childdetached);


int
mainbus_match(device_t parent, cfdata_t match, void *aux)
{
		return 1;
}

void
mainbus_attach(device_t parent, device_t self, void *aux)
{

		aprint_naive("\n");
		aprint_normal("\n");
		i386_mainbus_attach(parent, self, aux);
}

int
mainbus_rescan(device_t self, const char *ifattr, const int *locators)
{
		return ENOTTY; /* Inappropriate ioctl for this device */
}

void
mainbus_childdetached(device_t self, device_t child)
{
}

