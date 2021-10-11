/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#ident	"@(#)tty_ptyconf.c	1.8	93/08/24 SMI"
		/* SunOS-4.1 1.2	*/

/*
 * Pseudo-terminal driver.
 *
 * Configuration dependent variables
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/kmem.h>
#include <sys/tty.h>
#include <sys/ptyvar.h>

#ifndef	NOLDPTY
#define	NOLDPTY	48		/* crude XXX */
#endif

int	npty = NOLDPTY;

struct	pty *pty_softc;

struct pollhead ptcph;		/* poll head for ptcpoll() use */

/*
 * Allocate space for data structures at runtime.
 */
void
pty_initspace(void)
{
	pty_softc = (struct pty *)
		kmem_zalloc(npty * sizeof (struct pty), KM_SLEEP);
}
