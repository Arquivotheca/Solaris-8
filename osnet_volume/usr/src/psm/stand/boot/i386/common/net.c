/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)net.c	1.17	99/10/07 SMI"

#include <sys/types.h>
#include <sys/machine.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/ihandle.h>
#include <sys/salib.h>

#define	__ctype _ctype	/* Incredably stupid hack used by	*/
#include <ctype.h>	/* ".../stand/lib/i386/subr_i386.c"	*/

extern struct int_pb ic;
extern char *new_root_type;

extern long strtol(char *, char **, int);
extern int Oldstyleboot;
extern int doint(void);
extern void *memcpy(void *s1, void *s2, size_t n);

static char *
net_buffer(struct ihandle *ihp, caddr_t buf, int len)
{
	/*
	 *  Re-size the low-memory data transfer buffer:
	 *
	 *  We allow users to read or write to any memory location, even
	 *  though the realmode code that does the real work can only see
	 *  the first megabyte.  To get around this, we reserve an
	 *  intermediate transfer buffer in low core.  This routine returns
	 *  the address of this buffer if the buffer provided by the user
	 *  cannot be used directly.
	 */
	if ((buf+len) > (caddr_t)USER_START) {
		/*
		 *  User's buf is not in low core so we can't use it
		 *  directly.  Borrow the shared cache memory buffer.
		 *  Discard current cache contents first.
		 */
		if (len <= cache_info.csize) {
			invalidate_cache(NULL);
			buf = cache_info.memp;
		} else
			buf = 0;
	}

	return (buf);
}

int
find_net(int *addrs, int n, char *path)
{
	/*
	 *  Find network unit:
	 *
	 *  Work backwards up the path looking for a "boot-interface" property
	 *  that will give us the unit number.  Returns the unit number if it
	 *  finds it, -1 otherwise.
	 */
	if (!(n % sizeof (int))) {
		/*
		 *  Length of the "boot-interface" property looks reasonable
		 *  so far, but we need to know the instance number before we
		 *  can be sure.
		 */
		char *cp;
		int inst = 0;

		if (((cp = strrchr(path, '/')) != 0) &&
		    ((cp = strrchr(path, ':')) != 0)) {
			/*
			 *  We assume that the first string of digits to the
			 *  right of the colon in the last pathname component
			 *  is the instance number.  If there is no such
			 *  string, we'll assume instance zero.
			 */
			while (*cp && isascii(*cp) && !isdigit(*cp)) cp++;
			inst = (int)strtol(cp, 0, 0);
		}

		if ((n / sizeof (int)) > inst) {
			/*
			 *  If there's a bios index for this instance in the
			 *  "boot-interface" array, return it.
			 */
			return (addrs[inst+1]);
		}
	}

	return (-1);
}

int
open_net(struct ihandle *ihp)
{
	extern int BootDevType;

	/*
	 *  Open a network device:
	 *
	 *  Issue a (disk-like) "int x13" to bind the unit number to the network
	 *  device.
	 */
	if (!new_root_type) {
		/*
		 *  If caller is simply probing for the file system type, we
		 *  know
		 *  that it must be "nfs"!
		 */
		new_root_type = "nfs";

	} else if (*new_root_type && strcmp(new_root_type, "nfs")) {
		/*
		 *  Caller is trying to mount something other than an "nfs" file
		 *  system.  Generate error message and blow up.
		 */
		printf("%s: can't open - doesn't support "
		    "file system type %s.\n", ihp->pathnm, new_root_type);
		return (-1);
	} else {
		/*
		 *  Finally, we need to initialize the network device.
		 *  This is accomplished by issueing the BEF_RESET command
		 *  to the BEF.  Older network BEFs won't understand this
		 *  command and will return failure results.
		 *
		 *  The only time we should be willing to ignore
		 *  the failure is if we were MDB booted from a network
		 *  device.  In that case, the network is already initialized.
		 *  Any other time, the network must be initialized
		 *  and we have to have a newer BEF in order to do so.
		 */
		ic.intval = 0x13; /* Treat the device like disk to init .bef */
		ic.bx = ic.cx = 0;
		ic.dx = ihp->unit;
		ic.ax = 0;

		if ((doint() || (ic.ax & 0xFF00)) &&
		    !(Oldstyleboot && BootDevType == DEVT_NET)) {
			/*
			 *  Can't initialize the .bef, perhaps it's obsolete.
			 */
			printf("can't open %s: initialization failure "
			    "(old .bef?)\n", ihp->pathnm);
			return (-1);
		}
	}

	return (0);
}

/*ARGSUSED*/
void
close_net(struct ihandle *ihp)
{
	/* Nothing to do here */
}

/*ARGSUSED*/
int
read_net(struct ihandle *ihp, caddr_t adr, u_int len, u_int off)
{
	/* BEGIN CSTYLED */
	/*
	 *  Read from a network device:
	 *
	 *  The real work is done in the BIOS (or, more accurately, in
	 *  "dosemul.c").
	 *  Arguments to the "int FB" are:
	 *
	 *		ah     ...  Function code
	 *		al	   ...  Unit number
	 *		dx:di  ...  Pointer to receive buffer
	 *	    cx     ...  Receive buffer length
	 *      bx:si  ...  Callback addr (not used)
	 */
	/* END CSTYLED */
	int rc = -1;
	char *buf;

	if (buf = net_buffer(ihp, adr, len)) {
		/*
		 *  We have a buffer to work with.  Call bios to do the read.
		 */
		ic.dx = segpart((long)buf);
		ic.di = offpart((long)buf);
		ic.intval = DEVT_NET;
		ic.ax = 0x0400;
		ic.cx = len;

		if (((rc = (doint() ? -1 : ic.cx)) > 0) &&
		    (buf != (char *)adr)) {
			/*
			 *  If we were forced to use an intermediate buffer,
			 *  copy the data we just read into the caller's buffer.
			 */
			(void) memcpy(adr, buf, rc);
		}

	}

	if (rc < 0)
		printf("%s: I/O error\n", ihp->pathnm);

	return (rc);
}

/*ARGSUSED*/
int
write_net(struct ihandle *ihp, caddr_t adr, u_int len, u_int off)
{
	/*
	 *  Write to network device:
	 *
	 *  This routine is much like "read_net", except that the data transfer
	 *  goes in the opposite direction!
	 */
	int rc = -1;
	char *buf;

	if (buf = net_buffer(ihp, adr, len)) {
		/*
		 *  We have a data buffer.  Call bios to do the I/O.
		 */
		if (buf != (char *)adr) {
			/*
			 *  We're using the intermediate buffer.  Copy
			 *  caller's data into it before entering bios.
			 */
			(void) memcpy(buf, adr, len);
		}

		ic.bx = segpart((long)buf);
		ic.si = offpart((long)buf);
		ic.intval = DEVT_NET;
		ic.ax = 0x0300;
		ic.cx = len;

		rc = (doint() ? -1 : len);
	}

	if (rc < 0) printf("%s: I/O error\n", ihp->pathnm);
	return (rc);
}

/*ARGSUSED*/
int
macaddr_net(struct ihandle *ihp, unsigned char *eap)
{
	/*
	 *  Use enhanced bios call to obtain ethernet address of the indicated
	 *  network device (we assume it's already been "mounted").  Bios
	 *  returns addr in %bx/%cx/%dx, but little-endianness of i386 requires
	 *  a byteswap of each halfword.
	 */

	ic.intval = DEVT_NET;
	ic.ax = 0x500;
	(void) doint();

	*eap++ = ic.bx >> 8; *eap++ = ic.bx;
	*eap++ = ic.cx >> 8; *eap++ = ic.cx;
	*eap++ = ic.dx >> 8; *eap   = ic.dx;

	return (0);
}
