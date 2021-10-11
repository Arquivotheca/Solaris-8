/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ieee1275_devio.c	1.8	97/11/22 SMI"

/*
 * Firmware interface code for standalone I/O system.
 *
 * For OpenFirmware/IEEE 1275 client interface only.
 * Will NOT work for OpenBoot romvec version 0, 2 and 3.
 */

#include <sys/param.h>
#include <sys/promif.h>
#include <sys/promimpl.h>

#include "cbootblk.h"

static int (*cif_handler)(void *);



#ifdef DEBUG

void
dputs(char *s, u_int x)
{
	char p[16];
	utox(p, x);
	puts(s);
	puts(p);
	puts("\n");
}

#endif /* DEBUG */


void
fw_init(void *ptr)
{
	cif_handler = (int (*)(void *))ptr;
}


/*
 * If /chosen node has memory property implemented we will use this.
 */
ihandle_t
prom_memory_ihandle(void)
{
	static ihandle_t imemory;

	if (imemory != (ihandle_t)0)
		return (imemory);

	if (prom_getproplen(prom_chosennode(), "memory") != sizeof (ihandle_t))
		return (imemory = (ihandle_t)-1);

	(void) prom_getprop(prom_chosennode(), "memory", (caddr_t)(&imemory));
	imemory = (ihandle_t)prom_decode_int(imemory);
	return (imemory);
}


/*
 * Claim a region of physical memory, unmapped.
 * Returns 0: Success; Non-zero: failure.
 *
 * This routine is suitable for platforms with 1-cell physical addresses
 * and a single size cell in the "memory" node.
 */
int
prom_claim_phys(u_int size, u_int addr)
{
	cell_t ci[10];
	int rv;
	ihandle_t imemory = prom_memory_ihandle();

	if ((imemory == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");  /* Service name */
	ci[1] = (cell_t)5;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell("claim");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(imemory);	/* Arg2: mmu ihandle */
	ci[5] = 0;				/* Arg3: align */
	ci[6] = p1275_uint2cell(size);		/* Arg4: len */
	ci[7] = p1275_uint2cell(addr);	/* Arg5: addr */

	rv = p1275_cif_handler(&ci);

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[8]) != 0)	 /* Res1: Catch result */
		return (-1);

	return (0);
}

void
exit(void)
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("exit");		/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */

	(void) p1275_cif_handler(&ci);
}

dnode_t
prom_rootnode(void)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("peer");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_dnode2cell(OBP_NONODE);	/* Arg1: input phandle */
	ci[4] = p1275_dnode2cell(OBP_NONODE);	/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2dnode(ci[4]));	/* Res1: peer phandle */
}

int
prom_getproplen(dnode_t nodeid, char *name)
{
	cell_t ci[6];

	ci[0] = p1275_ptr2cell("getproplen");	/* Service name */
	ci[1] = (cell_t)2;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #return cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid);	/* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: Property name */
	ci[5] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2int(ci[5]));		/* Res1: Property length */
}

int
prom_bounded_getprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	cell_t ci[8];

	ci[0] = p1275_ptr2cell("getprop");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid); /* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: property name */
	ci[5] = p1275_ptr2cell(value);		/* Arg3: buffer address */
	ci[6] = p1275_int2cell(len);		/* Arg4: buffer length */
	ci[7] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2int(ci[7]));		/* Res1: Returned length */
}


int
prom_getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	int len, rv;
	cell_t ci[8];

	/*
	 * This function assumes the buffer is large enough to
	 * hold the result, so in 1275 mode, we pass in the length
	 * of the property as the length of the buffer, since we
	 * have no way of knowing the size of the buffer. Pre-1275
	 * OpenBoot(tm) PROMs did not have a bounded getprop.
	 *
	 * Note that we ignore the "length" result of the service.
	 */

	if ((len = prom_getproplen(nodeid, name)) <= 0)
		return (len);

	ci[0] = p1275_ptr2cell("getprop");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_phandle2cell((phandle_t)nodeid);	/* Arg1: package */
	ci[4] = p1275_ptr2cell(name);		/* Arg2: property name */
	ci[5] = p1275_ptr2cell(value);		/* Arg3: buffer address */
	ci[6] = len;				/* Arg4: buf len (assumed) */

	rv = p1275_cif_handler(&ci);
	if (rv != 0)
		return (-1);
	return (len);				/* Return known length */
}

dnode_t
prom_finddevice(char *path)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("finddevice");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(path);		/* Arg1: pathname */
	ci[4] = p1275_dnode2cell(OBP_NONODE);	/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return ((dnode_t)p1275_cell2dnode(ci[4])); /* Res1: phandle */
}

dnode_t
prom_chosennode(void)
{
	static dnode_t chosen;
	dnode_t	node;

	if (chosen)
		return (chosen);

	node = prom_finddevice("/chosen");

	if (node != OBP_BADNODE)
		return (chosen = node);

	puts("Could not find </chosen> node\n");
	exit();
	/*NOTREACHED*/
}

/*
 * Return ihandle of stdout
 */
ihandle_t
prom_stdout_ihandle(void)
{
	static ihandle_t istdout = 0;
	static char *name = "stdout";

	if (istdout)
		return (istdout);

	if (prom_getproplen(prom_chosennode(), name) !=
	    sizeof (ihandle_t))  {
		return (istdout = (ihandle_t)-1);
	}
	(void) prom_getprop(prom_chosennode(), name, (caddr_t)(&istdout));
	istdout = (ihandle_t)prom_decode_int(istdout);
	return (istdout);

}

char *
prom_bootargs(void)
{
	int length;
	dnode_t node;
	static char *name = "bootargs";
	static char bootargs[OBP_MAXPATHLEN];

	if (bootargs[0] != (char)0)
		return (bootargs);

	node = prom_chosennode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE))
		node = prom_rootnode();
	length = prom_getproplen(node, name);
	if ((length == -1) || (length == 0))
		return (NULL);
	if (length > OBP_MAXPATHLEN)
		length = OBP_MAXPATHLEN - 1;	/* Null terminator */
	(void) prom_bounded_getprop(node, name, bootargs, length);
	return (bootargs);
}

char *
prom_bootpath(void)
{
	static char bootpath[OBP_MAXPATHLEN];
	int length;
	dnode_t node;
	static char *name = "bootpath";

	if (bootpath[0] != (char)0)
		return (bootpath);

	node = prom_chosennode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE))
		node = prom_rootnode();
	length = prom_getproplen(node, name);
	if ((length == -1) || (length == 0))
		return (NULL);
	if (length > OBP_MAXPATHLEN)
		length = OBP_MAXPATHLEN - 1;	/* Null terminator */
	(void) prom_bounded_getprop(node, name, bootpath, length);
	return (bootpath);
}

/*
 *  Returns 0 on error. Otherwise returns a handle.
 */
int
prom_open(char *path)
{
	ihandle_t ih;
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("open");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(path);		/* Arg1: Pathname */
	ci[4] = (cell_t)0;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	ih = p1275_ihandle2cell(ci[4]);

	return (ih);		/* Res1: ihandle */
}

/*ARGSUSED3*/
int
prom_read(ihandle_t fd, caddr_t buf, u_int len, u_int startblk, char devtype)
{
	cell_t ci[7];

	ci[0] = p1275_ptr2cell("read");		/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int) fd);	/* Arg1: ihandle */
	ci[4] = p1275_ptr2cell(buf);		/* Arg2: buffer address */
	ci[5] = p1275_uint2cell(len);		/* Arg3: buffer length */
	ci[6] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2int(ci[6]));		/* Res1: actual length */
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

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2int(ci[6]));		/* Res1: actual */
}

int
prom_close(int fd)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("close");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int)fd);	/* Arg1: ihandle */

	(void) p1275_cif_handler(&ci);

	return (0);
}

static int
putchar(char c)
{
	cell_t ci[7];
	char	buf[1];
	static int	fd = -1;

	if (fd == -1)
		fd = prom_stdout_ihandle();

	buf[0] = c;

	ci[0] = p1275_ptr2cell("write");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_uint2cell((u_int)fd);	/* Arg1: ihandle */
	ci[4] = p1275_ptr2cell(buf);		/* Arg2: buffer address */
	ci[5] = p1275_uint2cell(1);		/* Arg3: buffer length */
	ci[6] = (cell_t)-1;			/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

	return (p1275_cell2int(ci[6]));		/* Res1: actual length */
}

caddr_t
prom_alloc(caddr_t virt, u_int size, u_int align)
{
	cell_t ci[7];
	int rv;

	ci[0] = p1275_ptr2cell("claim");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(virt);		/* Arg1: virt */
	ci[4] = p1275_uint2cell(size);		/* Arg2: size */
	ci[5] = p1275_uint2cell(align);		/* Arg3: align */

	rv = p1275_cif_handler(&ci);

	if (rv == 0)
		return ((caddr_t)p1275_cell2ptr(ci[6])); /* Res1: base */
	return ((caddr_t)-1);
}

void
puts(char *msg)
{
	char c;

	/* prepend carriage return to linefeed */
	while ((c = *msg++) != '\0') {
		if (c == '\n')
			while (putchar('\r') == -1)
				;
		while (putchar(c) == -1)
			;
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
	size_t blen;
	char *c, *p;
	char *head, *tail;
	char letter[2];

	(void) strcpy(bootpath, prom_bootpath());

	/*
	 * Find the ':' in the device name
	 */
	blen = strlen(head = bootpath);
	for (p = tail = &bootpath[blen];
	    p != head && *p != '/'; p--)
		if (*p == ':')
			break;

	/*
	 * Find the ',' after the device and strip off the boot program
	 * which is us :-)
	 */
	for (c = p; *c != '\0'; c++)
		if (*c == ',') {
			*c = '\0';
			break;
		}

	if (slice) {
		/*
		 * A hack - we assume that the
		 * slice name is a digit between '0' and '9'
		 */
		letter[0] = *slice - '0' + 'a';
		letter[1] = '\0';

		if (*p == ':')
			(void) strcpy(p + 1, letter);
		else {
			*tail = ':';
			(void) strcpy(tail + 1, letter);
		}
	}
	return (bootpath);
}

void *
devopen(char *devname)
{
	get_rootfs_start(devname);
	return ((void *)prom_open(devname));
}

int
devbread(void *handle, void *buf, int blkno, int size)
{
	ihandle_t ih = (ihandle_t)handle;
	u_int real_blkno;

	real_blkno = fdisk2rootfs(blkno);

	if (prom_seek(ih, (unsigned long long)real_blkno *
	    (unsigned long long)DEV_BSIZE) == -1) {
		puts("bootblk: Could not seek on device\n");
		exit();
	}

	return (prom_read(ih, buf, size, real_blkno, BYTE));
}

int
devclose(void *handle)
{
	return (prom_close((int)handle));
}
