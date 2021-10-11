/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bootio.c	1.7	99/04/14 SMI\n"

/*
 * Low-level I/O and device type checking routines.
 */

#include <sys\types.h>
#include <bioserv.h>	/* BIOS interface support routines */
#include "bootblk.h"

#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#endif

#pragma comment(compiler)
#pragma comment(user, "bootio.c	1.7	99/04/14")

ushort int13_check_ext_present(unchar, ushort *);
ushort int13_get_params(unchar, ushort *, ushort *, ushort *, unchar *);
ushort int13_packet(unchar, ushort, char _FAR_ *);

char AllocErr[] = "Cannot allocate deblocking buffer.";
char CylRangeErr[] = "Requested cylinder is beyond range of BIOS geometry.";
char GeomErr[] = "Cannot read device geometry.";

char block_buffer[2048];
struct bios_dev boot_dev;
static unsigned short io_count;

/*
 * This defines the Device Address Packet in Table 1 of the
 * BIOS Enhanced Disk Drive Specification, version 3.0
 */
struct ext_dev_addr_pkt {
	unchar	pktsize;
	unchar	reserved1;
	unchar	numblocks;
	unchar	reserved2;
	char _FAR_ *bufaddr;
	ulong	lba_addr_lo;
	ulong	lba_addr_hi;
	ulong	bigbufaddr_lo;
	ulong	bigbufaddr_hi;
};

/*
 * Result buffer for the extended getparams (13/48h).  So far,
 * this structure only supports what's needed for EDD-1.1 compliance.
 */
struct ext_getparam_resbuf {
	ushort	bufsize;
	ushort	info_flags;
	ulong	cyls;
	ulong	heads;
	ulong	secs;
	ulong	num_secs_lo;
	ulong	num_secs_hi;
	ushort	bytes_per_sec;
	void __far *dpte;			/* Only if bufsize == 30 */
};

// Read absolute sectors from the specified device.
// Return the number of sectors read.
unsigned short
read_sectors(struct bios_dev *dev, ulong start_sect,
	ushort sect_count, char far *buffer)
{
	ushort answer = 0;
	union {
		char far *p;
		struct {
			ushort off;
			ushort seg;
		} h;
	} u;

	io_count++;
	u.p = buffer;

	Dprintf(DBG_READ, ("read_sectors: %d sector%s starting at %ld "
		"into %x:%x.\n", sect_count,
		(char _FAR_ *)(sect_count == 1 ? "" : "s"),
		start_sect, u.h.seg, u.h.off));

	if (dev->use_LBA) {
		struct ext_dev_addr_pkt lba_packet;
		ulong blockNum;
		ushort blockOff;
		ushort blockCnt;
		ushort sectors;
		ushort result;
		unchar error_code;

		while (sect_count > 0) {
			/* Calculate read parameters in terms of blocks */
			blockOff = start_sect % boot_dev.u_secPerBlk;
			blockNum = start_sect / boot_dev.u_secPerBlk;
			blockCnt = sect_count / boot_dev.u_secPerBlk;

			/*
			 * Read one or more complete blocks into user buffer
			 * if on block boundary and have at least one block
			 * to read.
			 */
			if (blockOff == 0 && blockCnt > 0) {
				if (blockCnt > 127)
					blockCnt = 127;
				sectors = blockCnt * boot_dev.u_secPerBlk;
				lba_packet.pktsize = 0x10;
				lba_packet.reserved1 = 0;
				lba_packet.numblocks = blockCnt;
				lba_packet.reserved2 = 0;
				lba_packet.bufaddr = u.p;
				lba_packet.lba_addr_lo = blockNum;
				lba_packet.lba_addr_hi = 0;

				result = int13_packet(dev->BIOS_code, 0x4200,
					(char _FAR_ *)&lba_packet);

				Dprintf(DBG_READD, ("Read of %d block%s "
					"starting at %lx into %x:%x %s.\n",
					blockCnt, (char _FAR_ *)
					(blockCnt == 1 ? "" : "s"),
					blockNum, u.h.seg, u.h.off,
					(char _FAR_ *)(result ? "failed" :
					"succeeded")));

				if (result != 0)
					break;
			} else {
				/*
				 * Read one block into local buffer and copy
				 * a portion to the user buffer.
				 */
				sectors = boot_dev.u_secPerBlk - blockOff;
				if (sectors > sect_count)
					sectors = sect_count;
				lba_packet.pktsize = 0x10;
				lba_packet.reserved1 = 0;
				lba_packet.numblocks = 1;
				lba_packet.reserved2 = 0;
				lba_packet.bufaddr =
					(char _FAR_ *)block_buffer;
				lba_packet.lba_addr_lo = blockNum;
				lba_packet.lba_addr_hi = 0;

				result = int13_packet(dev->BIOS_code, 0x4200,
					(char _FAR_ *)&lba_packet);

				Dprintf(DBG_READD, ("Read of 1 block starting "
					"at %lx into local buffer %s.\n",
					blockNum, (char _FAR_ *)(result ?
					"failed" : "succeeded")));

				if (result != 0) {
					Dprintf(DBG_READD, ("Blocks read: %d, "
						"error code: %x.\n",
						lba_packet.numblocks,
						result));
					break;
				}

				Dprintf(DBG_READD, ("Copying %d sector%s from "
					"local buffer to %x:%x.\n", sectors,
					(char _FAR_ *)(sectors == 1 ? "" : "s"),
					u.h.seg, u.h.off));

				memcpy(u.p, (char _FAR_ *)block_buffer +
					blockOff * 512,	sectors * 512);
			}
			answer += sectors;
			sect_count -= sectors;
			start_sect += sectors;
			u.h.seg += sectors * (512 >> 4);
		}
	} else {
		register unsigned short BIOSCylinder, BIOSSector, BIOSHead;
		union fdrc_t devrc;
		unsigned short rc;

		BIOSCylinder = (unsigned short)(start_sect / dev->u_secPerCyl);
		BIOSSector = (unsigned short)(start_sect % dev->u_secPerCyl);
		BIOSHead = (unsigned short)(BIOSSector / dev->u_secPerTrk);
		BIOSSector = (BIOSSector % dev->u_secPerTrk) + 1;

		Dprintf(DBG_READD, ("read_disk: start_sect %ld\n",
			start_sect));
		Dprintf(DBG_READD, ("SecPerTrk: %d\n", dev->u_secPerTrk));
		Dprintf(DBG_READD, ("SecPerCyl: %d\n", dev->u_secPerCyl));
		Dprintf(DBG_READD, ("Head: %d\n", BIOSHead));
		Dprintf(DBG_READD, ("Cyl: %d\n", BIOSCylinder));
		Dprintf(DBG_READD, ("Sector: %d\n", BIOSSector));

		/* BIOS cylinder limit - 10 bits! */
		if (BIOSCylinder > 1023) {
			c_fatal_err(CylRangeErr);
		}

		if (BIOSCylinder > 255) {	/* reconstruct BIOS geometry */
			_asm {
				push ax
				push dx
				xor dx, dx
				mov ax, BIOSCylinder
				mov dl, al;	lower 8 bits of cylinder num
				xor al, al
				shr ax, 2
				add ax, BIOSSector
				mov BIOSCylinder, dx
				mov BIOSSector, ax
				pop dx
				pop ax
			}
		}

		devrc.retcode = (short)read_disk(dev->BIOS_code, BIOSCylinder,
			BIOSHead, BIOSSector, sect_count, buffer);
		rc = (dev->BIOS_code == Drive_A) ?
			devrc.f.fd_rc : devrc.retcode;
		answer = ((rc == 0) ? sect_count : 0);
	}
	return (answer);
}

/* Routine for detecting real (as opposed to cache) I/O for spinners */
int
io_done(void)
{
	static int saved_io_count;
	int answer = (io_count != saved_io_count);

	saved_io_count = io_count;
	return (answer);
}

/*
 * The following algorithm is also in boot.bin (prom.c) and bootconf
 * (biosprim.c).  Any enhancements made here should be made in those
 * places too.
 */
int
is_eltorito_device(unchar boot_dev_code, unsigned short LBA_read_supported,
		unsigned short LBA_block_size)
{
	int answer = 0;
	ushort error;

	// Issue an INT 13 function 4B, subfunction 1
	// which returns El Torito disk emulation status without
	// terminating emulation.

	{
		unsigned char spec_packet[0x13];
		int i;

		for (i = 0; i < sizeof (spec_packet); i++) {
			spec_packet[i] = 0;
		}

		spec_packet[0] = 0x13;
		spec_packet[2] = (boot_dev_code ^ 0xFF);

		error = int13_packet(boot_dev_code, 0x4B01, spec_packet);

		// The El Torito spec is not clear about what the carry flag
		// means for this call.  So if it returns CF set, examine the
		// buffer to determine whether it looks like the call worked.
		// Firstly we require that the boot device code matches.
		// Secondly we require the LBA address of the bootstrap or
		// image to be non-zero.  Thirdly we require the boot device
		// not to be one of the standard device codes for bootable
		// diskette or hard drive unless the CDROM is booted using an
		// emulated image.
		if ((error & BIOS_CARRY) && *(long *)(&spec_packet[4]) != 0 &&
				spec_packet[2] == boot_dev_code) {
			if ((boot_dev_code != 0 && boot_dev_code != 0x80) ||
					(spec_packet[1] & 0xF) != 0)
				error = 0;
		}

		if (error == 0 && spec_packet[0] >= 0x13 &&
				spec_packet[2] == boot_dev_code) {

			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
				printf("Emulation status call indicates "
					"El Torito boot.\n");

			answer = 1;
		} else {
			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
				printf("Emulation status call: error "
					"%x, size %x, device code %x.\n",
					error, spec_packet[0], spec_packet[2]);
		}

		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO)) {
			printf("Emulation data:");
			for (i = 0; i < 0x13; i++)
				printf(" %x", spec_packet[i]);
			printf("\n");
		}

		if (answer)
			return (answer);
	}

	// El Torito devices booted under "no-emulation mode" are supposed
	// to have device codes in the range 0x81 - 0xFF.  Assume the device
	// is not El Torito if the code is not in this range.  Note that
	// placing this test here allows us to tolerate an invalid device
	// code so long as the BIOS implements the 4B01 call properly.
	if (boot_dev_code < 0x81)
		return (0);

	// El Torito devices always have LBA access with 2K block size.
	// If the BIOS reported a larger block size, assume that the device
	// is not El Torito.  The primary purpose of this test is to avoid
	// trying to read from a device whose block size is too large for
	// our buffer.  We do not reject devices that report block sizes
	// smaller than 2K because we have seen at least one El Torito BIOS
	// report a block size of 512.
	if (LBA_read_supported && LBA_block_size > 2048)
		return (0);

	// Attempt an LBA read of block 0x11.  If it looks like an El Torito
	// BRVD assume we are booted from an El Torito device.
	{
		struct ext_dev_addr_pkt lba_packet;

		lba_packet.pktsize = 0x10;
		lba_packet.reserved1 = 0;
		lba_packet.numblocks = 1;
		lba_packet.reserved2 = 0;
		lba_packet.bufaddr = (char _FAR_ *)block_buffer;
		lba_packet.lba_addr_lo = 0x11;
		lba_packet.lba_addr_hi = 0;

		error = int13_packet(boot_dev_code, 0x4200,
				(char _FAR_ *)&lba_packet);
		if (error == 0 && block_buffer[0] == 0 &&
		    strncmp(block_buffer + 1, "CD001", 5) == 0 &&
		    strncmp(block_buffer + 7, "EL TORITO SPECIFICATION",
				23) == 0) {
			answer = 1;
			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
				printf("Boot device contains BRVD at block "
					"11.\n");
		} else if (error) {
			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
				printf("Boot device LBA read of block 11 "
					"failed with error code %x.\n", error);
		} else {
			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK |
					DBG_ELTORITO)) {
				int i;

				printf("Boot device block 11 is not BRVD:\n");
				for (i = 0; i < 26; i++)
					printf(" %x", block_buffer[i]);
				printf("\n");
			}
		}
	}
	return (answer);
}

void
chk_boot_device(unchar boot_dev_code)
{
	unsigned short LBA_read_supported = 0;
	unsigned short LBA_block_size = 0;
	char *s;

	// Debugging can be enabled in a debug build of the program
	// or by patching flags into BootDbg.  The CDROM version of
	// pboot includes a mechanism to allow users to request
	// debugging.  The request results in DBG_ELTORITO being
	// turned on.
	if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
		printf("\nDisplaying Solaris boot debug data.\n\n");

	boot_dev.BIOS_code = boot_dev_code;

	if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
		printf("Checking boot device %x.\n", boot_dev.BIOS_code);

	// Ideally we would use INT 21h "check extensions present" (func 41h)
	// and "get drive parameters" (func 48h) to determine whether 2K LBA
	// reads are supported for the boot device.  But some BIOSes that
	// support El Torito boot do not implement both these calls.

	{
		unsigned short present = 0;
		unsigned short mask = 0;

		present = int13_check_ext_present(boot_dev_code, &mask);

		if (present && (mask & 1))
			LBA_read_supported = 1;

		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
			printf("BIOS reports INT 13 extensions are%s "
				"present, mask %x.\n",
				present ? (char _FAR_ *)"" :
				(char _FAR_ *)" not", mask);

		/*
		 * If fixed disk access subset is present, check for LBA
		 * block size.  Other subsets implement the get drive
		 * parameters call too, but the block size is likely to
		 * be meaningless unless fixed disk access is supported.
		 */
		if (mask & 1) {
			unsigned short param_pkt[13];
			ushort error;

			param_pkt[0] = sizeof (param_pkt);
			param_pkt[12] = 0;

			error = int13_packet(boot_dev_code, 0x4800,
				(char _FAR_ *)param_pkt);
			if (error == 0 && (param_pkt[12] & 0x1FF) == 0) {
				LBA_block_size = param_pkt[12];
			}

			if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
				printf("BIOS INT 13 function 48 %s %x.\n",
					(char _FAR_ *)(error ?
					"failed with error code" :
					"reported blocksize"),
					error ? error : param_pkt[12]);
		}
	}

	if (is_eltorito_device(boot_dev_code, LBA_read_supported,
			LBA_block_size))
		boot_dev.dev_type = DT_CDROM;

	if (boot_dev.dev_type == DT_UNKNOWN) {

		// Device is not CDROM.  Use drive parameters to
		// distinguish hard drive from diskette by checking
		// the maximum head number (assume < 2 means diskette).

		// We could also get here if there are BIOSes that implement
		// El Torito boot but not function 4B01.  For now we just
		// hope noone was that lazy.

		ushort heads;
		ushort cylinders;
		ushort sectors;
		ushort error;
		unchar drive_type;

		error = int13_get_params(boot_dev_code, &cylinders,
			&heads, &sectors, &drive_type);

		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
			printf("int13_get_params: error %x, heads %d.\n",
				error, heads);

		if (error == 0 && heads > 2)
			boot_dev.dev_type = DT_HARD;
		else
			boot_dev.dev_type = DT_FLOPPY;
	}

	switch (boot_dev.dev_type) {
	case DT_UNKNOWN:
	default:
		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
			printf("Boot device type %d.\n", boot_dev.dev_type);

		c_fatal_err("Cannot identify boot device type.");
		break;
	case DT_HARD:
		s = "hard drive";
		if (LBA_read_supported)
			boot_dev.use_LBA = 1;
		break;
	case DT_FLOPPY:
		/* Never use LBA for diskette even if claimed to work */
		s = "diskette";
		boot_dev.use_LBA = 0;
		break;
	case DT_CDROM:
		/*
		 * Always use LBA for CDROM because it is required for
		 * El Torito but some ET BIOSes do not claim to support
		 * LBA for ET devices.
		 */
		s = "CDROM";
		boot_dev.use_LBA = 1;
		break;
	}

	// If the boot device will be accessed using standard BIOS read,
	// determine the device parameters.  Otherwise determine the
	// LBA block size.
	if (boot_dev.use_LBA == 0) {

		ushort heads;
		ushort cylinders;
		ushort sectors;
		ushort error;
		unchar drive_type;

		error = int13_get_params(boot_dev_code, &cylinders,
			&heads, &sectors, &drive_type);

		if (error) {
			c_fatal_err(GeomErr);
		}

		boot_dev.u_secPerTrk = sectors;
		boot_dev.u_nCyl = cylinders;
		boot_dev.u_trkPerCyl = heads;
		boot_dev.u_secPerCyl = sectors * heads;
		boot_dev.u_secPerBlk = 1;
		boot_dev.drive_type = drive_type;

		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO)) {
			printf("chk_boot_device: %d cyl, %d heads, %d "
				"sectors\n", cylinders, heads, sectors);
			printf("chk_boot_device: %d u_nCyl, %d u_secPerCyl\n",
				boot_dev.u_nCyl, boot_dev.u_secPerCyl);
		}

	} else if (boot_dev.dev_type == DT_CDROM) {
		boot_dev.u_secPerBlk = 4;
	} else {
		// Issue an extended get drive parameters call even
		// if it was not indicated to be present.
		struct ext_getparam_resbuf param_buf;
		ushort error;

		param_buf.bufsize = 26;
		error = int13_packet(boot_dev_code, 0x4800,
			(char _FAR_ *)&param_buf);

		// If the parameters call worked and gave a reasonable
		// block size, use it.  Otherwise assume blocks are 512.
		boot_dev.u_secPerBlk = 0;
		if (error == 0 && param_buf.bytes_per_sec > 0 &&
				(param_buf.bytes_per_sec % 512) == 0) {
			boot_dev.u_secPerBlk = param_buf.bytes_per_sec / 512;
		}
		if (boot_dev.u_secPerBlk == 0) {
			boot_dev.u_secPerBlk = 1;
		}

		// If the block size is greater than 1 sector, allocate a
		// buffer for deblocking.
		if (boot_dev.u_secPerBlk > 4) {
			c_fatal_err(AllocErr);
		}
	}

	if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO)) {
		printf("Boot device type is %s.\n", (char _FAR_ *)s);
		printf("Will%s use LBA read.\n",
			(char _FAR_ *)(boot_dev.use_LBA ? "" : " not"));
	}

	if (boot_dev.use_LBA) {
		if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO))
			printf("LBA block is %d sector%s.\n",
				boot_dev.u_secPerBlk, (char _FAR_ *)
				(boot_dev.u_secPerBlk == 1 ? "" : "s"));
	}

	if (BootDbg & (DBG_BIOS | DBG_NONUMLOCK | DBG_ELTORITO)) {
		printf("\nEnd of Solaris boot debug data.\n\n");
		pause(DBG_ALWAYS, 0);
	}
}

void
memcpy(char far *dest, char far *src, ushort count)
{
	Dprintf(DBG_COPY, ("memcpy: copying %d bytes from %x:%x to %x:%x.\n",
		count, (ushort)((long)src >> 16), (ushort)src,
		(ushort)((long)dest >> 16), (ushort)dest));

	_asm {
		push	si
		push	di
		push	ds
		push	es
		mov	cx, count
		les	di, dest
		lds	si, src
		jcxz	cpy_done
		cld
		rep	movsb
	cpy_done:
		pop	es
		pop	ds
		pop	di
		pop	si
	}
}

/*
 * Execute an extended INT 13 packet-style BIOS call.
 * Return a combination of AH (low byte) and CY (low
 * bit of high byte).  Return value of 0 theoretically
 * means success, but might need to check components
 * individually for some calls if BIOSes do not
 * implement them properly.
 */
ushort
int13_packet(unchar dev, ushort func, char _FAR_ *pack)
{
	ushort result;
	ushort carry = 0;

	_asm {
		push	ds
		push	si
		mov	ax, func
		mov	dl, dev
		lds	si, pack
		int	13h
		pop	si
		pop	ds
		jnc	pack_ok
		mov	carry, 1
	pack_ok:
		mov	al, ah
		mov	ah, 0
		mov	result, ax
	}

	if (carry)
		result |= BIOS_CARRY;

	Dprintf(DBG_INT13, ("int13_packet: dev %x, func %x, result: %x.\n",
		dev, func, result));

	return (result);
}

ushort
int13_get_params(unchar dev, ushort *pcyl, ushort *phead, ushort *psect,
		unchar *drive_type)
{
	unchar head, sect, dtype;
	ushort cyl;
	ushort result;
	unchar carry = 1;

	_asm {
		push	bx
		mov	ah, 8
		mov	dl, dev
		int	13h
		mov	al, ah
		mov	ah, 0
		mov	result, ax
		jc	paramDone
		mov	carry, 0
		mov	head, dh
		mov	sect, cl
		and	sect, 3Fh
		shr	cl, 6
		xchg	cl, ch
		mov	cyl, cx
		mov	dtype, bl
	paramDone:
		pop	bx
	}
	if (carry) {
		result |= BIOS_CARRY;
		Dprintf(DBG_BIOS | DBG_NONUMLOCK,
			("int13_get_params failed\n"));
	} else {
		*pcyl = cyl - 1;
		*phead = head + 1;
		*psect = sect;
		*drive_type = dtype;
		Dprintf(DBG_BIOS | DBG_NONUMLOCK, ("int13_get_params: %d cyl, "
			"%d heads, %d sectors, type %x\n",
			*pcyl, *phead, *psect, *drive_type));
	}
	return (result);
}

// Issue an INT 13h check extensions present (41H) call.
// Returns 0 for failure, 1 for success.  Returns the
// "mask value" either way.
//
// We have seen at least one BIOS where this call set
// BP to AA55 instead of BX.  So we save and restore
// BP to avoid trashing the stack.  We DO NOT take
// BP of AA55 to indicate success.
ushort
int13_check_ext_present(unchar dev, ushort *pmask)
{
	ushort result = 0;
	ushort mask;

	_asm {
		push	bp
		push	bx
		mov	ah, 41h
		mov	bx, 55AAh
		mov	dl, dev
		int	13h
		jc	noExt;		Require no carry and special
		cmp	bx, 0AA55h;	value in BX for success
		jne	noExt
		mov	result, 1
	noExt:
		mov	mask, cx
		pop	bx
		pop	bp
	}
	*pmask = mask;
	return (result);
}
