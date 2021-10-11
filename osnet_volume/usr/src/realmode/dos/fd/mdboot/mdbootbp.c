/*
 * Copyright (c) 1999 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)mdbootbp.c 1.1	99/06/29 SMI\n"

/*
 * SOLARIS MDB LOADER:
 *
 * PURPOSE: loads the primary boot executive from the boot floppy diskette.
 *
 *  Resides on the first physical sector of the boot diskette, and
 *  extends to the first data cluster.
 *  First sector is loaded by INT 19h (ROM bootstrap loader) at address
 *  0000:7C00h
 *  
 *  For compatibility with the ROM BIOS, the first sector contains standard
 *  DOS boot signature bytes (55h, 0AAh at 1FEh, 1FFh). 
 *    
 *  In the event that our master boot record is inadvertently replaced by
 *  a standard DOS boot sector, the booting operation will still succeed!
 *  We simply lose the capability to boot from alternate devices, since
 *  standard DOS only supports "Drive A:/Drive C:" as bootable devices.
 *
 *  SYNOPSIS:
 *    begins execution at 0000:7C00h
 *    load extended code in first data cluster.
 *    load boot executive from the boot diskette.  The boot executive is
 *            assumed to exist in consecutive sectors, starting from the
 *            next available cluster (003) in the DOS filesystem.  It is
 *            also the first file in the root directory.
 *    verify boot record signature bytes
 *    jump to/execute the SOLARIS Boot Executive
 *
 *    error handler - can either reboot, or invoke INT 18h.
 *
 *    interface to mdexec:       BootDev in DL
 *===========================================================================
 * MDB Boot Loader: first physical sector on diskette
 *
 */

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

#include <sys/types.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include <bpb.h>              /* diskette BIOS Parameter Block */


/*
 * make all functions self-contained;
 * we only have 512 bytes until extension is loaded.
 */
extern BPB_T FD_BPB;
void load_bootexec ();
extern void fatal_err ();

extern short BootDev;
short tmpAX, tmpDX; 	/* XXX required by math.s??? */

extern char LoadErr1[];
extern short LoadErr1Siz;
char LoadErr2[] = "SOLARIS MDBexec load error.";
short LoadErr2Siz = sizeof ( LoadErr2 );
char SigErr[] = "Missing SOLARIS MDBexec.";
short SigErrSiz = sizeof ( SigErr );

char _far *solaris_mdb_exec;       /* bogus dcl to placate MSC 6.0 */

/*
 *  OK, I've just been loaded by INT 19h.  Now what?
 *  assumptions: dl contains the boot device number (in this case 0x0).
 */

solaris_mdboot ()
{
	register u_short rdnum, nrem;
	short strap_sector;
	u_short SPTrk, SPCyl; 
	u_short cyl, head, sector; 
	u_long pboot_addr = PBOOT_ADDR;

#if VDEBUG
	testpt ( "a" );
#endif
	_asm {

	;calculate # sectors allocated for root directory
	mov	ax, cs:FD_BPB.VRootDirEntries
	shr	ax, 4		; 16 directory slots per sector
	mov	di, ax

	;calc # sectors used for FAT entries
	mov	ax, cs:FD_BPB.VSectorsPerFAT
	xor	bh, bh
	mov	bl, BYTE PTR cs:FD_BPB.VNumberOfFATs
	mul	bx
	add	ax, di
	add	ax, cs:FD_BPB.VReservedSectors
	mov	si, ax		; si = sector number of first data cluster
	mov	bl, BYTE PTR cs:FD_BPB.VSectorsPerCluster
	add	ax, bx
	mov	WORD PTR strap_sector, ax	; secnum of strap.com

	mov	ax, si		; get sector number of mdboot back
	; add in offset to BPB, stamped in at mdboot-install time
	; by either mkfs_pcfs or fdformat
	mov	dx, cs:FD_BPB.VbpbOffsetHigh
	add	ax, cs:FD_BPB.VbpbOffsetLow	; dx:cy:ax = mdboot 2nd sect
	jnc	retry_read
	inc	dx	; add in upper carry if necessary
retry_read:
	; dx:ax is sector to load
	les	bx, pboot_addr
	mov 	cx, 1		; count
	call	read_disk
	jnc	success

	mov	di, offset LoadErr1
	mov	si, cs:LoadErr1Siz
	call	fatal_err
success:
	mov	di, cs:FD_BPB.VSectorsPerTrack
	mov	SPTrk, di
	}

	load_bootexec(strap_sector, SPTrk);

	_asm mov	es, word ptr solaris_mdb_exec+2


	(long)solaris_mdb_exec = MDEXEC_ADDR;

	_asm mov	dx, cs:BootDev

	_asm {
	jmp	RESUME
	/* use "nop" to produce one extra byte of padding in the object
	 * file or "add [bx+si], al to produce two bytes. the nop generates
	 * 0x90 in the binary whereas the "add..." generates 00 00 which
	 * is nice.
	 */
	add	[bx+si], al
	add	[bx+si], al
	add	[bx+si], al

	/* 55aa are magic cookies for a fdisk table */
	push	bp			/* produces 55 in dump */
	stosb				/* produces aa in dump */

RESUME:
	}
	((void (_far *)())solaris_mdb_exec)();

}



/*
 * Read the boot executive that must be located in the first slot of the
 * diskette root directory.  And just like the old MS-DOS system files,
 * the boot executive must reside in consecutive clusters.
 * The load address is fixed to avoid DMA problems.
 */
void
load_bootexec(u_short psector1, u_short SPTrk)
{
	u_short numsect;
	u_short readcnt;
	u_short sector;
	u_long psector;
	
	char _far *md_adr;

	*((short _far *) (MDEXEC_ADDR + 7)) = 1;

	psector = (u_long)FD_BPB.VbpbOffsetHigh << 16;
	psector += (u_long)FD_BPB.VbpbOffsetLow + (u_long)psector1;
	
	for (readcnt = 0, (u_long) md_adr = MDEXEC_ADDR; 
	    readcnt < *((short _far *) (MDEXEC_ADDR + 7));
	    readcnt += numsect, psector += numsect,
	    (u_long) md_adr += (ls_shift(numsect, 9))) {
		numsect = SPTrk - (psector % SPTrk);
		_asm {
			les	bx, md_adr
			mov	cx, numsect
			mov	dx, word ptr [psector+2]
			mov  	ax, word ptr [psector]
			call	read_disk
			jnc	secread_success

			mov 	di, offset LoadErr2
			mov 	si, LoadErr2Siz
			call 	fatal_err
secread_success:
		}
		/*
		 * the following is a small performance penalty to gain
		 * the compact read loop.
		 */
		if (readcnt == 0 && *(u_short _far *) (MDEXEC_ADDR + 3) != 'DM' &&
		    *(u_short _far *) (MDEXEC_ADDR + 5) != 'XB') {
			_asm {
			mov	di, offset SigErr
			mov	si, SigErrSiz
			call	fatal_err
			}
		}
	}
}


		

		

