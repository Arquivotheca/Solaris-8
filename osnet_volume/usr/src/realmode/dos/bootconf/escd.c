/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * escd.c -- routines to access the on disk machine configuration
 *
 * The format of this file used to be escd, but is now internal (canonical)
 * format, and all that remains is the name (which was published).
 */

#ident "@(#)escd.c   1.65   97/08/12 SMI"

#include "types.h"
#include <sys/stat.h>
#include <biosmap.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "eisa.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "cfname.h"
#include "gettext.h"
#include "main.h"
#include "menu.h"
#include "pci.h"
#include "probe.h"
#include "resmgmt.h"
#include "tty.h"
#include "version.h"

/*
 * Function prototypes
 */
Board *AddFunction_escd(Board *bp, EisaFuncCfgInfo *efp);
unsigned long ExtMemSize();
struct ESCD_BrdHdr *CvtBoardRecord(Board *bp);
Board *read_cfg_escd(int file);

/*
 * Globals
 */
char Update_escd = 0;
char *Escd_name = 0;
static char minor_version;

#define	ptr(t) ((struct t##Info *)ip) /* ESCD type "t" resource record ptr */

static int reread_escd = 1;

/*
 * Get extended memory size:
 *
 * This routine reads the extended memory size of the host machine
 * as recorded in CMOS and returns it to the caller. This value
 * may not be completely accurate, as some systems won't report
 * stuff above 16MB, but it hardly matters given that we won't
 * be reserving anything above 1MB anyway! Still, it's nice to
 * be able display a reasonable value when the user looks for it.
 */
unsigned long
ExtMemSize()
{
	unsigned long len = 0;
#ifndef	__lint
	_asm {
		mov   dx, 70h;	    Read low byte of mem size from CMOS port
		mov   al, 17h;	    .. and save it in %cl
		out   dx, al
		mov   dx, 71h
		in    al, dx
		mov   cl, al
		mov   dx, 70h;	    Now read high byte of mem size and put
		mov   al, 18h;	    .. it in %ch
		out   dx, al
		mov   dx, 71h
		in    al, dx
		mov   ch, al
		mov   word ptr [len], cx
	}
#endif
	return (len << 10);
}

/*
 * Add a function to a board record:
 *
 * This routine will add the (packed) EISA function description at
 * "efp" to the board record at "bp", converting to canonical form
 * as it goes. Adding the space required to hold the new resources
 * record may cause the board record to be relocated.
 */

Board *
AddFunction_escd(Board *bp, EisaFuncCfgInfo *efp)
{
	EisaFuncEntryInfo flags;
	unsigned char *ip = (unsigned char *)efp;
	int mor;
	u_short type;
	unsigned long a, l;

	ip += ip[sizeof (short)] + sizeof (short) + 1;
	*(char *)&flags = *ip;

	if (flags.bFuncDisabled)
		bp->flags |= (BRDF_DISAB|BRDF_NOTREE);

	if (flags.bFreeFormEntry) {
		return (bp);
	}

	ip++;
	if (flags.bMemEntry) {
		do {
			type = RESF_Mem;
			if (ptr(Mem)->bMemShared)
				type |= RESF_SHARE;
			if (ptr(Mem)->bMemUsurp)
				type |= RESF_USURP;
			a = fetchESCD(ptr(Mem)->abMemStartAddr,
			    unsigned long) << 8;
			l = (unsigned long)
			    fetchESCD(ptr(Mem)->abMemSize,
			    unsigned short) << 10L;
			if (l == 0)
				l = 0x4000000;
			bp = AddResource_devdb(bp, type, a, l);
			mor = ptr(Mem)->bMemEntry;
			ip += sizeof (struct MemInfo);
		} while (mor);
	}

	if (flags.bIrqEntry) {
		do {
			type = RESF_Irq;
			if (ptr(Irq)->bIrqShared)
				type |= RESF_SHARE;
			if (ptr(Irq)->bIrqUsurp)
				type |= RESF_USURP;
			bp = AddResource_devdb(bp, type,
			    ptr(Irq)->bIrqNumber, 1);
			mor = ptr(Irq)->bIrqEntry;
			ip += sizeof (struct IrqInfo);
		} while (mor);
	}

	if (flags.bDmaEntry) {
		do {
			type = RESF_Dma	+
			    (ptr(Dma)->bDmaShared ? RESF_SHARE : 0);
			bp = AddResource_devdb(bp, type,
			    ptr(Dma)->bDmaNumber, 1);
			mor = ptr(Dma)->bDmaEntry;
			ip += sizeof (struct DmaInfo);
		} while (mor);
	}

	if (flags.bPortEntry) {
		do {
			type = RESF_Port;
			if (ptr(Port)->bPortShared)
				type |= RESF_SHARE;
			if (ptr(Port)->bPortUsurp)
				type |= RESF_USURP;
			mor = ptr(Port)->bPortEntry;
			a = fetchESCD(ptr(Port)->abPortAddr, unsigned short);
			/*
			 * Pick up the port count from the correct place
			 * dependent on the escd version. The rest of the
			 * fields are common between the 2 structs.
			 */
			if (minor_version < 1) {
				l = ptr(Port)->bPortCount + 1;
				ip += sizeof (struct PortInfo);
			} else {
				l = fetchESCD(ptr(Port1)->abPortCount, u_short);
				ip += sizeof (struct Port1Info);
			}
			bp = AddResource_devdb(bp, type, a, l);
		} while (mor);
	}
	return (bp);
}

/*
 * Convert canonical board record to ESCD format:
 *
 * This routine converts the canonical board record at "*bp" to
 * ESCD format suitable for writing to the ISA configuration file: escd.rf.
 */
struct ESCD_BrdHdr *
CvtBoardRecord(Board *bp)
{
	int j;
	unsigned len;
	char *ip, *xp;
	short *lp;

	int another;
	Resource *rp;
	EisaFuncEntryInfo *ffp;
	struct ESCD_BrdHdr *bhp;
#define	efp ((EisaFuncCfgInfo *)(bhp+1))

	len = sizeof (struct ESCD_BrdHdr)
	    + (unsigned)((EisaFuncCfgInfo *)0)->abSelections
	    + (2 * sizeof (short))
	    + (sizeof (int)
	    + (3 * sizeof (unsigned char))
	    + (bp->rescnt[RESC_Mem] * sizeof (struct MemInfo))
	    + (bp->rescnt[RESC_Irq] * sizeof (struct IrqInfo))
	    + (bp->rescnt[RESC_Dma] * sizeof (struct DmaInfo))
	    + (bp->rescnt[RESC_Port] * sizeof (struct Port1Info)));

	if (!(bhp = (struct ESCD_BrdHdr *)calloc(1, len)))
		MemFailure();

	bhp->bSlotNum = bp->slot;
	efp->bCFGMajorRevNum = ESCD_MAJOR_VER;
	efp->bCFGMinorRevNum = ESCD_MINOR_VER;
	storeESCD(efp, unsigned long, bp->devid);
	storeESCD(&efp->sIDSlotInfo, unsigned short, bp->slotflags);
	ip = (char *)efp->abSelections;
	lp = (short *)ip;

	/*
	 * Build function header: A length word, a single selection
	 * byte, and the FuncEntryInfo byte.
	 */
	ip = (char *)(lp+1);
	*ip++ = 1;
	*ip++ = 0;
	ffp = (EisaFuncEntryInfo *)ip;
	*ip++ = 0;
	/*
	 * If board is disabled, mark record as such
	 */
	if (bp->flags & BRDF_DISAB)
		ffp->bFuncDisabled = 1;

	/*
	 * Copy resource usage information
	 *
	 * This converts usage information of type from the canonical
	 * form to the highly compressed EISA format.
	 */
	for (another = 0, rp = resource_list(bp), j = resource_count(bp);
	    j--; rp++) {
		if ((rp->flags & RESF_TYPE) != RESF_Mem)
			continue;
		if (rp->flags & RESF_SHARE)
			ptr(Mem)->bMemShared = 1;
		if (rp->flags & RESF_USURP)
			ptr(Mem)->bMemUsurp = 1;
		if (another++)
			ptr(Mem)[-1].bMemEntry = 1;
		storeESCD(ptr(Mem)->abMemStartAddr, long, rp->base >> 8);
		storeESCD(ptr(Mem)->abMemSize, short, rp->length >> 10);
		storeESCD(ip, unsigned short,
		    (fetchESCD(ip, unsigned short)));
		ip += sizeof (struct MemInfo);
		ffp->bMemEntry = 1;
	}
	for (another = 0, rp = resource_list(bp), j = resource_count(bp);
	    j--; rp++) {
		if ((rp->flags & RESF_TYPE) != RESF_Irq)
			continue;
		ptr(Irq)->bIrqNumber = rp->base;
		if (rp->flags & RESF_SHARE)
			ptr(Irq)->bIrqShared = 1;
		if (rp->flags & RESF_USURP)
			ptr(Irq)->bIrqUsurp = 1;
		if (another++)
			ptr(Irq)[-1].bIrqEntry = 1;
		storeESCD(ip, unsigned short,
		    (fetchESCD(ip, unsigned short)));
		ip += sizeof (struct IrqInfo);
		ffp->bIrqEntry = 1;
	}
	for (another = 0, rp = resource_list(bp), j = resource_count(bp);
	    j--; rp++) {
		if ((rp->flags & RESF_TYPE) != RESF_Dma)
			continue;
		ptr(Dma)->bDmaNumber = rp->base;
		if (rp->flags & RESF_SHARE)
			ptr(Dma)->bDmaShared = 1;
		if (another++)
			ptr(Dma)[-1].bDmaEntry = 1;
		storeESCD(ip, unsigned char,
		    (fetchESCD(ip, unsigned char)));
		ip += sizeof (struct DmaInfo);
		ffp->bDmaEntry = 1;
	}
	for (another = 0, rp = resource_list(bp), j = resource_count(bp);
	    j--; rp++) {
		if ((rp->flags & RESF_TYPE) != RESF_Port)
			continue;
		/*
		 * we always write a minor version 1 port entry
		 * that allows port entries > 32 length.
		 */
		if (rp->flags & RESF_SHARE) {
			ptr(Port1)->bPortShared = 1;
		}
		if (rp->flags & RESF_USURP) {
			ptr(Port1)->bPortUsurp = 1;
		}
		if (another++)
			ptr(Port1)[-1].bPortEntry = 1;
		storeESCD(ptr(Port1)->abPortAddr, short, rp->base);
		storeESCD(ptr(Port1)->abPortCount, short, rp->length);
		storeESCD(ip, unsigned char, (fetchESCD(ip, char)));
		ip += sizeof (struct Port1Info);
		ffp->bPortEntry = 1;
	}

	/*
	 * store the length in the header word at "lp" ...
	 */
	*lp = (ip - (char *)lp) - sizeof (short);


	storeESCD(ip, short, 0);
	ip += sizeof (short);

	/* Store checksum and compute rec. len */
	for (j = 0, xp = (char *)efp; xp < ip; j += *xp++) {
		;
	}
	storeESCD(ip, short, -j);
	ip += sizeof (short);
	storeESCD(&bhp->wBrdRecSize, short, ip - (char *)bhp);

	return (bhp);
#undef	efp
}

/*
 * Get base memory size:
 *
 * Intel's PnP code assumes that all PC class machines have 640K
 * of realmode RAM available. We actually probe the BIOS work area
 * to determine the avalable realmode memory (some older 386[SD]X
 * machines have only 512K).
 */
#define	BasMemSize() ((unsigned long)bdap->RealMemSize << 10)

/*
 * Read ESCD Configuration Info:
 *
 * This routine reads configuration information from an open
 * "file". The file contains canonical board records which are re-
 * turned to the caller one at a time. It returns a null pointer
 * upon reaching end-of-file.
 *
 * The board records produced by this routine are dynamically
 * allocated and must be freed by the caller. We return a
 * dummy board record address (-1) if the file is corrupted.
 */

Board *
read_cfg_escd(int file)
{
	Board *bp = 0;
	static struct ESCD_CfgHdr chd;

	if (reread_escd) {
		if ((_read(file, &chd, sizeof (chd)) != sizeof (chd)) ||
		    (chd.dSignature != ESCD_SIGNATURE)) {
			/*
			 * We were unable to read the configuration header,
			 * or corrupt file.
			 */
			return ((Board *) -1);
		}
	}
	/*
	 * save version of read escd into module global
	 */
	minor_version = chd.bVerMinor;

	reread_escd = 0;

	while (!bp && chd.bBrdCnt--) {
		/*
		 * There's at least one board record left to be read.
		 * Allocate the neccessary buffers and read it in.
		 */

		char *ep, *ip;
		EisaFuncCfgInfo *efp;
		struct ESCD_BrdHdr bhd;
		int nn;

		if ((nn = _read(file, (void *)&bhd,
		    sizeof (bhd))) != sizeof (bhd)) {
			iprintf_tty("1 ReadEscdFile failed %d (%d) fd %d\n",
			    nn, sizeof (bhd), file);
			(void) getc_tty();
			break;
		}

		if (!(ip = (char *)malloc(bhd.wBrdRecSize -= sizeof (bhd))))
			MemFailure();
		if ((nn = _read(file, (void *)ip,
		    bhd.wBrdRecSize)) != bhd.wBrdRecSize) {
			printf("2 ReadEscdFile failed %d (%d)\n",
			    nn, bhd.wBrdRecSize);
			free(ip);
			break;
		}
		ep = ip + bhd.wBrdRecSize - (SZ_CHKSUM_FIELD+SZ_LASTFUNC);
		ip = (char *)(efp = (EisaFuncCfgInfo *)ip)->abSelections;

		bp = new_board();
		bp->slot = bhd.bSlotNum;
		bp->bustype = RES_BUS_ISA;
		bp->devid = fetchESCD(efp->abCompBoardId, unsigned long);
		bp->slotflags = fetchESCD(&efp->sIDSlotInfo, unsigned short);

		if (ip < ep) {
			/*
			 * Use AddFunction_escd to convert to canonical form
			 */
			bp = AddFunction_escd(bp, (EisaFuncCfgInfo *)ip);
			ip += (fetchESCD(ip, short) + sizeof (short));
		}

		free(efp);
	}

	return (bp);
}

/*
 * Table of fixed resources used by the motherboard
 */
static struct {
	u_short type;
	long base;
	long len;
} mb[] = {
	{ RESF_Mem, 0, 0}, /* 640KB (or 512KB) */
	{ RESF_Mem, 0xF0000, 0x10000 }, /* BIOS */
	{ RESF_Mem, MAX_REAL_MEM, 0 }, /* Mem above magic 1MB */
	{ RESF_Irq, 0, 1 }, /* NMI */
	{ RESF_Irq, 2, 1 }, /* Cascaded IRQ from 2nd PIC */
	{ RESF_Irq, 8, 1 }, /* Clock */
	{ RESF_Irq, 13, 1 }, /* FPU */
	{ RESF_Port, 0, 0x60 }, /* All ports < 0x100 missing the keyboard */
	{ RESF_Port, 0x61, 3 }, /* All ports < 0x100 missing the keyboard */
	{ RESF_Port, 0x65, 0x9b }, /* All ports < 0x100 missing the keyboard */
	{ RESF_Dma, 4, 1 }, /* cascaded dma */
};

#define	MbCnt (sizeof (mb)/sizeof (mb[0]))

/*
 * Create initial motherboard entry.
 */
void
MotherBoard()
{
	int j = 0;
	Board *bp;

	bp = new_board();
	bp->bustype = Main_bus;
	bp->devid = CompressName("SUNFFE2");

	mb[0].len = BasMemSize();
	mb[2].len = ExtMemSize();

	/*
	 * Set pre-assigned motherboard resources
	 */
	for (j = 0; j < MbCnt; j++) {
		bp = AddResource_devdb(bp, mb[j].type, mb[j].base, mb[j].len);
	}

	add_board(bp); /* Tell rest of system about device */
}

/*
 * Writes the modified ESCD back to disk
 */
void
write_escd(void)
{
	/*
	 * There's nothing to do unless the ESCD has been modified
	 * since we read it in. If this is the case, the "Update_
	 * escd" flag will be set.
	 */

	if (Update_escd) {
		Board *bp;
		int write_error = 0;
		int j;
		struct ESCD_CfgHdr hdr;
		struct ESCD_BrdHdr *list[MAX_SLOTS];
		FILE *fp = 0;

		memset(&hdr, 0, sizeof (hdr));
		hdr.dSignature = ESCD_SIGNATURE;
		hdr.bVerMajor = ESCD_MAJOR_VER;
		hdr.bVerMinor = ESCD_MINOR_VER;
		hdr.wEscdSize = sizeof (struct ESCD_CfgHdr);

		for (bp = Head_board; bp; bp = bp->link) {
			/*
			 * skip all but the user probed and manually added
			 * devices - all others are automatically enumerated
			 * or automatically probed (probe always)
			 */
			if (!(bp->flags & BRDF_DISK)) {
				continue;
			}
			ASSERT(hdr.bBrdCnt < MAX_SLOTS);

			j = hdr.bBrdCnt++;
			list[j] = CvtBoardRecord(bp);
			hdr.wEscdSize += list[j]->wBrdRecSize;
		}

retry:
		if (((fp = fopen(Escd_name, "wb")) == 0) ||
			    (fwrite(&hdr, sizeof (hdr), 1, fp) != 1)) {
			/*
			 * We were unable to open (or create) the ESCD file,
			 * or something went wrong writing the header record.
			 * Set error msg code and bypass further writing.
			 */

			write_error = 1;

		} else for (j = 0; (unsigned)j < hdr.bBrdCnt; j++) {
			/*
			 * Now write the ESCD board records to disk.
			 */

			struct ESCD_BrdHdr *bhp = list[j];
			unsigned len = bhp->wBrdRecSize;

			if (fwrite(bhp, len, 1, fp) != 1) {
				/*
				 * Error writing the board record.
				 * Set return code and give up.
				 */
				write_error = 1;
				break;
			}
		}

		if (fp != 0) {
			if (fclose(fp)) {
				write_error = 1;
			}
		}
		if (write_error) {
			write_err(Escd_name);
			write_error = 0;
			goto retry;
		}
		for (j = 0; j < hdr.bBrdCnt; j++)
			free(list[j]);

		Update_escd = 0;
	}

	/*
	 * Bug 1255301 - if we are using a named configuration
	 * then copy that file back to escd.rf so that the
	 * configuration gets properly saved in the installation.
	 */
	if (strcmp(Escd_name, "escd.rf") != 0) {
		(void) copy_file_cfname("escd.rf", Escd_name);
	}
}

/*
 * Read machines configuration
 *
 * This routine reads the target machine's configuration from the open file
 * desriptor, "fd". If "fd" is less than 0, we assume that the
 * target and host are the same machine and initialize the configuration
 * to contain a single motherboard record.
 */
void
read_escd(void)
{
	int fd, drop_board;
	Board *bp, *nbp, *escd_head = NULL;
	struct _stat stbuf;

	if (((fd = _open(Escd_name, _O_RDONLY|_O_BINARY)) < 0) &&
	    (errno != ENOENT)) {
		fatal("can't open ESCD file \"%s\": %!", Escd_name);
	} else if (fd >= 0) {
		/* a zero-length escd is just as good as no escd */
		if (_fstat(fd, &stbuf) < 0) {
			fatal("stat on \"%s\": %!", Escd_name);
		}
		if (stbuf.st_size == 0) {
			_close(fd);
			fd = -1;
		}
	}

	if (fd >= 0) {
		if (!Autoboot) {
			status_menu(Please_wait, "MENU_LOADING", Escd_name);
		}
		reread_escd = 1;

		while (bp = read_cfg_escd(fd)) {
			if (bp == (Board *) -1) {
				/*
				 * The ESCD file appears to be corrupted.
				 * Ignore it and create one from scratch
				 * (after first giving the user the bad news).
				 */
				enter_menu(0, "MENU_CORRUPT_ESCD", Escd_name);

				free_chain_boards(&escd_head);

				/*
				 * Mark the escd as modified so that at
				 * least the header gets written out.
				 * This is because delayed truncates
				 * and unlinks don't work (yet).
				 */
				Update_escd = 1;

				Autoboot = 0;
				_close(fd);
				return;
			}

			/*
			 * Check for conflicts with current devices
			 * Most of this is needed to handle old
			 * format escd.rf files that contained
			 * all the Head_board.
			 */
			drop_board = 0;
			if (board_conflict_resmgmt(bp, 0, 0)) {
				drop_board = 1;
			} else {
				for (nbp = Head_board; nbp; nbp = nbp->link) {
					if (equal_boards(bp, nbp)) {
						drop_board = 1;
						break;
					}
				}
			}
			if (drop_board) {
				free(bp);
				Update_escd = 1;
				continue;
			}

			bp->flags |= BRDF_DISK;
			bp->buflen = bp->reclen;
			bp->link = escd_head;
			escd_head = bp;
		}

		/*
		 * Now we know the escd is not corrupt
		 * add all the records to the Head_board
		 */
		for (bp = escd_head; bp; bp = nbp) {
			nbp = bp->link;
			add_board(bp); /* Tell rest of system about device */
		}
		_close(fd);
	}
}
