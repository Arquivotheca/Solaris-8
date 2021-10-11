/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)stub.c	1.1	99/05/28 SMI"

/*
 * Stub for deprecated OBP V0 and V2 systems (sun4c).
 */

#include <sys/param.h>
#include <sys/openprom.h>
#include <sys/comvec.h>
#include <sys/promif.h>

/*
 * XXX	Should be 'static'; 'extern' definition in header files prevent this
 */
union sunromvec *romp;

#define	OBP_ROMVEC_VERSION	(romp->obp.op_romvec_version)

static void
fw_init(void *ptr)
{
	romp = ptr;
}

void
exit()
{
	OBP_EXIT_TO_MON();
}

static void
putchar(char c)
{
	while (OBP_V2_WRITE(OBP_V2_STDOUT, &c, 1) != 1)
		;
}

static void
puts(char *msg)
{
	char c;

	if (OBP_ROMVEC_VERSION == OBP_V0_ROMVEC_VERSION)
		OBP_V0_PRINTF(msg);
	else {
		/* prepend carriage return to linefeed */
		while ((c = *msg++) != '\0') {
			if (c == '\n')
				putchar('\r');
			putchar(c);
		}
	}
}

void
main(void *ptr)
{
	fw_init(ptr);
	puts("This hardware platform is not supported by this "
	    "release of Solaris.\n");
}

void
bzero(void *p, size_t n)
{
	char	zeero	= 0;
	char	*cp	= p;

	while (n != 0)
		*cp++ = zeero, n--;	/* Avoid clr for 68000, still... */
}
