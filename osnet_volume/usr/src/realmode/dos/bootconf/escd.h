/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  escd.h -- public definitions for escd routines
 *
 *	The following structure definitions and typedefs map the
 *	Plug-and-Play Extended System Configuration Data (ESCD).  Most of
 *	this stuff was taken from the Intel Plug-and-Play devlopers kit:
 *
 *            @(#) .../1.43/cm/dll/escddef.h
 *            @(#) .../1.43/cm/dll/cminfo.h
 *
 *	These data structures make extensive use of C bit-fields, which are
 *	byte-order sensitive.  The ESCD itself is a little-endian data
 *	structure and this file provides a little-endian storage map of that
 *	structure.  If you're porting the ESCD maintenance utilities to a
 *	big-endian machine, you will have to provide a new version of this
 *	file!
 *
 *	This file also assumes that word and long-word access can be performed
 *	on unaligned memory locations.  If you're porting to a machine where
 *	this is not the case, you will have to provide appropriate variants of
 *	the "fetchESCD" and "storeESCD" macros (see below).
 *
 *       *******************************************************************
 *       *    Copyright 1993 Intel Corporation ALL RIGHTS RESERVED         *
 *       *                                                                 *
 *       * This program is confidential and a trade secret of Intel Corp.  *
 *       * The receipt of or possession of this program does not convey    *
 *       * any rights to reproduce or disclose its contents or to          *
 *       * manufacture, use, sell anything that it may describe, in        *
 *       * whole, or in part, without the specific written consent of      *
 *       * Intel Corp.                                                     *
 *       *******************************************************************
 *
 *
 *
 *	PURPOSE
 *	  Defines the structures needed to access the ESCD system config
 *	  info that was first introduced with EISA system support.
 *
 *	NOTES
 *
 *	  1.  The definitions here reflect the packed Eisa Format.
 *
 *	  2.  Virtual devices do not have slot-specific I/O space or a readable
 *	      ID.  Any peripheral, device or software that needs a configuration
 *	      file and is not covered by the other device types can be specified
 *	      as a virtual device.  Virtual devices are assigned numbers from
 *	      16 to 64.  Eisa slots 16-64 are referred to as Virtual Slots.
 *
 *	  3.  Configuration information for PCI devices in a EISA system is
 *	      stored in Virtual slots.
 *
 *	  4.  (E)ISA and PCI devices have device specific information that
 *	      cannot be represented by the EISA structures completly.  Such
 *	      information is stored in EISA FreeFormat as a disabled EISA
 *	      function.
 *
 *	  5.  Structure and field names are Intel's idea, not mine -- Reg.
 *
 *	HISTORY
 *	   Mohan - $Revision: 1.3 $
 *	   Anthony - $Revision: 1.3 $
 *	   Reg (SMI) - Force it thru cstyle
 */

#ifndef	_ESCD_H
#define	_ESCD_H

#ident "@(#)escd.h   1.30   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <dostypes.h>

/*
 *  First, some manifest constants that Intel carries in "cminfo.h":
 *
 *  NOTE: MAX_xxx_ENTRIES represent the maximum number of resources of the
 *        indicated type that may be made for any given function.  The num-
 *        ber of possible resource assignments may be greater!
 */

#define	MAX_FF_INFO	 204	/* Max length of free-form data field	    */
#define	MAX_Mem_ENTRIES	 9	/* Max number of memory resources/fctn	    */
#define	MAX_Port_ENTRIES 20	/* Max I/O ports per fctn.		    */
#define	MAX_Irq_ENTRIES	 7	/* Max IRQ assignments per fctn.	    */
#define	MAX_Dma_ENTRIES	 4	/* Max DMA channels per fctn.		    */

#define	SZ_LASTFUNC	 sizeof (short)	/* Size of last function marker	    */
#define	SZ_CHKSUM_FIELD	 sizeof (short)	/* Size of the checksum field	    */
#define	MAX_PNP_NVRAM	 8192		/* Max size of PnP NVRAM	    */
#define	MAX_EISA_NVRAM	 4096		/* EISA NVRAM is only half that big */
#define	MAX_EISA_SLOTS	 16		/* Maximum number of (E)ISA devices */
#define	DCD_FUNCTION	 0xC0		/* Function entry code for DCDs	    */
#define	ESCD_SIGNATURE	 0x47464341	/* "ACFG"			    */
#define	MAX_MEM_ADDR	 0x3FFFFFF	/* Can't reserve more than 64MB!    */
#define	MAX_PORT_ADDR	 0xFFFF		/* Only 16 bits of port address	    */
#define	MAX_REAL_MEM	 0x00100000	/* Realmode memory stops here!	    */
#define	REAL_MEM_HOLE	 0x000A0000	/* Start of real memory "hole"	    */


/*
 * escd.rf version constants
 */
#define	ESCD_MAJOR_VER	 1
#define	ESCD_MINOR_VER0	 0		/* Original version */
#define	ESCD_MINOR_VER	 1		/* Support for port lengths > 32 */

/*		    *** EISA STRUCTURE DEFINITIONS  ***			    */


#pragma pack(1)		/* Data is not aligned!				    */
#define	BIT BYTE

struct FreeFormData {
	/*
	 *  EISA free-form data isn't completely free form:  The first byte
	 *  give the actual length of the "abData" array.  MAX_FF_INFO is
	 *  absolute maximum.
	 */

	BYTE bDataSize;			/* Length of following data block   */
	BYTE abData[MAX_FF_INFO];	/* Max total length is 204 bytes    */
};

typedef struct {
	/*
	 *  Resource specifiers:  If the bit is set, the corresponding
	 *  <resource>Info structs can be found in the free-form data area.
	 */

	BIT bTypSubTypEntry	:1;	/* Bit 0 - Type/subtype present	flg */
	BIT bMemEntry		:1;	/* Bit 1 - Mem entry data present   */
	BIT bIrqEntry		:1;	/* Bit 2 - IRQ data present	    */
	BIT bDmaEntry		:1;	/* Bit 3 - DMA entry data present   */
	BIT bPortEntry		:1;	/* Bit 4 - Port range data prsent   */
	BIT bPortInitEntry	:1;	/* Bit 5 - Port init data present   */
	BIT bFreeFormEntry	:1;	/* Bit 6 - Free form data present   */
	BIT bFuncDisabled	:1;	/* Bit 7 - Board enabled if zero    */

} EisaFuncEntryInfo;

struct MemInfo {
	/*
	 *  Memory resource descriptor:
	 */

	BIT bMemRdWr		:1;	/* Bit 0 .. 0 = ROM, 1 = RAM	    */
	BIT bMemCached		:1;	/* Bit 1 .. 0 = not cached	    */
	BIT bMemUsurp		:1;	/* Bit 2 .. 1 = mem usurps weak mem */
	BIT bMemType		:2;	/* Bit 3-4. 0=sys,1=exp,2=vir,3=oth */
	BIT bMemShared		:1;	/* Bit 5 .. 0 = not shared	    */
	BIT bRsvrd2		:1;	/* Bit 6 .. (reserved)		    */
	BIT bMemEntry		:1;	/* Bit 7 .. 0 = last entry	    */

	/* Byte 1 ...							    */
	BIT bMemDataSize	:2;	/* Bit 0-1. 0=byte,1=word,2=dword   */
	BIT bMemDecodeSize	:2;	/* Bit 2-3. 0=20,1=24,2=32	    */
	BIT bRsvrd3		:4;	/* Bit 4-7. (reserved)		    */

	/* Bytes 2-6 ...						    */
	BYTE abMemStartAddr	[3];	/* Memory start address/265.	    */
	BYTE abMemSize		[2];	/* Memory size/1024, (0 = 64MB)	    */
};

struct IrqInfo {
	/*
	 *  IRQ resource descriptor:
	 */

	BIT bIrqNumber		:4;	/* Bit 0-3. IRQ Number		    */
	BIT bIrqUsurp		:1;	/* Bit 4 .. 1 = irq usurps weak irq */
	BIT bIrqTrigger		:1;	/* Bit 5 .. 0=Edge,1=Level	    */
	BIT bIrqShared		:1;	/* Bit 6 .. 0=Non-shared,1=Sharable */
	BIT bIrqEntry		:1;	/* Bit 7 .. 0=Last Entry	    */

	/* Byte 1 ...							    */
	BIT bRsvrd		:8;	/* Bit 8-15 (reserved)		    */
};

struct DmaInfo {
	/*
	 *  DMA resource descriptor:
	 */

	BIT bDmaNumber		:3;	/* Bit 0-2. DMA Number(0-7)	    */
	BIT bRsrvd1		:3;	/* Bit 3-5  (reserved)		    */
	BIT bDmaShared		:1;	/* Bit 6 .. 0=Non-Shared,1=Sharable */
	BIT bDmaEntry		:1;	/* Bit 7 .. 0=Last Entry	    */

	/* Byte 1 ...							    */
	BIT bRsvrd2		:2;	/* Bit 0-1. (reserved)		    */
	BIT bDmaTransferSiz	:2;	/* Bit 2-3. 0=8bit,1=16bit,2=32bit  */
	BIT bDmaTiming		:2;	/* Bit 4-5. 0=ISA,1="A",2="B",3="C" */
	BIT bRsvrd3		:2;	/* Bit 6-7  (reserved)		    */
};

struct PortInfo {
	/*
	 * I/O port resource descriptor:
	 * Header minor verion 1.
	 */

	BIT bPortCount		:5;	/* Bit 0-4. Number of ports less 1  */
	BIT bPortUsurp		:1;	/* Bit 5 .. 1 = port usurps weak port */
	BIT bPortShared		:1;	/* Bit 6 .. 0=Non-shared,1=Sharable */
	BIT bPortEntry		:1;	/* Bit 7 .. 0=Last Entry	    */

	/* Bytes 1-2 ...						    */
	BYTE abPortAddr		[2];    /* Port address			    */
};

struct Port1Info {
	/*
	 * Devconf port entry allowing greater than 32 ports for a length.
	 * Header minor verion 1. Same field positions as PortInfo except
	 * for abPortCount.
	 */

	BIT bUnused		:5;	/* Bit 0-4. Unused		    */
	BIT bPortUsurp		:1;	/* Bit 5 .. 1 = port usurps weak port */
	BIT bPortShared		:1;	/* Bit 6 .. 0=Non-shared,1=Sharable */
	BIT bPortEntry		:1;	/* Bit 7 .. 0=Last Entry	    */

	/* Bytes 1-2 ...						    */
	BYTE abPortAddr		[2];    /* Port address			    */
	/* Bytes 3-4 ...						    */
	BYTE abPortCount	[2];	/* Port count (exact count)	    */
};

struct InitData {
	/*
	 *  Port initialization record:
	 *
	 *  These records are variable length; This is only the header
	 *  portion!
	 */

	BIT bAccessType		:2;	/* Bit 0-1. 0=Byte,1=word,2=dword   */
	BIT bPortMaskSet	:1;	/* Bit 2 .. 0=no mask,1=use mask    */
	BIT bRsvrd1		:4;	/* Bit 3-6  (reserved)		    */
	BIT bEntry		:1;	/* Bit 7 .. 0=Last Entry	    */

	/* Bytes 1-2 ...						    */
	BYTE abPortAddr		[2];	/* Port address			    */
};

typedef struct {
	/*
	 *  Slot descriptor record:
	 *
	 *  There's only one of these per board, although the "EisaFuncCfgInfo"
	 *  record layout below might lead one to suspect otherwise!
	 */

	BIT bDupCFGNumId	:4;	/* Bit 0-3. Duplicate CFG file #s   */
	BIT bSlotType		:2;	/* Bit 4-5. 0=exp,1=emb,2=vir	    */
	BIT bIDNotReadable	:1;	/* Bit 6 .. 1=ID not readable	    */
	BIT bDupIDPresent	:1;	/* Bit 7 .. 1=Duplicate ID present  */

	/* Byte 1 ...							    */
	BIT bBrdEnableSup	:1;	/* Bit 0 .. 1=Board is enablable    */
	BIT bBrdIOChkErr	:1;	/* Bit 1 .. 1=IOCHKERR supported    */
	BIT bBrdOrEntryLck	:1;	/* Bit 2 .. 1=Board locked	    */
	BIT bIdRsrvd1		:2;	/* Bit 3-4. (reserved)		    */
	BIT bEisaDevice		:1;	/* Bit 5 .. 1=EISA device in slot   */
	BIT bNoCfgFile		:1;	/* Bit 6 .. 1=No CFG file	    */
	BIT bBrdConfgStat	:1;	/* Bit 7 .. 1=Cfg error (conflict)  */

} EisaIDSlotInfo;

typedef struct {
	/*
	 *  Board record:
	 *
	 *  This is the "unpacked" format as returned by an EISA BIOS
	 *  (Intel's NOTE 1 lies -- or is at least misleading).
	 */

	BYTE abCompBoardId	[4];	  /* Board ID, EISA compressed form */
	EisaIDSlotInfo sIDSlotInfo;	  /* Resource presence flags	    */
	BYTE bCFGMinorRevNum;		  /* Minor revision of CFG file	    */
	BYTE bCFGMajorRevNum;		  /* major revision of CFG file	    */
	BYTE abSelections[26];		  /* Choice selection information   */
	EisaFuncEntryInfo sFuncEntryInfo; /* Slot flags			    */
	BYTE abTypeSubType[80];		  /* Function type/subtype (ASCII)  */
	struct FreeFormData sFFData;	  /* Free format data (see below)   */

} EisaFuncCfgInfo;

typedef struct {
	/*
	 *  Fixed-format resource data:
	 *
	 *  This is the "unpacked" form for standard (non-free format)
	 *  board records.  It overlays the "sFFData" field of the
	 *  EisaFuncCfgInfo record, above.
	 */

	struct MemInfo asMemData[MAX_Mem_ENTRIES];	/* Memory resources */
	struct IrqInfo asIrqData[MAX_Irq_ENTRIES];	/* IRQ resources    */
	struct DmaInfo asDmaData[MAX_Dma_ENTRIES];	/* DMA resources    */
	struct PortInfo asPortData[MAX_Port_ENTRIES];	/* Port resources   */
	BYTE abInitData[60];				/* Port init'ers    */

} EisaFuncResData;

/*		    *** ESCD STRUCTURE DEFINITIONS  ***			    */

struct FreeFormBrdHdr {
	/*
	 *  Free form board header:
	 *
	 *  Dynamically configured devices (DCDs) are encoded as EISA free-
	 *  format functions, although their format is fixed by this data
	 *  structure!
	 */

	DWORD dSignature;	/* Initialize to "ACFG"			    */
	BYTE  bVerMinor;	/* should be >= 0			    */
	BYTE  bVerMajor;	/* should be 2				    */
	BYTE  bBrdType;		/* Board Type				    */
	BYTE  bRsrvd1;		/* Reserved				    */
	WORD  fwFuncsDisabled;	/* Function enabled map (for max 16 fctns)  */
	WORD  fwFuncsCfgError;	/* Config Error bitmap			    */
	WORD  fwCantConfig;	/* Non-configurable function bitmap	    */
	BYTE  abRsvrd2[2];	/* Reserved				    */
};

struct ESCD_CfgHdr {
	/*
	 *  File header:
	 *
	 *  Appears at the front of the ESCD file.  Also stored in NVRAM on
	 *  machines with PnP BIOSes, but is not present in EISA systems.
	 */

	WORD  wEscdSize;	/* Total Size of File/NVRAM		    */
	DWORD dSignature;	/* Initialize to "ACFG"			    */
	BYTE  bVerMinor;	/* Minor Version Number			    */
	BYTE  bVerMajor;	/* Major Version Number			    */
	BYTE  bBrdCnt;		/* Number of devices configured		    */
	BYTE  abReserved[3];
};

struct ESCD_BrdHdr {
	/*
	 *  Board header:
	 *
	 *  There's one of these in front of every slot record (except on
	 *  EISA systems).
	 */

	WORD  wBrdRecSize;	/* Size of this board record		    */
	BYTE  bSlotNum;		/* EISA slot number			    */
	BYTE  bRsvrd1;
};

#pragma pack()

/*
 *  Unaligned access macros:
 *
 *  These are pretty simple for x86 machines, since the architecture supports
 *  unaligned storage references.
 */

#define	fetchESCD(p, t)    *((t far *)(p))
#define	storeESCD(p, t, v)  *((t far *)(p)) = (t)(v)

extern char	Update_escd;		/* Escd modified if set */
extern char	*Escd_name;		/* escd.rf or whatever */

/*
 * global function prototypes
 */
void read_escd(void);
void write_escd(void);
void MotherBoard();

#ifdef	__cplusplus
}
#endif

#endif /* _ESCD_H */
