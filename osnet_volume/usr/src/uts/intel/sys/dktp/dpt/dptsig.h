/*
 * Copyright (c) 1995-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	__DPTSIG_H_
#define	__DPTSIG_H_

#pragma ident	"@(#)dptsig.h	1.9	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*	DPT SIGNATURE SPEC AND HEADER FILE				*/
/*	Signature Version 1 (sorry no 'A')				*/

/*	to make sure we are talking the same size under all OS's	*/
typedef unsigned char sigBYTE;
typedef unsigned short sigWORD;
typedef unsigned long sigLONG;

/*	Current Signature Version - sigBYTE dsSigVersion;	*/
/*	---------------------------------------------------------------	*/
#define	SIG_VERSION 1

/*	Processor Family - sigBYTE dsProcessorFamily;  DISTINCT VALUES	*/
/*	---------------------------------------------------------------	*/
/*	What type of processor the file is meant to run on.		*/
/*	This will let us know whether to read sigWORDs hi/lo or lo/high	*/
#define	PROC_INTEL	0x00	/*	Intel 80x86			*/
#define	PROC_MOTOROLA   0x01    /*	Motorola 68K			*/
#define	PROC_MIPS4000   0x02    /*	MIPS RISC 4000			*/
#define	PROC_ALPHA	0x03	/*	DEC Alpha			*/

/*	Specific Minimim Processor - sigBYTE dsProcessor; FLAG BITS 	*/
/*	-------------------------------------------------------------	*/
/*	Different bit definitions dependent on processor_family		*/

/*	PROC_INTEL: */
#define	PROC_8086	0x01	/*	Intel 8086	*/
#define	PROC_286	0x02	/*	Intel 80286s	*/
#define	PROC_386	0x04	/*	Intel 80386s	*/
#define	PROC_486	0x08	/*	Intel 80486s	*/
#define	PROC_PENTIUM	0x10	/*	Intel 586 aka P5 aka Pentiums	*/

/*	PROC_MOTOROLA:	*/
#define	PROC_68000	0x01	/*	Motorola 68000	*/
#define	PROC_68020	0x02	/*	Motorola 68020	*/
#define	PROC_68030	0x04	/*	Motorola 68030	*/
#define	PROC_68040	0x08	/*	Motorola 68040	*/

/*	Filetype - sigBYTE dsFiletype;		DISTINCT VALUES	*/
/* ------------------------------------------------------------	*/
#define	FT_EXECUTABLE	0	/*	Executable Program	*/
#define	FT_SCRIPT	1	/*	Script/Batch	File???	*/
#define	FT_HBADRVR	2	/*	HBA Driver		*/
#define	FT_OTHERDRVR	3	/*	Other	Driver	*/
#define	FT_IFS		4	/*	Installable Filesystem Driver1	*/
#define	FT_ENGINE	5	/*	DPT Engine			*/
#define	FT_COMPDRVR	6	/*	Compressed Driver Disk		*/
#define	FT_LANGUAGE	7	/*	Foreign Language file		*/
#define	FT_FIRMWARE	8	/*	Downloadable or actual Firmware	*/
#define	FT_COMMMODL	9	/*	Communications Module		*/
#define	FT_INT13	10	/*	INT 13 style HBA Driver		*/
#define	FT_HELPFILE	11	/*	Help file			*/
#define	FT_LOGGER	12	/*	Events 	Logger			*/
#define	FT_INSTALL	13	/*	An Install Program		*/
#define	FT_LIBRARY	14	/*	Storage	Manager	Real-Mode Calls	*/

/* Filetype flags - sigBYTE dsFiletypeFlags; FLAG	BITS		*/
/* ------------------------------------------------------------------	*/
#define	FTF_DLL		0x01	/*	Dynamic	Link	Library	*/
#define	FTF_NLM		0x02	/*	Netware	Loadable Module	*/
#define	FTF_OVERLAYS	0x04	/*	Uses overlays		*/
#define	FTF_DEBUG	0x08	/*	Debug version		*/
#define	FTF_TSR		0x10	/*	TSR			*/
#define	FTF_SYS		0x20	/*	DOS Lodable driver	*/
#define	FTF_PROTECTED	0x40	/*	Runs in	protected mode	*/
#define	FTF_APP_SPEC	0x80	/*	Application Specific	*/

/*	OEM - sigBYTE	dsOEM;		DISTINCT	VALUES	*/
/* ------------------------------------------------------------------ */
#define	OEM_DPT		0	/*	DPT	*/
#define	OEM_ATT		1	/*	ATT	*/
#define	OEM_NEC		2	/*	NEC	*/
#define	OEM_ALPHA	3	/*	Alphatronix	*/
#define	OEM_AST		4	/*	AST	*/

/*	Operating	System	- sigLONG dsOS;		FLAG	BITS	*/
/* ------------------------------------------------------------------	*/
#define	OS_DO		0x00000001	/* PC/MS-DOS			*/
#define	OS_WIND		0x00000002	/* Microsoft Windows 3.x	*/
#define	OS_WINDOWS_NT	0x00000004	/* Microsoft Windows NT		*/
#define	OS_OS2M		0x00000008	/* OS/2				*/
#define	OS_OS2L		0x00000010	/* Microsoft OS/2 1.301 - LADDR */
#define	OS_OS22x	0x00000020	/* IBM OS/2 2.x			*/
#define	OS_NW286	0x00000040	/* Novell	NetWare	286	*/
#define	OS_NW386	0x00000080	/* Novell	NetWare	386	*/
#define	OS_UNIX		0x00000100	/* Generic	Unix		*/
#define	OS_SCO_UNIX	0x00000200	/* SCO	Unix			*/
#define	OS_ATT_UNIX	0x00000400	/* ATT	Unix			*/
#define	OS_USG_UNIX	0x00000800	/* USG Unix			*/
#define	OS_INT_UNIX	0x00001000	/*	Interactive	Unix	*/
#define	OS_SOLARIS	0x00002000	/*	SunSoft	Solaris	*/
#define	OS_QNX	0x00004000	/*	QNX	for	Tom	Moch	*/
#define	OS_NEXTSTEP	0x00008000	/*	NeXTSTEP	*/
#define	OS_OTHER	0x80000000	/*	Other	*/

/*	Capabilities	-	sigWORD	dsCapabilities;	FLAG	BITS	*/
/*	--------------------------------------------------------------	*/
#define	CAP_RAID0	0x0001	/* RAID-0	*/
#define	CAP_RAID1	0x0002	/* RAID-1	*/
#define	CAP_RAID3	0x0004	/* RAID-3	*/
#define	CAP_RAID5	0x0008	/* RAID-5	*/
#define	CAP_SPAN	0x0010	/* Spanning	*/
#define	CAP_PASS	0x0020	/* Provides passthrough	*/
#define	CAP_OVERLAP	0x0040	/* Passthrough supports overlapped commands */
#define	CAP_ASPI	0x0080	/* Support	ASPI Command Requests	*/
#define	CAP_EXTEND	0x8000	/* Extended info appears after description */

/*	Devices	Supported	-	sigWORD	dsDeviceSupp; FLAG BITS	*/
/*	--------------------------------------------------------------	*/
#define	DEV_DASD	0x0001	/*	DASD (hard drives)	*/
#define	DEV_TAPE	0x0002	/*	Tape	drives	*/
#define	DEV_PRINTER	0x0004	/*	Printers	*/
#define	DEV_PROC	0x0008	/*	Processors	*/
#define	DEV_WORM	0x0010	/*	WORM	drives	*/
#define	DEV_CDROM	0x0020	/*	CD-ROM	drives	*/
#define	DEV_SCANNER	0x0040	/*	Scanners	*/
#define	DEV_OPTICAL	0x0080	/*	Optical	Drives	*/
#define	DEV_JUKEBOX	0x0100	/*	Jukebox	*/
#define	DEV_COMM	0x0200	/*	Communications	Devices	*/
#define	DEV_OTHER	0x0400	/*	Other	Devices	*/
#define	DEV_ALL	0xFFFF	/*	All	SCSI	Devices	*/

/* Adapters Families Supported - sigWORD dsAdapterSupp;	FLAG	BITS	*/
/*	--------------------------------------------------------------	*/
#define	ADF_2001	0x0001	/*	PM2001	*/
#define	ADF_2012A	0x0002	/*	PM2012A	*/
#define	ADF_PLUS_ISA	0x0004	/*	PM2011,PM2021	*/
#define	ADF_PLUS_EISA	0x0008	/*	PM2012B,PM2022	*/
#define	ADF_ALL	0xFFFF	/* ALL	DPT adapters	*/

/* Application	- sigWORD	dsApplication;	FLAG	BITS		*/
/*	------------------------------------------------------		*/
#define	APP_DPTMGR	0x0001	/*	DPT	Storage	Manager		*/
#define	APP_ENGINE	0x0002	/*	DPT	Engine			*/
#define	APP_SYTOS	0x0004	/*	Sytron	Sytos	Plus		*/
#define	APP_CHEYENNE	0x0008	/*	Cheyenne ARCServe + ARCSolo 	*/
#define	APP_MSCDEX	0x0010	/*	Microsoft CD-ROM extensions 	*/
#define	APP_NOVABACK	0x0020	/*	NovaStor Novaback		*/
#define	APP_AIM		0x0040	/*	Archive	Information Manager	*/

/*	Requirements	-	sigBYTE	dsRequirements;	FLAG	BITS	*/
/*	--------------------------------------------------------------	*/
#define	REQ_SMARTROM	0x01	/*	Requires SmartROM to be	present	*/
#define	REQ_DPTDDL	0x02	/*	Requires DPTDDL.SYS loaded	*/
#define	REQ_HBA_DRIVER	0x04	/*	Requires an HBA	driver loaded	*/
#define	REQ_ASPI_TRAN	0x08	/*	Requires an ASPI Transport Modules */
#define	REQ_ENGINE	0x10	/*	Requires a DPT Engine to be loaded */
#define	REQ_COMM_ENG	0x20	/*	Requires a DPT Communications Engine */

/*	DPT	Signature	Structure	*/

#define	DPT_VERSION	2
#define	DPT_REVISION	'a'
#define	DPT_SUBREVISION	'0'
#define	DPT_MONTH	12
#define	DPT_DAY		02
#define	DPT_YEAR	13

typedef	struct	dpt_sig	{
	char	dsSignature[6];		/* ALWAYS "dPtSiG"		*/
	sigBYTE	dsSigVersion;		/* signature version (now 1)	*/
	sigBYTE	filler1;		/* filler			*/
	sigBYTE	filler2;		/* filler			*/
	sigBYTE	dsFiletype;		/* type	of	file		*/
	sigBYTE	dsFiletypeFlags;	/* flags to specify load typec 	*/
	sigBYTE	dsOEM;			/* OEM file was created for 	*/
	sigLONG	dsOS;			/* which Operating	systems	*/
	sigWORD	dsCapabilities;		/* RAID levels, etc.		*/
	sigWORD	dsDeviceSupp;		/* Types of SCSI devices 	*/
	sigWORD	dsAdapterSupp;		/* DPT adapter families 	*/
	sigWORD	dsApplication;		/* applications file is for	*/
	sigBYTE	dsRequirements;		/* Other driver dependencies	*/
	sigBYTE	filler3;		/* filler			*/
	sigBYTE	filler4;		/* filler			*/
	sigBYTE	filler5;		/* filler			*/
	sigBYTE	filler6;		/* filler			*/
	sigBYTE	filler7;		/* filler			*/
	sigBYTE	filler8;		/* filler			*/
	char	dsDescription[50];	/* description NULL terminated	*/
}	dpt_sig_t;

/*	32 bytes min - with no description. Put NULL at description[0]	*/
/*	81 bytes max - with 49 character description plus NULL.		*/

#ifdef	__cplusplus
}
#endif

#endif	/* __DPTSIG_H_ */
