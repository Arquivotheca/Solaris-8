/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_wrtestr.c	1.9	97/06/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Write string to PROM's notion of stdout.
 */
void
prom_writestr(const char *buf, size_t bufsize)
{
	size_t written = 0;
	int i;

	promif_preprom();

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		(OBP_V0_FWRITESTR)((char *)buf, bufsize);
		break;

	default:
		while (written < bufsize)  {
			if ((i =  OBP_V2_WRITE(OBP_V2_STDOUT, (char *)buf,
			    bufsize - written)) == -1)
				continue;
			written += i;
		}
		break;
	}

	promif_postprom();
}
