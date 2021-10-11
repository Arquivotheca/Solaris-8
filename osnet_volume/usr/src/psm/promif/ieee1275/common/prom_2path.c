/*
 * Copyright (c) 1991-1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_2path.c	1.6	98/07/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

static int token2path(char *svc, u_int token, char *buf, u_int len);

int
prom_ihandle_to_path(ihandle_t instance, char *buf, u_int len)
{
	return (token2path("instance-to-path", (u_int)instance, buf, len));
}

int
prom_phandle_to_path(phandle_t package, char *buf, u_int len)
{
	return (token2path("package-to-path", (u_int)package, buf, len));
}

static int
token2path(char *service, u_int token, char *buf, u_int len)
{
	cell_t ci[7];
	int rv;
#ifdef PROM_32BIT_ADDRS
	char *obuf = NULL;
#endif

	promif_preprom();
#ifdef PROM_32BIT_ADDRS
	if ((uintptr_t)buf > (uint32_t)-1) {
		obuf = buf;
		buf = promplat_alloc(len);
		if (buf == NULL) {
			promif_postprom();
			return (-1);
		}
	}
#endif

	ci[0] = p1275_ptr2cell(service);	/* Service name */
	ci[1] = 3;				/* #argument cells */
	ci[2] = 1;				/* #return cells */
	ci[3] = p1275_uint2cell(token);		/* Arg1: ihandle/phandle */
	ci[4] = p1275_ptr2cell(buf);		/* Arg2: Result buffer */
	ci[5] = p1275_uint2cell(len);		/* Arg3: Buffer len */
	rv = p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (obuf != NULL) {
		promplat_bcopy(buf, obuf, len);
		promplat_free(buf, len);
	}
#endif
	promif_postprom();

	if (rv != 0)
		return (-1);
	return (p1275_cell2int(ci[6]));		/* Res1: Actual length */
}
