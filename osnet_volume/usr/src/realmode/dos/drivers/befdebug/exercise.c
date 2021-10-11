/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)exercise.c	1.10	97/10/20 SMI"
 */

/*
 *	BEF testing code for BEFDEBUG.  Calls the BEF for the requested
 *	function.  If the function is "install", also tries to use the
 *	BEF determine its usefulness for booting.
 */

#include "befdebug.h"

Static int bef_exercise(int);
Static int callout(int, char far *, struct bef_interface far *);
Static int mem_adjust(unsigned short, unsigned short);
Static void scsi_exercise(int, struct bdev_info far *, char far *);
Static int scsi_read_test(int, ushort, int, ulong, unchar **);

/*
 * Callbacks are permitted only between the initial driver entry and
 * the corresponding return.  Detect callbacks from service calls by
 * checking the callback_state variable before permitting the call.
 */
int callback_state = CALLBACK_FORBIDDEN;

/*
 * The callback structure is global so that its contents remain
 * in place during any service calls.  If the driver attempts to
 * perform any callbacks from service calls it will still call
 * the correct routine rather than making a wild jump.  As described
 * above, each callback routine warns about illegal calls.
 */
Static struct bef_interface bfi;

/* Buffer is big enough for 2K plus some alignment and guard words */
Static unchar buffer[2100];

void
test_bef(ushort bef_code, ushort psp, ushort mem_size)
{
	ulong far *sign;
	char far *dest;
	int last_dev;
	int dev;
	
	/* Make sure whatever called me really looks like a BEF */
	sign = (ulong far *)((((ulong)bef_code) << 16) + BEF_EXTMAGOFF);
	dest = (char far *)((((ulong)bef_code) << 16) + BEF_EXTENTRY);
	if (*sign != BEF_EXTMAGIC) {
		printf("%s: cannot find driver extended BEF signature.\n",
			prog_name);
		return;
	}
	
	/* Reset the node_op state machine */
	node_reset();

	bfi.version = BEF_IF_VERS_MEM_SIZE;
	bfi.resource = resource;
	bfi.node = node;
	bfi.putc = char_from_driver;
	bfi.mem_adjust = mem_adjust;
	bfi.mem_base = psp;
	bfi.mem_size = mem_size;

	switch (bd.function) {
	case TEST_PROBE:
		bd.table_size = 0;
		UDprintf(("About to call BEF_LEGACYPROBE.\n"));
		callout(BEF_LEGACYPROBE, dest, &bfi);
		UDprintf(("Finished BEF_LEGACYPROBE.\n"));
		printf("Probe completed. %d node%s created.\n",
			node_count(), node_count() == 1 ? "" : "s");
		break;
	case TEST_INSTALL:
		UDprintf(("About to call BEF_INSTALLONLY.\n"));
		last_dev = callout(BEF_INSTALLONLY, dest, &bfi);
		if (last_dev == BEF_FAIL) {
			printf("Driver did not install devices.\n");
			return;
		}
		UDprintf(("BEF_INSTALLONLY succeeded, last device is %x.\n",
			last_dev));
		if (last_dev == 0x10)
			printf("Driver installed 1 device (code 10).\n");
		else
			printf("Driver installed %d devices (codes %x - %x).\n",
				last_dev - 0xF, 0x10, last_dev);
		for (dev = 0x10; dev <= last_dev; dev++)
			if (bef_exercise(dev))
				break;
		if (dev > last_dev)
			break;
		printf("\nNetwork drivers cannot support multiple active ");
		printf("devices, so only device\n10 was exercised.  To ");
		printf("exercise a different device, run %s with a\n",
			prog_name);
		printf("different install data file where the node for the ");
		printf("device to be exercised\nappears first.\n");
		break;
	}
}

Static int
bef_exercise(int dev)
{
	struct bdev_info far *info;
	char far *driver_name;
	ushort magic;
	ulong *p;
	ulong v;
	
	UDprintf(("Calling INT 13 BEF_IDENT command for device %x.\n", dev));
	p = (ulong *)(0x13 * 4);
	v = *p;
	_asm {
		mov	dl, byte ptr dev
		mov	ah, BEF_IDENT

;		The next 2 lines mimic an INT 13h.  Some debuggers seem to
;		prevent certain kinds of INT 13h access.
		pushf
		call	dword ptr [v]

		mov	magic, dx
		mov	word ptr info, bx
		mov	word ptr info+2, es
		mov	word ptr driver_name, cx
		mov	word ptr driver_name+2, es
	}
	if (magic != BEF_MAGIC) {
		printf("BEF_IDENT failed for device %x.\n", dev);
		return (0);
	}

	/*
	 * Only one device can be exercised in a network driver because
	 * by convention we select the boot device according to the
	 * first INT 13h RESET.  So we do not do anything with net
	 * devices other than 0x10.  To test a different device it
	 * is necessary to edit the install data file so that the
	 * node for the device to be tested appears first.
	 */
	if (info->dev_type == MDB_NET_CARD && dev != 0x10) {
		return (1);
	}

	UDprintf(("Calling INT 13 RESET command for device %x.\n", dev));
	p = (ulong *)(0x13 * 4);
	v = *p;
	_asm {
		mov	dl, byte ptr dev
		xor	ax, ax

;		The next 2 lines mimic an INT 13h.  Some debuggers seem to
;		prevent certain kinds of INT 13h access.
		pushf
		call	dword ptr [v]

	}
	UDprintf(("Returned from INT 13 RESET command.\n"));

	switch (info->dev_type) {
	case MDB_SCSI_HBA:
		scsi_exercise(dev, info, driver_name);
		break;
	case MDB_NET_CARD:
		net_exercise(dev, info, driver_name);
		break;
	default:
		printf("Unrecognized device type (info->dev_type = %x).\n", 
			info->dev_type);
		break;
	}
	return (0);
}

Static void
scsi_exercise(int dev, struct bdev_info far *info, char far *driver_name)
{
	int errors = 0;
	unchar *buf;
	ulong sector = 0;
	int i;

	switch (info->MDBdev.scsi.pdt & INQD_PDT_DMASK) {
	case INQD_PDT_SEQ:
		printf("Device %x is a tape drive (ignored).\n", dev);
		return;
	case INQD_PDT_PRINT:
		printf("Device %x is a printer (ignored).\n", dev);
		return;
	case INQD_PDT_SCAN:
		printf("Device %x is a scanner (ignored).\n", dev);
		return;
	case INQD_PDT_COMM:
		printf("Device %x is a communication device (ignored).\n",
			dev);
		return;
	}

	/* These read tests check reading 2k blocks and alignment */
	errors += scsi_read_test(dev, 2048, 0, sector, &buf);
	errors += scsi_read_test(dev, 2048, 1, sector, &buf);
	errors += scsi_read_test(dev, 2048, 2, sector, &buf);
	errors += scsi_read_test(dev, 2048, 3, sector, &buf);

	/*
	 * This read test checks reading 512-byte sectors and supplies the
	 * data for the boot record test.
	 */
	errors += scsi_read_test(dev, 512, 0, sector, &buf);

	if (errors)
		return;

	if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xAA) {
		printf("No master boot record on device %x.\n",
			dev);
		return;
	}

	/* Look for the first (should be only) bootable partition */
	for (i = 0; i < 4; i++) {
		if (buf[0x1be + i * 0x10] == 0x80)
			break;
	}

	if (i == 4) {
		printf("Master boot record on device %x ", dev);
		printf("has no active partition.\n");
		return;
	}
	printf("Master boot record on device %x shows partition %d active.\n",
		dev, i + 1);
	sector = *(ulong *)(buf + 0x1c6 + i * 0x10);
	if (scsi_read_test(dev, 512, 0, sector, &buf) != 0) {
		printf("Read failed while attempting to read partition %d\n",
			i + 1);
		printf("boot record from sector %ld of device %x.\n",
			sector, dev);
		return;
	}

	if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xAA) {
		printf("No partition boot record found on partition %d.\n",
			i + 1);
		return;
	}
	printf("Partition boot record found on partition ");
	printf("%d of device %x at sector %ld.\n", i + 1, dev, sector);
}

Static int
scsi_read_test(int dev, ushort read_size, int alignment, ulong start_sector,
	unchar **bufp)
{
	int i;
	unchar read_error;
	unchar *dbuf;
	ulong *guard_word1;
	ulong *guard_word2;
	ulong ul;
	unchar sectors = read_size / 512;
	ushort creg;
	ushort dreg;
	ushort sec_per_trk;
	ushort trk_per_cyl;
	ushort cyls;
	ulong *p;
	ulong v;
#define	GUARD 0xbefdbbefL

	if (sectors * 512 != read_size) {
		printf("%s internal error: scsi_read_test called ", prog_name);
		printf("for %d bytes.\n", read_size);
		return (1);
	}

	p = (ulong *)(0x13 * 4);
	v = *p;
	_asm {
		mov	ah, BEF_GETPARMS
		mov	dl, byte ptr dev
		xor	dh, dh		/* head 0 */

;		The next 2 lines mimic an INT 13h.  Some debuggers seem to
;		prevent certain kinds of INT 13h access.
		pushf
		call	dword ptr [v]

		mov	read_error, ah
		mov	creg, cx
		mov	dreg, dx
	}
	if (read_error != 0) {
		printf("BEF_GETPARMS failed for device %x.\n", dev);
		return (1);
	}

	/* Calculate the disk parameters */
	sec_per_trk = (creg & 0x3F);
	trk_per_cyl = (dreg >> 8);
	cyls = ((creg >> 8) | ((creg >> 6) & 3));

	/* Calculate the CX and DX registers for the read */
	creg = (start_sector % sec_per_trk) + 1;
	ul = start_sector / sec_per_trk;
	dreg = (((ul % trk_per_cyl) << 8) | (dev & 0xFF));
	ul = ul / trk_per_cyl;
	creg |= ((ul << 8) | ((ul >> 8) << 6));

	/*
	 * Lay out the guard bytes and data buffer within the overall
	 * buffer.  The data buffer is first placed at a 4-byte boundary
	 * that is guaranteed to be at least one long from the start of
	 * the overall buffer.  Then it is moved according to the
	 * alignment algorithm.  Finally the guard byte locations are
	 * calculated.
	 */
	dbuf = (unchar *)(((ulong)(buffer + 8) & ~3) + (alignment & 3));
	*bufp = dbuf;
	guard_word1 = ((ulong *)(dbuf)) - 1;
	guard_word2 = (ulong *)(dbuf + read_size);
	
	for (i = 0; i < read_size; i++)
		dbuf[i] = 0;
	*guard_word1 = GUARD;
	*guard_word2 = GUARD;
	if (sectors == 1)
		UDprintf(("Reading sector %ld of device %x.\n",
			start_sector, dev));
	else
		UDprintf(("Reading sectors %ld - %ld of device %x.\n",
			start_sector, start_sector + sectors - 1, dev));

	p = (ulong *)(0x13 * 4);
	v = *p;
	_asm {
		mov	ah, 2		/* READ command */
		mov	al, sectors	/* number of sectors to read */
		mov	dx, dreg
		mov	cx, creg
		les	bx, dbuf

;		The next 2 lines mimic an INT 13h.  Some debuggers seem to
;		prevent certain kinds of INT 13h access.
		pushf
		call	dword ptr [v]

		mov	read_error, ah
	}
	if (*guard_word1 != GUARD || *guard_word2 != GUARD) {
		if (*guard_word1 != GUARD) {
			printf("Data read from device %x ", dev);
			printf("overwrote guard word preceding buffer.\n");
			printf("Contents before read: %lx, ", GUARD);
			printf("contents after read: %lx.\n", 
				*guard_word1);
		}
		if (*guard_word2 != GUARD) {
			printf("Data read from device %x ", dev);
			printf("overwrote guard word following buffer.\n");
			printf("Contents before read: %lx, ", GUARD);
			printf("contents after read: %lx.\n", 
				*guard_word2);
		}
		return (1);
	}	
	if (read_error == 0) {
		UDprintf(("Read succeeded: %0.2x %0.2x %0.2x %0.2x ",
			dbuf[0], dbuf[1], dbuf[2], dbuf[3]));
		UDprintf(("%0.2x %0.2x %0.2x %0.2x ... ",
			dbuf[4], dbuf[5], dbuf[6], dbuf[7]));
		UDprintf(("%0.2x %0.2x %0.2x %0.2x ",
			dbuf[read_size - 8], dbuf[read_size - 7],
			dbuf[read_size - 6], dbuf[read_size - 5]));
		UDprintf(("%0.2x %0.2x %0.2x %0.2x.\n",
			dbuf[read_size - 4], dbuf[read_size - 3],
			dbuf[read_size - 2], dbuf[read_size - 1]));
	} else {
		printf("Read from device %x with buffer alignment %d failed, ",
			dev, alignment & 3);
		printf("error code %x.\n", read_error);
		return (1);
	}
	return (0);
}

void
net_addr(unchar *ether)
{
	UDprintf(("Requesting network address for installed device.\n"));
	_asm {
		push	bx
		mov	ah, GET_ADDR_CALL
		int	0fbh
		mov	ax, bx
		mov	bx, word ptr ether
		mov	[bx], ah
		mov	[bx+1], al
		mov	[bx+2], ch
		mov	[bx+3], cl
		mov	[bx+4], dh
		mov	[bx+5], dl
		pop	bx
	}
	UDprintf(("Returned from network address request call.\n"));
}

void
net_open(void)
{
	UDprintf(("Sending open request to network device.\n"));
	_asm {
		mov	ah, OPEN_CALL
		int	0fbh
	}
	UDprintf(("Returned from network device open request.\n"));
}

void
net_close(void)
{
	UDprintf(("Sending close request to network device.\n"));
	_asm {
		mov	ah, CLOSE_CALL
		int	0fbh
	}
	UDprintf(("Returned from network device close request.\n"));
}

void
net_send_packet(unchar *packet, ushort psize)
{
	UDprintf(("Requesting to send %d-byte packet.\n", psize));
	_asm {
		push	bx
		mov	ah, SEND_CALL
		mov	si, word ptr packet
		mov	bx, word ptr packet+2
		mov	cx, psize
		int	0fbh
		pop	bx
	}
	UDprintf(("Returned from packet send request.\n"));
}

int
net_receive_packet(unchar *packet, ushort psize)
{
	int answer;
	static int msg_limit = 5;

	if (msg_limit > 0)
		UDprintf(("Requesting to receive up to %d-byte packet.\n",
			psize));
	_asm {
		/*
		 * Register Setup:
		 * DX:DI = callers receive buffer
		 * CX    = size of receive buffer
		 * Return Setup:
		 * CX    = number of bytes received
		 */
		push	di
		mov	ah, RECEIVE_CALL
		mov	di, word ptr packet
		mov	dx, word ptr packet+2
		mov	cx, psize
		int	0fbh
		mov	answer, cx
		pop	di
	}
	if (msg_limit > 0) {
		msg_limit--;
		if (answer == 0)
			UDprintf(("No received packet available.\n", answer));
		else 
			UDprintf(("Received %d-byte packet.\n", answer));
		if (msg_limit == 0)
			UDprintf(("No more receive poll messages will appear "
				"so that output is not excessive.\n"));
	}
	return (answer);
}

/* This code was lifted from befext.c in bootconf.  Might need updating
 * from time to time.  Would be better to include it somehow.
 *
 * A few changes made.  Be careful if updating!
 */
Static int
/*ARGSUSED0*/
callout(int op, char *ep, struct bef_interface *ap)
{
	/*
	 * Call into the driver:
	 * Pass the address of the bef interface struct
	 * at "ap" to the driver via "ES:DI"
	 */

	int x = 0;

	callback_state = CALLBACK_ALLOWED;

#ifndef __lint
	_asm {
		les	di, ap;			ES:DI will contain ap
		mov	ax, op;			Option to %ax
		call	ep;			call out
		mov	x, ax;			return result
	}
#endif
 
	callback_state = CALLBACK_FORBIDDEN;
	return (x);
}

Static int 
mem_adjust(unsigned short block, unsigned short size)
{
	unsigned short max = 0;

	if (callback_state != CALLBACK_ALLOWED) {
		printf("%s: driver made a %s callback after initial return\n",
			prog_name, "mem_adjust");
	}

	return (dos_mem_adjust(block, size, &max) == 0 ? BEF_OK : BEF_FAIL);
}
