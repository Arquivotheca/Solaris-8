/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _CBUS_H
#define	_CBUS_H

#pragma ident	"@(#)cbus.h	1.4	96/05/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	IN
#define	OUT

#define	TRUE		1
#define	FALSE		0

typedef	void *		PVOID;
typedef	void		VOID;

typedef	unsigned long	ULONG;

typedef	unsigned long	*PULONG;
typedef	long		*PLONG;

typedef	long		LONG;
typedef	char		CHAR;
typedef	char		BOOL;
typedef	char		*PCHAR;
typedef	unsigned char	UCHAR;
typedef	unsigned char	*PUCHAR;

typedef	unsigned short	USHORT;

extern char		*psm_map_phys(PVOID, ULONG, ULONG);

extern	void	psm_unmap_phys(PVOID, ULONG);
#ifndef sun
extern	void	bcopy(PVOID, PVOID, ULONG);
#endif

/*
 *
 * Processor Types - counting number
 */

#define	PT_NO_PROCESSOR	0x0
#define	PT_386		0x1
#define	PT_486		0x2
#define	PT_PENTIUM	0x3

/*
 *
 * Processor Attributes - counting number
 *
 */
#define	PA_CACHE_OFF	0x0
#define	PA_CACHE_ON	0x1

/*
 *
 * I/O Function - counting number
 *
 */
#define	IOF_NO_IO		0x0
#define	IOF_CBUS1_SIO		0x1
#define	IOF_CBUS1_SCSI		0x2
#define	IOF_REACH_IO		0x3
#define	IOF_ISA_BRIDGE		0x4
#define	IOF_EISA_BRIDGE		0x5
#define	IOF_HODGE		0x6
#define	IOF_MEDIDATA		0x7
#define	IOF_INVALID_ENTRY	0x8	/* the entry is invalid, */
					/* that pm must equal zero as well. */
#define	IOF_MEMORY		0x9

/*
 *
 * Bit fields of pel_features, independent of whether pm indicates it
 * has an attached processor or not.
 *
 */
#define	ELEMENT_SIO		0x00001		/* SIO present */
#define	ELEMENT_SCSI		0x00002		/* SCSI present */
#define	ELEMENT_IOBUS		0x00004		/* IO bus is accessible */
#define	ELEMENT_BRIDGE		0x00008		/* IO bus Bridge */
#define	ELEMENT_HAS_8259	0x00010		/* local 8259s present */
#define	ELEMENT_HAS_CBC		0x00020		/* local Corollary CBC */
#define	ELEMENT_HAS_APIC	0x00040		/* local Intel APIC */
#define	ELEMENT_WITH_IO		0x00080		/* some extra I/O device here */
						/* could be SCSI, SIO, etc */
#define	ELEMENT_RRD_RESERVED	0x20000		/* Old RRDs used this */

/*
 * Due to backwards compatibility, the check for an I/O
 * device is somewhat awkward.
 */

#define	ELEMENT_HAS_IO		(ELEMENT_SIO | ELEMENT_SCSI | ELEMENT_WITH_IO)

/*
 *
 * Bit fields of machine types
 *
 */
#define	MACHINE_CBUS1		0x1		/* Original C-bus 1 */
#define	MACHINE_CBUS1_XM	0x2		/* XM C-bus 1 */
#define	MACHINE_CBUS2		0x4		/* C-bus 2 */

/*
 *
 * Bit fields of supported environment types
 *
 */
#define	SCO_UNIX		0x1
#define	USL_UNIX		0x2
#define	WINDOWS_NT		0x4
#define	NOVELL			0x8

/*
 *
 * address of configuration passed
 *
 */
#define	RRD_RAM		0xE0000

/*
 *
 *  extended structures passed by RRD ROMs to various kernels
 *	for the Corollary smp architectures
 *
 *	layout of information passed to the kernels:
 *		The exact format of the configuration structures is hard
 *		coded in info.s (rrd).  The layout is designed such that
 *		the ROM version need not be in sync with the kernel version.
 *
 * checkword:		ULONG
 *			 - extended configuration list must be terminated
 *			   with EXT_CFG_END (0)
 * length:		ULONG
 *			 - length is for structure body only; does not include
 *			   either the checkword or length word
 *
 * structure body:	format determined by checkword
 *
 *
 */

typedef struct _ext_cfg_header {
	ULONG	ext_cfg_checkword;
	ULONG	ext_cfg_length;
} EXT_CFG_HEADER_T, *PEXT_CFG_HEADER;

/*
 *
 * slot parameter structure (overrides any matching previous entry,
 * but is usually used in conjunction with the ext_cfg_override)
 * in processor_configuration or ext_memory_board.
 *
 *	checkword is EXT_ID_INFO
 *
 *	each structure is 16 bytes wide, and any number
 *	of these structures can be presented by the ROM.
 *	the kernel will keep reading them until either:
 *
 *	a) an entry with id == 0x7f (this is treated as the list delimiter)
 *			OR
 *	b) the kernel's internal tables fill up.  at which point, only
 *	   the entries read thus far will be used and the rest ignored.
 *
 */
#define	EXT_ID_INFO	0x01badcab
typedef struct _ext_id_info {

	ULONG		id:7;

	/*
	 *
	 * pm == 1 indicates CPU, pm == 0 indicates non-CPU (ie: memory or I/O)
	 *
	 */
	ULONG		pm:1;

	ULONG		proc_type:4;
	ULONG		proc_attr:4;

	/*
	 *
	 * io_function != 0 indicates I/O,
	 * io_function == 0 or 9 indicates memory
	 *
	 */
	ULONG		io_function:8;

	/*
	 *
	 * io_attr can pertain to an I/O card or memory card
	 *
	 */
	ULONG		io_attr:8;

	/*
	 *
	 * pel_start & pel_size can pertain to a CPU card,
	 * I/O card or memory card
	 *
	 */
	ULONG		pel_start;
	ULONG		pel_size;

	ULONG		pel_features;

	/*
	 *
	 * below two fields can pertain to an I/O card or memory card
	 *
	 */
	ULONG		io_start;
	ULONG		io_size;

} EXT_ID_INFO_T, *PEXT_ID_INFO;

#define	LAST_EXT_ID	0x7f		/* delimit the extended ID list */

extern ULONG		cbus_valid_ids;
extern EXT_ID_INFO_T	cbusext_id_info[];

/*
 *
 * configuration parameter override structure
 *
 *	checkword is EXT_CFG_OVERRIDE.
 *	can be any length up to the kernel limit.  this
 *	is a SYSTEMWIDE configuration override structure.
 *
 */
#define	EXT_CFG_OVERRIDE	(ULONG)0xdeedcafe

typedef struct _ext_cfg_override {
	ULONG		baseram;
	ULONG		memory_ceiling;
	ULONG		resetvec;
	/*
	 *
	 * cbusio is the base of global C-bus I/O space.
	 *
	 */
	ULONG		cbusio;

	UCHAR		bootid;
	UCHAR		useholes;
	UCHAR		rrdarb;
	UCHAR		nonstdecc;
	ULONG		smp_creset;
	ULONG		smp_creset_val;
	ULONG		smp_sreset;

	ULONG		smp_sreset_val;
	ULONG		smp_contend;
	ULONG		smp_contend_val;
	ULONG		smp_setida;

	ULONG		smp_setida_val;
	ULONG		smp_cswi;
	ULONG		smp_cswi_val;
	ULONG		smp_sswi;

	ULONG		smp_sswi_val;
	ULONG		smp_cnmi;
	ULONG		smp_cnmi_val;
	ULONG		smp_snmi;

	ULONG		smp_snmi_val;
	ULONG		smp_sled;
	ULONG		smp_sled_val;
	ULONG		smp_cled;

	ULONG		smp_cled_val;
	ULONG		machine_type;
	ULONG		supported_environments;
	ULONG		broadcast_id;

} EXT_CFG_OVERRIDE_T, *PEXT_CFG_OVERRIDE;

extern EXT_CFG_OVERRIDE_T	CbusGlobal;

#define	EXT_CFG_END	0

/*
 *
 * this is the original structure passed from RRD to UNIX for the
 * Corollary multiprocessor architecture.  The only fields we are
 * still interested in is the jumper settings - all other fields
 * are now obtained from the extended configuration tables.  hence
 * the structure below contains only a subset of the original structure.
 *
 */

#define	ATMB			16
#define	OLDRRD_MAXMB		64	/* limit for 1987-style machines */

#define	MB(x)	((x) * 1024 * 1024)

typedef struct _rrd_configuration {

	ULONG		checkword;		/* must be 0xdeadbeef */
	UCHAR		mem[64];		/* signifies a real MB */
	UCHAR		jmp[ATMB];		/* signifies jumpered MB */

} RRD_CONFIGURATION_T, *PRRD_CONFIGURATION;

#define	JUMPER_SIZE	(sizeof (RRD_CONFIGURATION_T))

#ifdef	__cplusplus
}
#endif

#endif /* _CBUS_H */
