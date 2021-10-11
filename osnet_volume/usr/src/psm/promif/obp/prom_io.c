/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_io.c	1.11	99/10/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 *  Returns 0 on error. Otherwise returns a handle.
 */
int
prom_open(char *path)
{
	int fd;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		promif_preprom();
		fd = OBP_V0_OPEN(path);
		promif_postprom();
		break;

	default:
		promif_preprom();
		fd = OBP_V2_OPEN(path);
		promif_postprom();
		break;
	}

	return (fd);
}


int
prom_seek(int fd, unsigned long long offset)
{
	int rv;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		return (-1);

	default:
		promif_preprom();
		rv = OBP_V2_SEEK((ihandle_t)fd, (uint_t)(offset >> 32),
		    (uint_t)offset);
		promif_postprom();
		break;
	}

	return (rv);
}


ssize_t
prom_read(ihandle_t fd, caddr_t buf, size_t len, uint_t startblk, char devtype)
{
	int i;

	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION:
		switch (devtype) {
		case BLOCK:
			promif_preprom();
			if ((i = (int)(OBP_V0_READ_BLOCKS((int)fd,
			    len/512, startblk, buf))) == 0)
				i = -1;
			promif_postprom();
			break;

		case NETWORK:
			promif_preprom();
			i = (int)(OBP_V0_POLL_PACKET((int)fd, len, buf));
			promif_postprom();
			break;

		case BYTE:
			promif_preprom();
			if ((i = (int)(OBP_V0_READ_BYTES((int)fd, len, 0,
			    buf))) < 0)
				i = -1;
			promif_postprom();
			break;

		default:
			PROMIF_DPRINTF(("prom_read: bad device type!\n"));
			i = -1;
			break;
		}
		break;
		/* NOTREACHED */

	default:
		promif_preprom();
		i = OBP_V2_READ(fd, buf, len);
		promif_postprom();
	}

	return (i);
}


ssize_t
prom_write(ihandle_t fd, caddr_t buf, size_t len, uint_t startblk, char devtype)
{
	int i;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		switch (devtype) {
		case BLOCK:
			promif_preprom();
			if ((i = (int)(OBP_V0_WRITE_BLOCKS((int)fd,
			    len/512, startblk, buf))) == 0)
				i = -1;
			promif_postprom();
			break;

		case NETWORK:
			promif_preprom();
			i = (int)(OBP_V0_XMIT_PACKET((int)fd, len, buf));
			promif_postprom();
			break;

		case BYTE:
			promif_preprom();
			if ((i = (int)(OBP_V0_WRITE_BYTES((int)fd,
			    len, 0, buf))) < 0)
				i = -1;
			promif_postprom();
			break;

		default:
			PROMIF_DPRINTF(("prom_write: bad device type!\n"));
			i = -1;
			break;
		}
		break;

	default:
		promif_preprom();
		i = OBP_V2_WRITE(fd, buf, len);
		promif_postprom();
		break;
	}

	return (i);
}


int
prom_close(int fd)
{
	int rv;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		promif_preprom();
		rv = (int)(OBP_V0_CLOSE(fd));
		promif_postprom();
		break;

	default:
		promif_preprom();
		rv = (int)(OBP_V2_CLOSE((ihandle_t)fd));
		promif_postprom();
		break;
	}

	return (rv);
}
