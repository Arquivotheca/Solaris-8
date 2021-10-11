/*
 * Copyright (c) 1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_io.c	1.17	98/07/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 *  Returns 0 on error. Otherwise returns a handle.
 */
int
prom_open(char *path)
{
	cell_t ci[5];
#ifdef PROM_32BIT_ADDRS
	char *opath = NULL;
	size_t len;
#endif

	promif_preprom();
#ifdef PROM_32BIT_ADDRS
	if ((uintptr_t)path > (uint32_t)-1) {
		opath = path;
		len = prom_strlen(opath) + 1; /* include terminating NUL */
		path = promplat_alloc(len);
		if (path == NULL) {
			promif_postprom();
			return (0);
		}
		(void) prom_strcpy(path, opath);
	}
#endif

	ci[0] = p1275_ptr2cell("open");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(path);		/* Arg1: Pathname */
	ci[4] = (cell_t)0;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (opath != NULL)
		promplat_free(path, len);
#endif
	promif_postprom();

	return (p1275_cell2int(ci[4]));		/* Res1: ihandle */
}


int
prom_seek(int fd, unsigned long long offset)
{
	cell_t ci[7];

	ci[0] = p1275_ptr2cell("seek");		/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int) fd);	/* Arg1: ihandle */
	ci[4] = p1275_ull2cell_high(offset);	/* Arg2: pos.hi */
	ci[5] = p1275_ull2cell_low(offset);	/* Arg3: pos.lo */
	ci[6] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[6]));		/* Res1: actual */
}

/*ARGSUSED3*/
ssize_t
prom_read(ihandle_t fd, caddr_t buf, size_t len, u_int startblk, char devtype)
{
	cell_t ci[7];
#ifdef PROM_32BIT_ADDRS
	caddr_t obuf = NULL;
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

	ci[0] = p1275_ptr2cell("read");		/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_size2cell((u_int)fd);	/* Arg1: ihandle */
	ci[4] = p1275_ptr2cell(buf);		/* Arg2: buffer address */
	ci[5] = p1275_uint2cell(len);		/* Arg3: buffer length */
	ci[6] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (obuf != NULL) {
		promplat_bcopy(buf, obuf, len);
		promplat_free(buf, len);
	}
#endif
	promif_postprom();

	return (p1275_cell2size(ci[6]));	/* Res1: actual length */
}

/*ARGSUSED3*/
ssize_t
prom_write(ihandle_t fd, caddr_t buf, size_t len, u_int startblk, char devtype)
{
	cell_t ci[7];
#ifdef PROM_32BIT_ADDRS
	caddr_t obuf = NULL;
	static char smallbuf[1];
#endif

	promif_preprom();
#ifdef PROM_32BIT_ADDRS
	if ((uintptr_t)buf > (uint32_t)-1) {
		if (len == 1) {
			/*
			 * This is a hack for kernel printf's... they end
			 * up going through here one character at a time
			 * so we foolishly optimize very small prom_writes
			 * and avoid a trip through promplat_alloc/free.
			 */
			smallbuf[0] = buf[0];
			buf = smallbuf;
		} else {
			obuf = buf;
			buf = promplat_alloc(len);
			if (buf == NULL) {
				promif_postprom();
				return (-1);
			}
			promplat_bcopy(obuf, buf, len);
		}
	}
#endif
	ci[0] = p1275_ptr2cell("write");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int)fd);	/* Arg1: ihandle */
	ci[4] = p1275_ptr2cell(buf);		/* Arg2: buffer address */
	ci[5] = p1275_size2cell(len);		/* Arg3: buffer length */
	ci[6] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (obuf != NULL)
		promplat_free(buf, len);
#endif
	promif_postprom();

	return (p1275_cell2size(ci[6]));	/* Res1: actual length */
}

int
prom_close(int fd)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("close");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int)fd);	/* Arg1: ihandle */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (0);
}
