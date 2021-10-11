/*
 * Copyright (c) 1995,1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_version.c	1.3	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/obpdefs.h>
#include <sys/types.h>
#include <sys/kmem.h>

#define	PROM_VERSION_NUMBER 3   /* Fake version number (1275-like) */

extern int emul_1275;
extern dnode_t prom_chosennode(void);
extern dnode_t prom_rootnode(void);

int
prom_getversion(void)
{
	return (PROM_VERSION_NUMBER);
}

int
prom_is_openprom(void)
{
	return (emul_1275);
}

int
prom_is_p1275(void)
{
	return (emul_1275);
}

/*
 * return a string representing the prom version (In this case
 * a string representing the version of devconf that created
 * the device tree will be returned).
 */
int
prom_version_name(char *buf, int len)
{
	dnode_t pnp;
	char *verp, *pbuf = NULL;
	int plen;

	pnp = prom_chosennode();
	if (pnp == OBP_BADNODE)
		verp = "unknown";
	else {
		if ((plen = prom_getproplen(pnp, "devconf-version")) < 0)
			verp = "unknown";
		else {
			pbuf = kmem_zalloc(plen + 1, KM_SLEEP);
			(void) prom_getprop(pnp, "devconf-version", pbuf);
			pbuf[plen] = '\0';
			verp = pbuf;
		}
	}
	(void) prom_strncpy(buf, verp, len - 1);
	buf[len - 1] = '\0';
	if (pbuf != NULL)
		kmem_free(pbuf, plen + 1);
	return (prom_strlen(buf));
}
