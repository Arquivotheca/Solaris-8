/*
 * Copyright (c) 1985-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)obp_devio.c	1.10	95/06/27 SMI"

/*
 * Firmware interface code for standalone I/O system.
 *
 * For OpenBoot romvec version 0, 2 and 3 only.
 * Will NOT work with OpenFirmware/IEEE 1275 client interface.
 */

#include <sys/param.h>
#include <sys/openprom.h>
#include <sys/comvec.h>
#include <sys/prom_plat.h>
#include <sys/promif.h>

#include "cbootblk.h"

/*
 * XXX	Should be 'static'; 'extern' definition in header files prevent this
 */
union sunromvec *romp;

#define	OBP_ROMVEC_VERSION	(romp->obp.op_romvec_version)

void
fw_init(void *ptr)
{
	romp = ptr;
}

void
exit()
{
	OBP_EXIT_TO_MON();
}

dnode_t
prom_rootnode(void)
{
	return (OBP_DEVR_NEXT(0));
}

int
prom_getproplen(dnode_t n, char *nm)
{
	return (OBP_DEVR_GETPROPLEN(n, nm));
}

int
prom_getprop(dnode_t n, char *nm, caddr_t buf)
{
	return (OBP_DEVR_GETPROP(n, nm, buf));
}

static void
putchar(char c)
{
	while (OBP_V2_WRITE(OBP_V2_STDOUT, &c, 1) != 1)
		;
}

void
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


/*
 * unsigned int to hex/ascii
 */
int
utox(char *p, u_int n)
{
	char buf[32];
	char *cp = buf;
	char *bp = p;

	do {
		*cp++ = "0123456789abcdef"[n & 0xf];
		n >>= 4;
	} while (n);

	do {
		*bp++ = *--cp;
	} while (cp > buf);

	*bp = (char)0;
	return (bp - p);
}

/*
 * Return the device name of the boot slice.
 *
 * If the 'slice' string is set, return the boot device
 * specifier for that slice name on the same disk.
 *
 * Valid 'slice' names obey SunOS conventions: '0' -> '7'
 * We translate them to OBP conventions ('a' -> 'h') here.
 */
char *
getbootdevice(char *slice)
{
	static char bootpath[OBP_MAXPATHLEN];

	if (OBP_ROMVEC_VERSION == OBP_V0_ROMVEC_VERSION) {

		struct bootparam *bp = OBP_V0_BOOTPARAM;
		char *p = bootpath;

		*p++ = bp->bp_dev[0];
		*p++ = bp->bp_dev[1];
		*p++ = '(';
		p += utox(p, bp->bp_ctlr);
		*p++ = ',';
		p += utox(p, bp->bp_unit);
		*p++ = ',';
		if (slice) {
			/*
			 * A hack - we assume that the
			 * slice name is a single digit between '0' and '9'
			 */
			*p++ = *slice;
		} else
			p += utox(p, bp->bp_part);
		*p++ = ')';
		*p = (char)0;
	} else {
		(void) strcpy(bootpath, OBP_V2_BOOTPATH);

		if (slice) {
			size_t blen;
			char *p;
			char *head, *tail;
			char letter[2];

			/*
			 * A hack - we assume that the
			 * slice name is a digit between '0' and '9'
			 */
			letter[0] = *slice - '0' + 'a';
			letter[1] = '\0';

			blen = strlen(head = bootpath);
			for (p = tail = &bootpath[blen];
			    p != head && *p != '/'; p--)
				if (*p == ':')
					break;
			if (*p == ':')
				(void) strcpy(p + 1, letter);
			else {
				*tail = ':';
				(void) strcpy(tail + 1, letter);
			}
		}
	}
	return (bootpath);
}

void *
devopen(char *devname)
{
	static int romdev;

	if (OBP_ROMVEC_VERSION == OBP_V0_ROMVEC_VERSION)
		romdev = OBP_V0_OPEN(devname);
	else
		romdev = OBP_V2_OPEN(devname);
	if (romdev == 0)
		return (NULL);
	return (&romdev);
}

int
devbread(void *handle, void *buf, int blkno, int size)
{
	if (OBP_ROMVEC_VERSION == OBP_V0_ROMVEC_VERSION) {
		int fd = *(int *)handle;

		return ((OBP_V0_READ_BLOCKS(fd, size >> DEV_BSHIFT,
		    blkno, buf)) << DEV_BSHIFT);
	} else {
		ihandle_t ih = *(ihandle_t *)handle;

		if (OBP_V2_SEEK(ih, 0, blkno << DEV_BSHIFT) == -1)
			return (0);
		return (OBP_V2_READ(ih, buf, size));
	}
}

int
devclose(void *handle)
{
	if (OBP_ROMVEC_VERSION == OBP_V0_ROMVEC_VERSION)
		return ((int)OBP_V0_CLOSE(*(int *)handle));
	else
		return ((int)OBP_V2_CLOSE(*(ihandle_t *)handle));
}
