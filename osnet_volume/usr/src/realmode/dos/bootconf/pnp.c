/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  pnp.c -- Plug-n-Play ISA device enumerator
 */

#ident "@(#)pnp.c   1.47   99/05/28 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <names.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <conio.h>
#include "types.h"

#include "boards.h"
#include "boot.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "escd.h"
#include "gettext.h"
#include "menu.h"
#include "pnp.h"
#include "pnp1275.h"
#include "pnpbios.h"
#include "probe.h"
#include "resmgmt.h"
#include "tree.h"
#include "tty.h"
#include "acpi_rm.h"

void SetReadPort();
void ReadResource(char *buf, int n);
Resource *AnyUnitResource(Board **bpp, int flags);
Resource *AnyRangeResource(Board **bpp, int flags);
Board *GetSmallResourcePnp(Board *bp, char tag);
Board *GetLargeResourcePnp(Board *bp, char tag);
void ExtractResources();
int copyout(int port, char *buf, int n);
void copyin(int port, char *buf, int n);
int program_res(Resource *rp, int t, int n);
void get_programmed_pnp(Board *bp);
unsigned long ReadBits(unsigned port, int size);
unsigned CheckSum(unsigned long vid, unsigned long sid);
int IsolateCard(unsigned port);
int FindNextCard();
void clear_config_pnp();
void ResetCards(int flag);
void init_pnp();

/*  Standard Port addresses for ISA+ cards:				    */

#define	ADDRESS_CMD_PORT	0x279	/* Printer (PNP card) status port   */
#define	WRITE_DATA_PORT		0xA79	/* Write data port		    */

#define	PORTRANGE_MIN	0x203	/* Spec defined minimum */
#define	PORTRANGE_START	0x20F	/* Our start of search for port map range   */
#define	PORTRANGE_END	0x3FF	/* End of I/O port map range		    */

#define	PnP_PORTS_BOARD_SZ 256

/*  ISA+ Device Control registers:					    */

#define	SET_READ_DATA		0x00
#define	SERIAL_ISOLATION	0x01
#define	CONFIG_CONTROL		0x02
#define	WAKE_CMD		0x03
#define	RESOURCE_DATA		0x04
#define	STATUS			0x05
#define	SET_CARD_SELECT_NO	0x06
#define	LOGICAL_DEVICE		0x07
#define	RESERVED_START		0x08
#define	RESERVED_END		0x1f
#define	VENDOR_DEFINED_START	0x20
#define	VENDOR_DEFINED_END	0x2f
#define	ACTIVATE		0x30
#define	IO_RANGE_CHECK		0x31
#define	LOGICALDEV_VENDOR_START	0x38
#define	LOGICALDEV_VENDOR_END	0x3f

u_char PnpCardCnt = 0;		/* Number of PnP cards detected.	    */
static int ResFlags = 0;	/* Current resource flags		    */
static int DepFlags = 0;	/* Dependent function flag		    */
static u_int ReadPort = 0;	/* Global read port for PnP configuration   */
static u_char LogicalDevice = 0; /* Logical device number		    */
static u_long serial_no;
static u_long board_id;
u_char csn;

/*
 *	Configuration space offsets
 *
 *				Port  IRQ   DMA   Mem
 *				----  ---   ---   ---
 */
static int port_base[]	= { 0,	0x60, 0x70, 0x74, 0x40 };
static int port_size[]	= { 0,	   2,    2,    1,    8 };
static int port_count[]	= { 0,	   8,    2,    2,    4 };

void
/*ARGSUSED0*/
delay(unsigned long n)
{
#ifndef __lint
	/*
	 *  Wait "n" microseconds!
	 */
	_asm {
		mov  ax, 8600h
		mov  dx, word ptr [n]
		mov  cx, word ptr [n+2]
		int  15h
	}
#endif
}

void
ResetCards(int flag)
{
	/*
	 *  Reset ISA+ Cards:
	 *
	 *  This routine is used to enable/disable device configuration of
	 *  any and all PnP ISA cards that may be installed.  If the "flag"
	 *  word is non-zero we enable PnP configuration, disable it other-
	 *  wise.
	 */

	if (flag == 0) {
		/*
		 *  Put all PnP cards back into the "wait-for-key" state.
		 */

		_outp(ADDRESS_CMD_PORT, CONFIG_CONTROL);
		_outp(WRITE_DATA_PORT, 2);

	} else {
		/*
		 *  Enable PnP device configuration.  We assume that devices
		 *  are in "wait-for-key" state when we're called, and we
		 *  write the magic activation key to the ADDRESS port.
		 */

		int j;
		unsigned char y, x = 0x6A;

		_outp(ADDRESS_CMD_PORT, 0);
		_outp(ADDRESS_CMD_PORT, 0);

		for (j = 32; j--; x = (y | ((x ^ y) << 7))) {
			/*
			 *  Write the 32-byte PnP activation key, one byte
			 *  at a time!
			 */

			_outp(ADDRESS_CMD_PORT, x);
			y = (x >> 1);
		}
	}
}

void
SetReadPort()
{
	/*
	 *  Set PNP Configuration Read Port:
	 *
	 *  ISA+ cards are configured thru a pair of 8-bit ports.  The output
	 *  port is pre-defined, but the input port may lie anywhere in the
	 *  specified input PORTRANGE.  Once the input port address has been
	 *  determined, this routine is used to reserve that port to prevent
	 *  other devices from trying to use it.  This is done by adding an
	 *  appropriate RESF_Port entry to the ESCD motherboard.
	 */

	Board *bp;

	/*
	 * Fill in the Board fields
	 */
	bp = new_board();
	bp->bustype = RES_BUS_ISA;
	bp->category = DCAT_OTH;
	bp->devid = CompressName("SUN0001");

	/*
	 * Fill in resource fields
	 */
	bp = AddResource_devdb(bp, RESF_Port, ReadPort, 1);

	bp = AddResource_devdb(bp, RESF_Port, ADDRESS_CMD_PORT, 1);

	bp = AddResource_devdb(bp, RESF_Port, WRITE_DATA_PORT, 1);

	add_board(bp); /* Tell rest of system about device */
}

void
ReadResource(char *buf, int n)
{
	/*
	 *  Read PnP Resource Info:
	 *
	 *  Reads the next "n" bytes of PnP resource information into the
	 *  indicated "buf"fer.  Assumes that the current device has already
	 *  been isolated.
	 */

	while (n-- > 0) {
		/*
		 *  Loop until "n" bytes have been read.  We have to wait
		 *  for the low order status bit to settle prior to reading
		 *  each byte.
		 */

		_outp(ADDRESS_CMD_PORT, STATUS);
		while (!(_inp(ReadPort) & 1)) delay(250);

		_outp(ADDRESS_CMD_PORT, RESOURCE_DATA);
		*buf++ = _inp(ReadPort);
	}
}

/*
 * Allocate unit resource records:
 *
 * This routine allocates a RESF_ANY resource record with the given
 * "flags". RESF_ANY records for unit resources (DMA and IRQ) are
 * actually a pair of resource records: The first gives the valid
 * resource alternates (as a bit mask in the "base" field), while
 * the second serves as a place holder for the actual resource
 * assignment.
 */
Resource *
AnyUnitResource(Board **bpp, int flags)
{
	Board *bp;
	Resource *rp;

	bp = ResizeBoard_devdb(*bpp, 2 * sizeof (Resource));

	rp = (Resource *)((char *)bp + bp->reclen);
	if (!bp->resoff) {
		bp->resoff = bp->reclen;
	}
	bp->reclen += (2 * sizeof (Resource));
	memset(rp, 0, 2 * sizeof (Resource));
	bp->rescnt[flags-1] += 2;
	flags |= ResFlags;
	ResFlags = 0;

	if (!(flags & RESF_FIRST)) {
		rp[-1].flags |= RESF_MULTI;
	}
	rp[0].flags = flags | (RESF_ALT+RESF_ANY+RESF_MULTI);
	rp[1].flags = rp[0].flags & ~(RESF_FIRST+RESF_SUBFN+RESF_MULTI);
	*bpp = bp;
	return (rp);
}

Resource *
AnyRangeResource(Board **bpp, int flags)
{
	/*
	 *  Allocate range resource records:
	 *
	 *  This routine allocates a RESF_ANY resource record with the given
	 *  "flags".  RESF_ANY records for range resources (Ports and memory)
	 *  consists of three records:  The first two give the minimum/maximum
	 *  base address and step/range lengths in their respective "base" and
	 *  "length" fields.  The third record is the place holder for actual
	 *  resource assignments.
	 */

	Board *bp;
	Resource *rp;

	bp = ResizeBoard_devdb(*bpp, 3 * sizeof (Resource));

	rp = (Resource *)((char *)bp + bp->reclen);
	if (!bp->resoff) {
		bp->resoff = bp->reclen;
	}
	bp->reclen += (3 * sizeof (Resource));
	memset(rp, 0, 3 * sizeof (Resource));
	bp->rescnt[flags-1] += 3;
	flags |= ResFlags;
	ResFlags = 0;

	if (!(flags & RESF_FIRST)) {
		rp[-1].flags |= RESF_MULTI;
	}
	rp[0].flags = flags | (RESF_ALT+RESF_ANY+RESF_MULTI);
	rp[1].flags = rp[0].flags & ~(RESF_FIRST+RESF_SUBFN);
	rp[2].flags = rp[1].flags & ~RESF_MULTI;
	*bpp = bp;
	return (rp);
}

Board *
GetSmallResourcePnp(Board *bp, char tag)
{
	/*
	 *  Convert "Small" Resource to Canonical form:
	 *
	 *  This routine reads the "small" resource record specified by the
	 *  "tag" argument from the current ISA+ device and copies the data
	 *  contained therein into the board record at "bp".   Returns a
	 *  pointer to the updated board record (which may move due to
	 *  resizing).
	 */

	Resource *rp;
	unsigned char buf[8];
	char *name;
	u_long bitmap, start, end, align, length;

	/*  Read the resource info and process according to type ...	    */
	ReadResource((char *)buf, tag & 7);
	switch (tag & 0x78) {

	case COMPATIBLE_DEVICE_ID:
		name = id_to_str_pnp1275(* (u_long *) buf);
		if (bp->pnp_compat_ids == NULL) {
			/*
			 * 1st one
			 */
			bp->pnp_compat_ids = (char *)malloc(strlen(name) + 2);
			*bp->pnp_compat_ids = ' '; /* space is 1st char */
			strcpy(bp->pnp_compat_ids + 1, name);
		} else {
			bp->pnp_compat_ids =
			    (char *)realloc(bp->pnp_compat_ids,
			    strlen(bp->pnp_compat_ids) + strlen(name) + 2);
			strcat(bp->pnp_compat_ids, " ");
			strcat(bp->pnp_compat_ids, name);
		}

		/*
		 * Check if there is a mapping in the master file
		 * file for the current bp->devid. If not check if there
		 * is one for this compat id. If so replace the devid
		 * with the compat id.
		 */
		if ((bp->dbentryp == 0) && (bp->dbentryp =
		    TranslateDevice_devdb(* (u_long *) buf, RES_BUS_PNPISA))) {
			bp->devid = * (u_long *) buf;
		}
		break;

	case LOGICAL_DEVICE_ID:
		/*
		 * allocate a new board for each logical device
		 */
		if (bp && (resource_count(bp) == 0)) {
			/*
			 * Reuse same Board when no resources
			 */
			goto reuse;
		}
		if (bp) {
			if (!(bp = realloc(bp, bp->buflen = bp->reclen))) {
				MemFailure();
			}

			bp->pnp_multi_func = 1;
			bp->link = Head_prog;
			Head_prog = bp;
		}

		/*
		 * Allocate a new Board for each Logical device
		 * (with resources).
		 */
		bp = new_board();
reuse:
		if (bp->bus_u.pnp == NULL) {
			if (!(bp->bus_u.pnp = (struct pnp_s *)
			    calloc(1, sizeof (struct pnp_s)))) {
				MemFailure();
			}
		}
		bp->bustype = RES_BUS_PNPISA;
		bp->pnp_serial = serial_no;
		bp->pnp_csn = csn;
		bp->pnp_board_id = board_id;
		ResFlags = (RESF_FIRST+RESF_SUBFN);
		DepFlags = RESF_SUBFN;
		if (LogicalDevice != 0) {
			bp->pnp_multi_func = 1;
		}
		bp->pnp_ldn = LogicalDevice++;
		bp->flags = BRDF_PGM;
		bp->pnp_func_id = * (u_long *) buf;
		bp->devid = bp->pnp_func_id;
		bp->dbentryp = TranslateDevice_devdb(bp->devid, RES_BUS_PNPISA);

		if (buf[4] & 1) {
			/*
			 * Device can participate in boot sequence
			 * Mark as possibly fixed configured, and continue.
			 */
			bp->flags |= BRDF_PNPBOO;
		}

		break;

	case START_DEPENDENT_TAG:
		ResFlags = (RESF_FIRST | DepFlags);
		DepFlags = 0;
		break;

	case END_DEPENDENT_TAG:
		ResFlags = (RESF_FIRST+RESF_SUBFN);
		DepFlags = RESF_SUBFN;
		break;

	case IRQ_FORMAT:
		/*
		 *  IRQ information:  The resource "base" is actually a bit
		 *  map that specifies which IRQ lines are useable by this
		 *  function.
		 */
		bitmap = (u_long)(*(unsigned *)buf);
		if (bitmap) {
			rp = AnyUnitResource(&bp, RESF_Irq);
			rp->EISAflags =
			    ((buf[2] & 0x0C) ? RES_IRQ_TRIGGER : 0);
			rp->base = bitmap;
		}
		break;

	case DMA_FORMAT:
		/*
		 *  DMA information:  The resource "base" is actually a bit
		 *  map that specifies which DMA channels are useable by this
		 *  function.
		 */
		bitmap = (u_long) buf[0];
		if (bitmap) {
			rp = AnyUnitResource(&bp, RESF_Dma);
			rp->EISAflags = ((int)(buf[1] & 0x03) << 8)
			    + ((int)(buf[1] & 0x60) << 5);
			rp->base = bitmap;
		}
		break;

	case IO_PORT_DESCRIPTOR:
		/*
		 *  Port info:  Resource base and length are each made up of
		 *  two 16-bit fields:  Min/Max address and alignment/length,
		 *  respectively.
		 */
		start = (u_long) (*(unsigned *)&buf[1]);
		end = (u_long) (*(unsigned *)&buf[3]);
		align = (u_long) buf[5];
		length = (u_long) buf[6];
		if (end && length) {
			rp = AnyRangeResource(&bp, RESF_Port);
			rp[0].length = align;
			rp[1].length = length;
			rp[0].base = start;
			rp[1].base = end;

			ASSERT(start <= end);
		}
		break;

	case FIXED_LOCATION_IO_DESCRIPTOR:
		/*
		 *  More Port Info:  Although the IO_PORT_DESCRIPTOR record,
		 *  above, can easily encode fixed location ports this would
		 *  have made things much too simple for Intel!
		 */
		start = end = (u_long) (*(unsigned *)buf);
		align = length = (u_long) buf[2];
		if (end && length) {
			rp = AnyRangeResource(&bp, RESF_Port);
			rp[0].base = start;
			rp[1].base = end;
			rp[0].length = align;
			rp[1].length = length;
		}
		break;
	}

	return (bp);
}

Board *
GetLargeResourcePnp(Board *bp, char tag)
{
	/*
	 *  Convert "Large" Resource to Canonical Form:
	 *
	 *  This routine reads the next large-format resource (identified by
	 *  the "tag" argument) from the current ISA+ card and copies the
	 *  information contained therein into the board record at "bp".  It
	 *  returns a pointer to the updated board record.
	 *
	 *  The primary "large" resources are memory buffers, but the device
	 *  name is also considered to be a resource.
	 */

	char *buf;
	unsigned len;
	Resource *rp;
	u_long start, end, align, length;

	ReadResource((char *)&len, 2); /* Read resource length */
	if (!(buf = malloc(len + 1))) {
		MemFailure();
	}
	ReadResource(buf, len);

	switch (tag & 0xFF) {

	case CARD_IDENTIFIER_ANSI:
		buf[len] = 0; /* ensure string is terminated */
		ASSERT(bp);
		bp->pnp_desc = buf;
		break;

	case MEMORY_RANGE_DESCRIPTOR:
		/*
		 *  Well, if we can have two record types to encode I/O ports,
		 *  why not three record types for memory resources!  This is
		 *  the 24-bit address form.
		 */

		start = (u_long) (*(unsigned short *)&buf[1]) << 8;
		end = (u_long) (*(unsigned short *)&buf[3]) << 8;
		align = (u_long) *(unsigned short *)&buf[5];
		length = (u_long) (*(unsigned short *)&buf[7]) << 8;
		if (!end || !length) {
			break;
		}

		rp = AnyRangeResource(&bp, RESF_Mem);

		rp[0].base = start;
		rp[1].base = end;
		rp[0].length = align;
		if (rp[0].length == 0) {
			rp[0].length = 0x10000;
		}
		rp[1].length = length;
		goto mfl;

	case MEMORY32_RANGE_DESCRIPTOR:
		/*
		 *  .. Here's the same record with 32-bit addresses, which
		 *  makes it a mere 6 bytes larger than the 24-bit form above.
		 */

		start = *(unsigned long *)&buf[1];
		end = *(unsigned long *)&buf[5];
		align = *(unsigned long *)&buf[9];
		length = *(unsigned long *)&buf[13];
		if (!end || !length) {
			break;
		}
		rp = AnyRangeResource(&bp, RESF_Mem);

		rp[0].base = start;
		rp[1].base = end;
		rp[0].length = align;
		rp[1].length = length;
		goto mfl;

	case MEMORY32_FIXED_DESCRIPTOR:
		/*
		 *  .. And finally, the fixed buffer specification for 32-bit
		 *  addresses (6 bytes shorter than the range descriptor which
		 *  can easily encode the same info).  This is a perfect ex-
		 *  ample of how Intel designs stuff:  Adding triple redundancy
		 *  (and software complexity) to save about 6 bytes of NVRAM
		 *  per card (because most cards can't use more than one
		 *  memory buffer).
		 */

		start = end = *(unsigned long *)&buf[1];
		align = length = *(unsigned long *)&buf[5];
		if (!end || !length) {
			break;
		}
		rp = AnyRangeResource(&bp, RESF_Mem);
		rp[0].base = start;
		rp[1].base = end;
		rp[0].length = align;
		rp[1].length = length;

	mfl:	rp->EISAflags = (buf[0] & 0x03)
				    + RES_MEM_TYPE_OTH
				    + ((buf[0] & 0x18) << 3)
				    + ((buf[0] & 0x04)
						    ? RES_MEM_CODE_4GB
						    : RES_MEM_CODE_16MB);
		ASSERT(rp[0].base <= rp[1].base);
		free(buf);
		break;
	}

	return (bp);
}

void
ExtractResources()
{
	/*
	 *  Extract board resource requirements:
	 *
	 *  This routine reads resource requirements from the current ISA+
	 *  card (as sequenced by "csn") and builds a board record describing
	 *  the possible resource assignments for this card.  It returns a
	 *  pointer to this dynamically constructed DDN.
	 *
	 *  NOTE: DCDs use the RESF_ANY form for their resource records.
	 */

	char tag;
	Board *bp;

	struct {
		/* PnP card ID information:				    */
		unsigned long vid;	/* .. Vendor ID	(EISA format)	    */
		unsigned long sno;	/* .. Serial number		    */
		unsigned char sum;	/* .. Checksum byte		    */
	} cid;

	/*
	 * Note can't use sizeof (cid) as this yields 10 and
	 * all hell breaks loose
	 */
	ReadResource((char *)&cid, 9);
	serial_no = cid.sno;
	board_id = cid.vid;
	LogicalDevice = 0;
	bp = new_board();

	for (ReadResource(&tag, 1); tag != END_TAG; ReadResource(&tag, 1)) {
		/*
		 *  Read thru the resource info, and extract resource
		 *  requirements according to type.
		 */
		if (tag & 0x80) {
			bp = GetLargeResourcePnp(bp, tag);
		} else {
			bp = GetSmallResourcePnp(bp, tag);
		}
	}

	if (bp) {
		if (!(bp = realloc(bp, bp->buflen = bp->reclen))) {
			MemFailure();
		}

		bp->link = Head_prog;
		Head_prog = bp;
	}

	ResFlags = 0;
}

/*
 *  Plug-n-Play ISA device enumerator:
 *
 *  This routine reads configuration information from each of the
 *  PNP ISA cards identified by init_pnp (see below) and converts
 *  this information into "Board" records.
 */

void
enumerator_pnp()
{
	Board *bp, *prev, *next;
	u_char active;

	if (PnpCardCnt == 0) {
		return;
	}

	ResetCards(1);	/* Re-enable PnP card */
	SetReadPort();	/* Record read port usage */

	for (csn = 1; csn <= PnpCardCnt; csn++) {
		/*
		 *  We've already counted the PNP ISA boards (total number is
		 *  in "PnpCardCnt"), so now all we have to do laboriously read
		 *  each card's configuration information thru the 8-bit
		 *  configuration port!
		 */
		_outp(ADDRESS_CMD_PORT, WAKE_CMD);
		_outp(WRITE_DATA_PORT, csn);
		_outp(ADDRESS_CMD_PORT, SET_CARD_SELECT_NO);
		if (_inp(ReadPort) != csn) {
			return;
		}

		ExtractResources();
	}

	/*
	 * For each logical device found a Board will have been created.
	 * We now have to check for logical devices that are activated
	 * and can participate in the boot sequence, and then read their
	 * current (fixed) configuration. Deactivate all others.
	 */

	for (prev = NULL, bp = Head_prog; bp; bp = next) {
		next = bp->link;
		_outp(ADDRESS_CMD_PORT, WAKE_CMD);
		_outp(WRITE_DATA_PORT, bp->pnp_csn);
		_outp(ADDRESS_CMD_PORT, LOGICAL_DEVICE);
		_outp(WRITE_DATA_PORT, bp->pnp_ldn);
		_outp(ADDRESS_CMD_PORT, ACTIVATE);
		active = _inp(ReadPort) & 1;

		if (Debug & D_DISP_DEV) {
			Resource *rp = resource_list(bp);
			int rc;
			iprintf_tty("board %s\n", GetDeviceId_devdb(bp, 0));
			for (rc = 0; rc < resource_count(bp); rc++, rp++) {
				iprintf_tty("%d: base %lx len %lx flags %x\n",
				    rc, rp->base, rp->length, rp->flags);
			}
		}

		if (active && (bp->flags & BRDF_PNPBOO) &&
		    (!bp->dbentryp || (bp->dbentryp->category != DCAT_NET))) {
			if (prev) {
				prev->link = bp->link;
			} else {
				Head_prog = bp->link;
			}
			get_programmed_pnp(bp);
			/* check with ACPI first */
			if ((bp = acpi_check(bp)) != NULL) {
				/* Tell rest of system about device */
				add_board(bp);
			}
		} else {
			_outp(WRITE_DATA_PORT, 0); /* de-activate */
			clear_config_pnp();
			prev = bp;
		}
	}
	ResetCards(0);
}

int
copyout(int port, char *buf, int n)
{
	/*
	 *  Copy multi-byte sequence to PNP configuration registers.
	 *
	 *  This routine copies "n" bytes from the specified "buf"fer to the
	 *  idicated PNP configuration "port".  It returns the number of bytes
	 *  written.
	 *
	 *  NOTE: Even tho Intel engineers designed the PNP configuration
	 *	  space, and even tho configuration alternatives read from
	 *	  the global "ReadPort" are delivered in standard little-
	 *	  endian fashion, multi-byte binary data must be stored into
	 *	  the configuration space in BIG-ENDIAN fashion!
	 */

	int x = n;

	while (x--) {
		/*
		 *  Write bytes to ports from right to left (rather than
		 *  standard left to right).
		 */

		_outp(ADDRESS_CMD_PORT, port++);
		_outp(WRITE_DATA_PORT, buf[x]);
	}

	return (n);
}

void
copyin(int port, char *buf, int n)
{
	int x = n;

	while (x--) {
		/*
		 *  Write bytes to ports from right to left (rather than
		 *  standard left to right).
		 */

		_outp(ADDRESS_CMD_PORT, port++);
		buf[x] = _inp(ReadPort);
	}
}

/*
 *  Program a PNP resource:
 *
 *  This routine is used to set resource usage for the currently
 *  selected ISA+ card.  This is done by storing data from the
 *  resource record at "rp" to the "n"th set of PNP configuration
 *  registers of type "t".
 *
 */
int
program_res(Resource *rp, int t, int n)
{
	int port = port_base[t] + (n * port_size[t]);
	unsigned long len = rp->length;
	int x, rc = 0;

	ASSERT(n < port_count[t]);

	switch (t) {

	case RESF_Port:

		/* Another I/O port ...				    */
		(void) copyout(port, (char *)&rp->base, 2);
		rc += 1;
		break;

	case RESF_Irq:

		/* Another IRQ; "len" holds level trigger flag.	    */
		len = (rp->base << 8) + 2 /* Assume level = high */ +
			    ((rp->EISAflags & RES_IRQ_TRIGGER) != 0);

		(void) copyout(port, (char *)&len, 2);
		rc += 1;
		break;

	case RESF_Dma:

		/* Another DMA channel ...			    */
		(void) copyout(port, (char *)&rp->base, 1);
		rc += 1;
		break;

	case RESF_Mem:

		if ((rp->EISAflags & RES_MEM_CODE_4GB) != 0) {
			/*
			 *  There are two types of memory resources;
			 *  32-bits wide and 24-bits wide.  They use
			 *  separate configuration registers.  We
			 *  assumed 24 bits upon entry and now must
			 *  change to the 32-bit registers.
			 */

			x = (int)(rp->EISAflags & RES_MEM_SIZE) >> 7;
			port = 0x70 + (n ? (n << 4) : 6);

			port += copyout(port, (char *)&rp->base, 4);
			_outp(ADDRESS_CMD_PORT, port++);
			_outp(WRITE_DATA_PORT, (x == 4) ? 6 : x);

			if (_inp(ReadPort) & 1) {
				len += rp->base;
			} else {
				len = ~(len - 1);
			}

			(void) copyout(port, (char *)&len, 4);

		} else {
			/*
			 *  .. And here's the 24-bit configuration
			 *  setup.  Note that only the high-order 16
			 *  bits of each 24-bit address/length are
			 *  actually stored in the cfg regs.
			 */

			port += copyout(port, ((char *)&rp->base)+1, 2);
			_outp(ADDRESS_CMD_PORT, port++);
			x = ((rp->EISAflags & RES_MEM_SIZE) ? 2 : 0);
			_outp(WRITE_DATA_PORT, x);

			if (_inp(ReadPort) & 1) {
				len += rp->base;
			} else {
				len = ~(len - 1);
			}
			(void) copyout(port, ((char *)&len)+1, 2);
		}

		rc += 1;
		break;
	}
	return (rc);
}

/*
 * Go through the input Board setting the resources to the configuration
 * space values. Note, we can't produce a new Board directly from
 * configuration space because the config space doesn't contain
 * the information about the length of IO space.
 */
void
get_programmed_pnp(Board *bp)
{
	Resource *rp = resource_list(bp);
	int io_cnt = 0, dma_cnt = 0, mem_cnt = 0, irq_cnt = 0;
	Resource *rp_end = rp + resource_count(bp);
	short t;

	bp->flags &= ~BRDF_PGM; /* device is not programmable */

	while (rp < rp_end) {
		t = rp->flags & RESF_TYPE;
		switch (t) {
		case RESF_Port:
			if (rp->flags & RESF_SUBFN)  {
				rp[2].flags &= ~RESF_ALT;
				rp[2].length = rp[1].length;
				copyin(port_base[t] + (port_size[t] * io_cnt),
					(char *)&rp[2].base, 2);
				io_cnt++;
			}
			rp += 3;
			break;

		case RESF_Mem:
			if (rp->flags & RESF_SUBFN)  {
				rp[2].flags &= ~RESF_ALT;
				rp[2].length = rp[1].length;
				if (rp->EISAflags & RES_MEM_CODE_4GB) {
					int port = 0x70 +
					    (mem_cnt ? (mem_cnt << 4) : 6);
					copyin(port,
					    (char *)rp[2].base, 4);
				} else {
					copyin(port_base[t] +
					    (port_size[t] * mem_cnt),
					    ((char *)&rp[2].base) + 1, 2);
				}
				mem_cnt++;
			}
			rp += 3;
			break;

		case RESF_Irq:
			rp[1].flags &= ~RESF_ALT;
			rp[1].length = 1;
			copyin(port_base[t] + (port_size[t] * irq_cnt),
				(char *)&rp[1].base, 1);
			irq_cnt++;
			rp += 2;
			break;

		case RESF_Dma:
			rp[1].flags &= ~RESF_ALT;
			rp[1].length = 1;
			copyin(port_base[t] + (port_size[t] * dma_cnt),
				(char *)&rp[1].base, 1);
			dma_cnt++;
			rp += 2;
			break;
		}
	}
}

void
program_pnp(Board *bp)
{
	/*
	 *  Program a PNP card:
	 *
	 *  This routine programs resource usage for the device described by
	 *  the board record at "bp".  We program all functions of the device
	 *  to use the resources currently assigned thereto (i.e, those whose
	 *  RESF_ALT bits have been cleared).
	 */

	int t, rc;
	int ax = 0;
	int x = 0;

	ResetCards(1);

	if ((rc = resource_count(bp)) != 0) {
		/*
		 *  This function has resources, which means we'll
		 *  have to program it ...
		 */

		if (!x++) {
			/*
			 *  If we haven't awakened the device yet,
			 *  do so now (and make sure it's inactive).
			 */

			_outp(ADDRESS_CMD_PORT, WAKE_CMD);
			_outp(WRITE_DATA_PORT, bp->pnp_csn);

			_outp(ADDRESS_CMD_PORT, LOGICAL_DEVICE);
			_outp(WRITE_DATA_PORT, bp->pnp_ldn);

			_outp(ADDRESS_CMD_PORT, ACTIVATE);
			_outp(WRITE_DATA_PORT, 0);
		}

		for (t = RESF_Port; t < RESF_Max; t++) {
			/*
			 *  Program resources by type, doing all the
			 *  ports first, IRQs next, etc.
			 */

			int j, k = 0;
			Resource *rp = resource_list(bp);

			for (j = rc; j--; rp++) {
				/*
				 *  Step thru the resource list look-
				 *  ing for resources of the current
				 *  type ("t" register).
				 */

				if (t == (rp->flags & (RESF_TYPE
						    + RESF_ALT))) {
					/*
					 *  Here's another resource of
					 *  the type we're interested
					 *  in, let's program it!
					 */

					ax += program_res(rp, t, k++);
				}
			}
		}
	}

	if (ax) {
		/*
		 *  If we successfully allocated resources to this device,
		 *  we can now activate it ...
		 */

		_outp(ADDRESS_CMD_PORT, IO_RANGE_CHECK);
		_outp(WRITE_DATA_PORT, 0);

		_outp(ADDRESS_CMD_PORT, ACTIVATE);
		_outp(WRITE_DATA_PORT, 1);
	}

	ResetCards(0);
}

/*
 * Unprogram the pnp logical device by deactivating it from the bus
 */
void
unprogram_pnp(Board *bp)
{
	ResetCards(1);

	_outp(ADDRESS_CMD_PORT, WAKE_CMD);
	_outp(WRITE_DATA_PORT, bp->pnp_csn);
	_outp(ADDRESS_CMD_PORT, LOGICAL_DEVICE);
	_outp(WRITE_DATA_PORT, bp->pnp_ldn);
	_outp(ADDRESS_CMD_PORT, ACTIVATE);
	_outp(WRITE_DATA_PORT, 0); /* de-activate */

	ResetCards(0);
}

unsigned long
ReadBits(unsigned port, int size)
{
	/*
	 *  Serially read ID info:
	 *
	 *  This routine reads a "size"-byte value from the indicated I/O
	 *  "port" and returns it to the caller.  The value is read serially,
	 *  one bit at a time!
	 */

	int j;
	unsigned long x = 0;

	size *= 8;
	delay(3000);

	for (j = 0; j < size; j++) {
		/*
		 *  Read value bit separately.  Each time we get the magic
		 *  "0x55AA" value from the global read port, it signifies
		 *  a ONE bit.
		 */

		long b0 = (_inp(port) == 0x55);
		long b1 = (_inp(port) == 0xAA);
		x |= ((b0 & b1) << j);
		delay(1000);
	}

	return (x);
}

unsigned
CheckSum(unsigned long vid, unsigned long sid)
{
	/*
	 *  Calculate PNP checksum:
	 *
	 *  Returns a checksum word (OK, so it's only a byte) that can be
	 *  used to validate an ISA+ card's vendor ID and serial number.
	 */

	int j, k;
	unsigned long x = vid;
	unsigned char n = 0x6A;

	for (j = 2; j--; x = sid) {
		/*
		 *  We have two words to sum over:  The vendor ID and the
		 *  card's serial number.  The "x" register holds the cur-
		 *  rent ID word.
		 */

		for (k = 32; k--; x >>= 1) {
			/*
			 *  Calculate 8-bit cyclic checksum.
			 */

			n = (n >> 1) | (((n ^ (n >> 1) ^ x) & 1) << 7);
		}
	}

	return (n);
}

int
IsolateCard(unsigned port)
{
	/*
	 *  Isolate an ISA+ card:
	 *
	 *  This routine attempts to isolate the next ISA+ card by reading
	 *  a vendor ID/Serial number/Checksum thru the indicated "port",
	 *  assigning the indicated card the next available card ID number,
	 *  then reading the card ID back thru the input port.  Returns a
	 *  non-zero value if this all works!
	 */

	unsigned long vid = ReadBits(port, sizeof (vid));
	unsigned long sid = ReadBits(port, sizeof (sid));
	unsigned char sum = ReadBits(port, sizeof (sum));

	csn = (PnpCardCnt + 1);

	if ((vid || sid) && (!sum || (sum == CheckSum(vid, sid)))) {
		/*
		 *  The vendor ID and serial numbers checksum properly (or
		 *  card has no checksum).  See if we can assign a card ID
		 *  number to this device.
		 */


		_outp(ADDRESS_CMD_PORT, SET_CARD_SELECT_NO);
		_outp(WRITE_DATA_PORT, csn);
		return (_inp(port) == csn);
	}

	return (0);
}

int
FindNextCard()
{
	/*
	 *
	 *  Locate the Next ISA+ Card:
	 *
	 *  This routine searches for PnP ISA cards that haven't been
	 *  isolated yet.  If we find one, we assign isolate it from the
	 *  rest of the PnP configuration space by assigning it the next
	 *  available card sequence number.
	 *
	 *  Returns the CSN of the next isolated card, or 0 if there are
	 *  no more ISA+ cards to be isolated!
	 */

	static int p_start = PORTRANGE_START;
	static int p_end = PORTRANGE_END;
	int j;

	for (j = p_start; j <= p_end; j += 16) {
		/*
		 *  Outer loop increments the "j" register thru the possible
		 *  input ports (minimum port range is 4 bytes, even tho we
		 *  only use 1 of them).  If "p_start == p_end", then we've
		 *  already found the global configuration port, otherwise
		 *  we're still looking.
		 */

		if (!Query_Port(j, 1)) {
			/*
			 *  This port ("j" register) is either available or
			 *  known to be the global read port.  Use it to try
			 *  and isolate the next ISA+ card.
			 */

			_outp(ADDRESS_CMD_PORT, WAKE_CMD);
			_outp(WRITE_DATA_PORT, 0);
			_outp(ADDRESS_CMD_PORT, SET_READ_DATA);
			_outp(WRITE_DATA_PORT, (j >> 2));
			_outp(ADDRESS_CMD_PORT, SERIAL_ISOLATION);

			if (IsolateCard(j)) {
				/*
				 *  We were able to successfully isolate
				 *  another ISA+ card.  This means we now
				 *  know where our global read port is!
				 */

				ReadPort = p_start = p_end = j;
				return (PnpCardCnt+1);
			}
		}
	}

	return (0);
}

void
init_pnp()
{
	/*
	 *  Initialize PNP ISA enumerator:
	 *
	 *  All we do here is count the number of ISA+ cards that are cur-
	 *  rently plugged in, and assign serial ID numbers to them.  We
	 *  then set these cards into the "Wait for key" state.
	 */

	static int recur = 0;


	if (!recur++) {
		/*
		 * First check if the fixed ports are available.
		 * Note, we don't check if 0x279 is available as it can
		 * can also be used (shared) by lpt2 (0x278-27B)
		 */
		if (Query_Port(WRITE_DATA_PORT, 1)) {
			return;
		}

		/*
		 * Check the pnpbios which already has the number
		 * of cards and ReadPort address. Sanity check returns as
		 * some pnpbioses (eg EDJones) are buggy. They return
		 * 0xff as the PnpCardCnt, and 0xffff as the ReadPort.
		 */
		if ((get_pnp_info_pnpbios(&PnpCardCnt, &ReadPort) == 0) &&
		    (PnpCardCnt != 0xff) &&
		    (ReadPort >= PORTRANGE_MIN) &&
		    (ReadPort <= PORTRANGE_END)) {
			return;
		}

		/*
		 * PnP bios not installed (or buggy) so count the cards
		 * ourselves. We only have to do this once. We have to
		 * assume that no PNP cards will magically appear between
		 * calls to the ISA configuration screens!
		 */
		ResetCards(1);
		PnpCardCnt = 0;
		ReadPort = 0;

		_outp(ADDRESS_CMD_PORT, CONFIG_CONTROL);
		_outp(WRITE_DATA_PORT, 4);
		_outp(WRITE_DATA_PORT, 2);

		for (ResetCards(1); FindNextCard(); PnpCardCnt++) {
			;
		}
		ResetCards(0);
	}
}

#define	RES_START 0x40
#define	RES_END 0x76

/*
 * Clear all the config space.
 * This shouldn't be necessary but some cards (eg eepro) seem to
 * look at this space and respond to the isa bus, even when
 * de-activated (told to stay off bus).
 * This fixes those cards.
 */
void
clear_config_pnp()
{
	int port;

	for (port = RES_START; port < RES_END; port++) {
		_outp(ADDRESS_CMD_PORT, port);
		_outp(WRITE_DATA_PORT, 0);
	}
}
