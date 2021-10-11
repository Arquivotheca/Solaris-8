/*
 * Copyright (c) 1992-1994,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_io.c	1.13	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

#ifdef KADB
#undef	_KERNEL
#include <sys/bootsvcs.h>
#endif

#ifdef I386BOOT
#include <sys/bootsvcs.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>

/* For now dev_handle is a device handle, not a file handle */
static int dev_handle = 0;

extern struct	int_pb	ic;
extern int	boot_device;
#endif

#if !defined(KADB) && !defined(I386BOOT)
#include <sys/bootsvcs.h>
#include <sys/bootconf.h>

extern struct bootops *bootops;

int do_prom_open = 1;	/* allow dynamic debugging of prom_open */
#endif

/*
 * prom_open
 *
 * returns 0 on error. Otherwise returns a handle.
 */
/*ARGSUSED*/
int
prom_open(char *path)
{
#ifdef I386BOOT
	return (++dev_handle);
#endif

#ifdef KADB
#ifndef O_RDWR
#define	O_RDWR		2		/* stolen from fcntl.h! */
#endif
	return (open(path, O_RDWR));
#endif

#if !defined(KADB) && !defined(I386BOOT)
	if (do_prom_open) {
		prom_printf("prom_open() - opening -------- '%s'\n", path);
		int20();
	}
	return (BOP_OPEN(bootops, path, 0));
#endif
}


/*
 * prom_seek
 *
 * The Intel version of this prom service only takes two arguments.
 * The third lseek "whence" argument is always assumed to be 0.
 * fake the return code; syscall always returns 0.
 *
 */
/*ARGSUSED*/
int
prom_seek(int fd, unsigned long long offset)
{
#ifdef I386BOOT
	return (0);
#endif

#ifdef KADB
	return (lseek(fd, (off_t)offset, 0));
#endif

#if !defined(KADB) && !defined(I386BOOT)
	return (BOP_SEEK(bootops, fd, (off_t)offset, 0));
#endif
}

/*
 * prom_read
 */

/*ARGSUSED4*/
int
prom_read(int fd, caddr_t buf, uint_t len, uint_t startblk, char devtype)
{
#ifdef I386BOOT
	extern int rdisk();
	int bytes;

#ifdef BOOT_DEBUG
	prom_printf("prom_read: fd %d buf 0x%x len 0x%x startblk 0x%x"
		" type %d\n", fd, buf, len, startblk, devtype);
#endif

	if (boot_device == BOOT_FROM_DISK) {
		if (fd != dev_handle) {
			prom_printf("prom_read: bad device handle\n");
			return (0);
		}

		bytes = rdisk(fd, buf, len, startblk, devtype);
		/* rdisk is in sunos/stand/boot/i86pc/disk.c */
		return (bytes);
	}

	if (boot_device == BOOT_FROM_NET) {
		/*
		 * Using the driver interface, we need:
		 *	DX:DI = receive buffer
		 *	CX = length of buffer
		 *	BX:SI = callback function (not applicable here)
		 */
		ic.intval = 0xfb;
		ic.ax = 0x0400;		/* receive function + IRET return */
		ic.dx = (long)buf/0x10;		/* segment */
		ic.di = (long)buf%0x10;		/* offset */
		ic.cx = len;
		(void) doint();
		return (ic.cx);	/* number of bytes received */
	}

	/* Other boot devices can go here */
#endif

#ifdef KADB
	return (read((int)fd, buf, len));
#endif

#if !defined(KADB) && !defined(I386BOOT)
	if (do_prom_open)
		prom_printf("prom_read(%d) - reading 0x%x\n", fd, startblk);
	return (BOP_READ(bootops, fd, buf, len));
#endif
}

/*
 * prom_write
 */

/*ARGSUSED*/
int
prom_write(int fd, caddr_t buf, uint_t len, uint_t startblk, char devtype)
{
#ifdef I386BOOT
	/*
	 * In the network boot case:
	 * Send out the packet that is pointed to by buf and len bytes long
	 * through the gluecode and the driver.
	 *
	 * The driver interface calls for the registers to be set up as:
	 *	BX:SI = packet pointer
	 *	CX = length of packet
	 */
	if (boot_device == BOOT_FROM_DISK)
		return (-1);	 /* not supported */

	if (boot_device == BOOT_FROM_NET) {
		ic.intval = 0xfb;
		ic.ax = 0x0300;		/* send command, with IRET return */
		ic.bx = (long)buf/0x10;
		ic.si = (long)buf%0x10;
		ic.cx = len;
		(void) doint();
		return (len);
	}

	/* other boot devices can go here */
#else
	return (-1);	 /* who us, write? right! */
#endif
}

/*
 * prom_close
 */

/*ARGSUSED*/
int
prom_close(int fd)
{
#ifdef I386BOOT
	dev_handle--;
	return (0);
#endif

#ifdef KADB
	return (close((int)fd));
#endif

#if !defined(KADB) && !defined(I386BOOT)
	if (do_prom_open)
		prom_printf("prom_close(%d)\n", fd);
	return (BOP_CLOSE(bootops, fd));
#endif
}
