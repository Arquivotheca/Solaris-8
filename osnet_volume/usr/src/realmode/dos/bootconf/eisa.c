/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * eisa.c -- eisa bus enumerator
 */

#ident "@(#)eisa.c   1.49   97/08/12 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <conio.h>
#include <dos.h>
#include <names.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"

#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "escd.h"
#include "eisa.h"
#include "enum.h"
#include "gettext.h"
#include "main.h"
#include "menu.h"
#include "probe.h"
#include "resmgmt.h"
#include "tty.h"

#define	HEX_VERSION 0x0100
#define	BIOS_ROM_BASE  ((char far *)0xF0000000)
#define	EISA_SIG_BASE  ((char far *)0xF000FFD9)
#define	EISA_EXP_SLOT	0x0
#define	EISA_EMBED_SLOT	0x1

/*
 * Function prototypes
 */
unsigned long readEISAid(u_char slot);
Board *ReadEisaNvram(u_char slot);
Board *AddFunction_eisa(Board *bp, EisaFuncCfgInfo *efp);
Board *iss_fixup(Board *bp);

/*
 * External globals
 */
int Eisa = 0;

void
init_eisa()
{
	if (Main_bus == RES_BUS_EISA) {
		Eisa = 1;
	}
}

/*
 * Read EISA function record:
 *
 * This routine uses EISA BIOS services to read "unpacked" function
 * records from the NVRAM. The caller must supply the "slot" and
 * "func"tion numbers, as well as a pointer to the "buf"fer where
 * the corresponding function is to be stored.
 */
void
GetEisaFctnTab(int slot, int func, void *buf)
{
	unsigned char x;
	memset(buf, 0, sizeof (EisaFuncCfgInfo));

#ifdef __lint
	x = slot + func;
#else
	_asm {
		/*
		 * EISA BIOS service call 15/D8/1 returns an "unpacked"
		 * function record at "buf".
		 */
		push  si
		push  ds
		mov   ax, 0D801h
		mov   cl, byte ptr [slot];	Slot number to %cl
		mov   ch, byte ptr [func];	Function number to %ch
		mov   si, word ptr [buf];	Buffer addr to ds:si
		mov   ds, word ptr [buf+2]
		int   15h;			Call BIOS
		mov   x, ah
		pop   ds
		pop   si
	}
#endif

	ASSERT(x == 0);
}

/*
 * Read the packed format EISA ID from the actual slot
 * EISA Id's are 4 bytes at 0xzC80 - 0xzC83 where z is slot no.
 *
 * Returns:
 *	An unsigned long with the 4 bytes from the slot
 */
unsigned long
readEISAid(u_char slot)
{
	unsigned long eisaid = 0;
	union _REGS inregs, outregs;

	/*
	 * Make EISA read physical slot info BIOS call
	 */
	inregs.h.ah = 0xd8;
	inregs.h.al = 4;
	inregs.h.cl = slot;
	_int86(0x15, &inregs, &outregs);
	if (outregs.h.ah == 0)
		eisaid = (unsigned long)outregs.x.di |
			((unsigned long)outregs.x.si << 16);
	return (eisaid);
}

/*
 * Read EISA Configuration Info:
 *
 * This routine reads ESCD  configuration information from the EISA
 * NV-RAM on a slot-by-slot basis. The caller passes the "slot"
 * number of the last board record returned by this routine (or -1
 * to indicate that no records have yet been delivered). We locate
 * the next non-empty slot in the current EISA configuration, allocate
 * a board record for this slot, and copy the configuration
 * information from the NV-RAM to this record.
 *
 * Returns the address of the "Board" record describing the next non-empty
 * slot; NULL if there are no more occupied slots in the configuration.
 */
Board *
ReadEisaNvram(u_char slot)
{
	int fc;
	unsigned long slothwid;
	int slottype, errcode;
	Board *cbp;

	/*
	 * Check each possible slot to see if there's data there that
	 * needs to be extracted. We do this by issuing a BIOS call
	 * to determine which slots are actually occupied, then
	 * reading the functions out of these slots one by one.
	 */
	while (++slot < MAX_EISA_SLOTS) {
		union _REGS inregs, outregs;

		/*
		 * Make EISA read slot info BIOS call
		 */
		inregs.h.ah = 0xd8;
		inregs.h.al = 0;
		inregs.h.cl = slot;
		_int86(0x15, &inregs, &outregs);
		fc = outregs.h.dh;
		errcode = outregs.h.ah;
		slottype = (outregs.h.al >> 4) & 0x3;
		/* get id from actual hardware in slot */
		if (errcode == 0 && slottype == EISA_EXP_SLOT) {
			slothwid = readEISAid(slot);
			if (slothwid == 0) {
				continue;
			}
		} else
			slothwid = 0xffffffff;

		if (errcode == 0 && fc > 0) {
			/*
			 * There appears to be at least one function
			 * configured in the current slot. Allocate
			 * a "Board" record and start converting stuff!
			 */
			int j;
			Board *bp, *bp2, *tbp;
			Resource *rp;
			int rc;
			static EisaFuncCfgInfo func;

			bp = new_board();

			for (j = 0; j < fc; j++) {
				/*
				 * Extract the EISA configuration information
				 * for each function and convert it to
				 * canonical form.
				 */
				GetEisaFctnTab(slot, j, (void *)&func);

				if (j == 0) {
					/*
					 * If this is the first function, pull
					 * the board-level information out of
					 * the unpacked function buffer before
					 * attempting to pack it.
					 */
					bp->bustype = RES_BUS_EISA;
					bp->slot = slot;
					bp->devid = fetchESCD(func.
					    abCompBoardId, unsigned long);
					/*
					 * Check that slot hardware id matches
					 * NVRAM info except for slot 0
					 */
					if ((slothwid != 0xffffffff) &&
					    (bp->devid != slothwid) &&
					    (slot != 0)) {
						enter_menu(NULL,
							"MENU_HW_NVRAM_NOMATCH",
							slot);
					}

					/*
					 * Initialise with the io slot range
					 */
					bp = AddResource_devdb(bp, RESF_Port,
					    ((u_long) slot) * 0x1000, 0x1000);
				}

				debug(D_EISA, "Eisa slot %d func %d\n",
				    slot, j);
				/*
				 * Don't add any floppy functions
				 */
				bp2 = copy_boards(bp);
				bp = AddFunction_eisa(bp, &func);
				for (rp = resource_list(bp),
				    rc = resource_count(bp); rc--; rp++) {
					if (rp->flags & RESF_ALT) {
						continue;
					}
					if ((cbp = Query_resmgmt(Head_board,
					    (rp->flags & RESF_TYPE),
					    rp->base, rp->length)) == 0) {
						continue;
					}
					if (cbp->devid ==
						CompressName("ISY0050")) {
						/*
						 * found floppy conflict -
						 * swap boards
						 */
						tbp = bp;
						bp = bp2;
						bp2 = tbp;
						break;
					}
				}
				free_board(bp2);
			}
			/*
			 * Fix up for iss .cfg files - which don't include
			 * the irq or the shared memory
			 */
			bp = iss_fixup(bp);
			return (bp);
		}
nextslot:
		;
	}
	debug(D_EISA, "End of eisa slots\n");
	if ((Debug & (D_EISA|D_TTY)) == (D_EISA|D_TTY)) {
		iprintf_tty("<press return to continue>");
		(void) getc_tty();
	}
	return (0);
}

/*
 * Enumerate EISA devices:
 *
 * All we have to do is read the EISA NVRAM and build board records
 * to describe all the devices listed therein.
 */
void
enumerator_eisa()
{
	Board *bp;
	Resource *rp;
	int rc;
	u_char slot;

	if (!Eisa) {
		return;
	}

	/*
	 * Start at slot 1 - so that we ignore the motherboard at slot 0.
	 *
	 * We hand craft a standardised motherboard,
	 * as we had so many problems with eisa
	 * motherboards. So we just throw away
	 * this Board, knowing that any standard
	 * devices like serial, parallel, vga, ...
	 * are found by other means.
	 *
	 * Alternatively we could mark the Board as
	 * invisible (look back in s.boards.c)
	 * in case the motherboard has any extra
	 * resources that we didn't find.
	 */
	slot = 0; /* slot last processed */

	while (bp = ReadEisaNvram(slot)) {
		slot = bp->slot;

		/*
		 * Step thru each resource of this board
		 * looking for conflicts with other boards.
		 * Throw the whole Board away when there's a
		 * conflict. This handles the case where many
		 * standard devices are embedded on one board
		 * (eg 2 serial, 1 parallel). Note, separate
		 * code remove enabled floppy controllers from
		 * scsi boards.
		 */
		for (rp = resource_list(bp),
		    rc = resource_count(bp); rc--; rp++) {
			/*
			 * Check each resource of each function.
			 */
			if (rp->flags & RESF_ALT) {
				continue;
			}
			if (Query_resmgmt(Head_board,
			    (rp->flags & RESF_TYPE),
			    rp->base, rp->length) == 0) {
				continue;
			}
			free_board(bp);
			goto next;
		}
		add_board(bp); /* Tell rest of system about device */
next:		/* beats cstyle */;
	}
}

/*
 * Add the eisa function into the board.
 * Adding the space required to hold new resources
 * record may cause the board record to be relocated, so we return
 * the (possibly relocated) address of the board.
 */
Board *
AddFunction_eisa(Board *bp, EisaFuncCfgInfo *efp)
{
	EisaFuncEntryInfo flags = efp->sFuncEntryInfo;
	int k;

	debug(D_EISA, " Function info 0x%x\n", flags);

	if (flags.bFreeFormEntry) {
		return (bp);
	}

	if (flags.bMemEntry) {
		struct MemInfo *ip;
		u_long a, l;

		ip = ((EisaFuncResData *)&efp->sFFData)->asMemData;
		for (k = 0; k < MAX_Mem_ENTRIES; k++, ip++) {
			a = (u_long) (fetchESCD(ip->abMemStartAddr, u_long)
			    << 8L);
			l = (u_long) (fetchESCD(ip->abMemSize, u_short)
			    << 10L);

			if (!l) {
				l = 0x4000000;
			}
			debug(D_EISA, " mem base 0x%lx, len 0x%lx\n", a, l);
			bp = AddResource_devdb(bp, RESF_Mem, a, l);
			if (ip->bMemEntry == 0) {
				break;
			}
		}
	}

	if (flags.bIrqEntry) {
		struct IrqInfo *ip;
		unsigned int irq;

		ip = ((EisaFuncResData *)&efp->sFFData)->asIrqData;
		for (k = 0; k < MAX_Irq_ENTRIES; k++, ip++) {
			irq = (unsigned int) ip->bIrqNumber;
			debug(D_EISA, " irq %d\n", irq);
			bp = AddResource_devdb(bp, RESF_Irq, (long)irq, 1L);
			if (ip->bIrqEntry == 0) {
				break;
			}
		}
	}

	if (flags.bDmaEntry) {
		struct DmaInfo *ip;
		unsigned int dma;

		ip = ((EisaFuncResData *)&efp->sFFData)->asDmaData;
		for (k = 0; k < MAX_Dma_ENTRIES; k++, ip++) {
			dma = (unsigned int) ip->bDmaNumber;
			debug(D_EISA, " dma %d\n", dma);
			bp = AddResource_devdb(bp, RESF_Dma, (long)dma, 1L);
			if (ip->bDmaEntry == 0) {
				break;
			}
		}
	}

	if (flags.bPortEntry) {
		struct PortInfo *pp;
		u_int a, l;

		pp = ((EisaFuncResData *)&efp->sFFData)->asPortData;
		for (k = 0; k < MAX_Port_ENTRIES; k++, pp++) {
			a = ((unsigned int) (pp->abPortAddr[1] << 8)) |
			    pp->abPortAddr[0];
			l = pp->bPortCount + 1;

			/*
			 * Only add the io address if the range is
			 * in the ISA range.
			 *
			 * Note this includes all addresses below (0x1000)
			 * in order to handle the tr which uses addresses
			 * 0xa20-0xa23
			 */
			debug(D_EISA, " io base 0x%x, len 0x%x\n", a, l);
			if (a < 0x1000) {
				bp = AddResource_devdb(bp, RESF_Port,
				    (long)a, (long)l);
			}
			if (pp->bPortEntry == 0) {
				break;
			}
		}
	}
	return (bp);
}

/*
 * Fix up the iss device - which lacks an irq and shared memory
 * - iss code from Chris Johnson
 */

#define	EBS_EISA_ID 0xc80
#define	IMS_K2_PRODUCT_ID 8
#define	ISS_X_ID_REG 0x0c80 /* ID register address. */
#define	ISS_BRD_MASK 0x70 /* (K2_ADDED) ISS board mask */

Board *
iss_fixup(Board *bp)
{
	devtrans *dtp;
	static u_long mem = 0xfe010000;

	/*
	 * Check from the master file if this devid maps to an iss
	 */
	dtp = TranslateDevice_devdb(bp->devid, bp->bustype);
	if (!dtp || (strcmp(dtp->real_driver, "iss") != 0)) {
		return (bp);
	}

	/*
	 * IRQ: Slots 9 - 15 are hardwired to 23 - 17.
	 */
	bp = AddResource_devdb(bp, RESF_Irq, (long)(23 - (bp->slot - 9)), 1);

	/*
	 * Memory: The driver allocates from a base of 0xfe010000 upwards to
	 * each slot. In the case of the K2(), it's calculated based
	 * on a register setting.
	 */
	if (_inp(EBS_EISA_ID+2) >= IMS_K2_PRODUCT_ID) {
		u_int port, brd_num;

		port = (((u_int) bp->slot << 12) | ISS_X_ID_REG) + 4;
		brd_num = (_inp(port) & ISS_BRD_MASK) >> 4;
		mem = 0xfe010000 + (0x8000 * brd_num);
	}
	bp = AddResource_devdb(bp, RESF_Mem, mem, 0x8000);
	mem += 0x8000; /* increment for non-K2 next call */

	return (bp);
}
