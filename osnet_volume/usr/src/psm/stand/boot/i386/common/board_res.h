/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * board_res.h -- public definitions for boards and resources
 */

#ifndef	_BOARD_RES_H
#define	_BOARD_RES_H

#pragma ident	"@(#)board_res.h	1.2	99/08/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * IMPORTANT:	this file is shared between bootconf.exe and boot.bin -
 * usr/src/realmode/dos/bootconf and usr/src/psm/stand/boot/i386/common
 * Whenever you update this file, remember it affects both boot.bin and
 * bootconf.exe.
 */

/*
 *  This file contains record definitions for two types of objects:
 *
 *     1. Boards (a.k.a. devices).  These are meant to model physical devices,
 *        and are identified by the manufacturer's EISA device ID and the
 *        bus slot at which they are installed.  Each board record contains
 *        one or more ...
 *
 *     2. Resources.  These records identify the bus resources (I/O ports,
 *        IRQ lines, DMA channels, and/or Memory addresses) dedicated to each
 *        function of the device.
 *
 *   Although similiar in form, these records do NOT represent configuration
 *   data as stored in EISA NV-RAM or the Plug-and-Play ESCD.  Platform-
 *   specific conversion routines must be used to convert to and from the
 *   strict EISA format (see function prototypes at the end of this file).
 */

typedef struct dvp {
	/*
	 *  Device node properties:
	 *
	 *  Certain devices may have special (i.e, non-standard) properties
	 *  associated with their Solaris device nodes.  The realmode database
	 *  entries describing such devices record the special properties in
	 *  a linked list of "devprop" elements with the following format ...
	 */

	struct dvp *next;	/* Next entry in property list. */
	short vof;		/* Offset to property value */
	short bin;		/* Property is binary flag */
	short len;		/* property length */
	char name[1];		/* Name of this property */

	/*
	 *  Get pointer to property value:  The value of a given property
	 *  follows the "name" field, but may be missing.  This macro re-
	 *  turns a pointer to the property value, or NULL if it's missing.
	 */
#define	prop_value(p) (char far *) ((p)->vof ? &(p)->name[(p)->vof] : 0)

} devprop;

#pragma pack(2)		/* Pack 'em the way DOS likes to see 'em	    */

typedef struct resource {
	/*
	 *  EISA Resource Definition:
	 *
	 *  This record is used to identify the bus resources associated with
	 *  a given EISA function.  There are four types of bus resources, as
	 *  indicated by the RESF_TYPE bits in the "typeflags" field, below.
	 *  The "base" and "len" fields serve to uniquely identify the resource
	 *  in question.
	 *
	 *  Normally, an array of resource records (the "resource list") is as-
	 *  sociated with each function record.  The entries in this list may
	 *  be used in two different ways:
	 *
	 *    1.  To represent the bus resources that are currently assigned to
	 *	  the function.  In this case, the RESF_ALT flags of all entries
	 *	  in the list are zero.
	 *
	 *    2.  To represent all possible bus resources that could be legally
	 *	  assigned to the function.  In this case, sublists of valid
	 *	  resource alternates are listed one after the other, with the
	 *	  first entry of each group marked by a non-zero RESF_FIRST bit
	 *	  and each member of the group marked with a non-zero RESF_ALT
	 *	  bit (to distinquish them from required resource assignments).
	 *
	 *  It is also possible to "disable" resource usage for some functions.
	 *  In this case, there will be an alternate resource entry marked
	 *  RESF_DISABLE in the resource that describes the possible configu-
	 *  rations (the "function model"), but no corresponding entry in the
	 *  actual function description (i.e, ESCD entry).
	 */

	unsigned long base;		/* Base resource address.	    */
	unsigned long length;		/* Length of resource range.	    */
	unsigned short EISAflags;	/* Type-specific flags:		    */
					/* see RES_* below */

	unsigned short flags;		/* Resource type flags:		    */
#define		RESF_TYPE   0x000F 	/* .. Type mask:		    */
#define		RESF_Port   0x0001	/* .. .. I/O port definition	    */
#define		RESF_Irq    0x0002	/* .. .. Interrupt definition	    */
#define		RESF_Dma    0x0003	/* .. .. DMA channel definition	    */
#define		RESF_Mem    0x0004	/* .. .. Memory definition	    */
#define		RESF_Max    0x0005	/* .. .. ** Max number of types **  */

#define		RESF_FIRST  0x0010	/* .. First resource of this group  */
#define		RESF_ALT    0x0020	/* .. Alternate resource flag	    */
#define		RESF_SHARE  0x0040	/* .. Shared resource flag	    */
#define		RESF_DISABL 0x0080	/* .. Disabled resource flag	    */
#define		RESF_MULTI  0x0100	/* .. Multi-entry res, more follow  */
#define		RESF_SUBFN  0x0200	/* .. Start of next subfunction	    */
#define		RESF_ANY    0x0400	/* .. Multi-resource/single entry   */
#define		RESF_CNFLCT 0x0800	/* .. Conflict occured		    */
#define		RESF_WEAK   0x1000	/* .. Weak binding		    */
#define		RESF_USURP  0x4000	/* .. Usurp a weak resource	    */
#define		RESF_MARK   0x8000	/* .. Resource marker bit	    */

} Resource;

typedef struct {
	unsigned long start;
	unsigned long len;
} pci_range_t;

typedef struct board {
	/*
	 *  EISA Board (i.e, device) Description:
	 *
	 *  This is the internal form of an ESCD board record.  It consists of
	 *  a fixed-format header that describes general characteristics of the
	 *  board, followed by a variable-length portion containing bus resource
	 *  usage information.
	 *
	 *  Most fields contain simple numeric values and/or bit flags.  Access
	 *  methods (macros) are provide to extract data from fields with more
	 *  complex encodings.
	 *
	 *  Typical memory layout for board records looks something like this:
	 *
	 *	Fixed-format:	+--------------------------+
	 *			|	Board Header	   |
	 *	Variable-format:+--------------------------+
	 *			|  Resource #1		   |
	 *			+--------------------------+
	 *			|  Resource #N		   |
	 *			+--------------------------+
	 */

	struct board *link;		/* Next board in configuration.	    */
	struct dvp   *prop;		/* Property list header		    */
	struct devtrans *dbentryp;	/* pointer to master file entry	    */
#define	DB_NODB	((struct devtrans *)0)	/* .. no database entry		    */
	struct board *beflink;		/* link of Boards by bef	    */

	/*
	 *  Note, fields from here on are written out to/read in from a file
	 *  and thus can't contain any pointers
	 */

	unsigned short reclen;		/* Total record length		    */
	unsigned short buflen;		/* Total length of buffer	    */
	unsigned long  devid;		/* Device ID, format depends on bus */

/* define a magic ID for Compaq 8x5 chips: illegal PCI vid, CPQ vid for did */
#define	MAGIC_COMPAQ_8X5_DEVID	0xFFFF0E11

/* define a magic ID for NCR PQS devices: illegal PCI vid, NCR vid for did */
#define	MAGIC_NCR_PQS_DEVID	0xFFFF101A

#define	SYMBIOS_VID		0x1000	/* Symbios vendor ID... */
#define	SYMBIOS_825_DID		0x3	/* ...and device IDs.. */
#define	SYMBIOS_875_DID		0xF	/* ..to lie about on Compaqs */

	unsigned char  bustype;		/* Bus type (see "befext.h")	    */
	unsigned short flags;		/* General device flags:	    */
#define		BRDF_PNPBOO 0x0001	/* .. PnP boot device		    */
#define		BRDF_PGM    0x0002	/* .. Programable device if set	    */
#define		BRDF_DISK   0x0004	/* .. Save node on disk		    */
#define		BRDF_CHAN   0x0008	/* .. Channel board if set	    */
#define		BRDF_NOTREE 0x0010	/* .. Dont put node in tree	    */
#define		BRDF_PNPBIOS 0x0020	/* .. Device found by PnP bios	    */
#define		BRDF_DISAB  0x0040	/* .. Board disabled		    */
#define		BRDF_ACPI   0x0080	/* .. Device found by ACPI	    */
#define		BRDF_NOLBA   0x0100	/* .. BEF doesn't support LBA fns   */

	unsigned short slotflags;	/* Slot flags (see <befext.h>)	    */
	unsigned char  slot;		/* EISA slot number		    */
#define		MAX_SLOTS	64	/* .. PnP limit: boards/machine	    */

	unsigned char category;		/* EISA device category:	    */
#define		DCAT_UNKNWN 0x00	/* .. Unknown category		    */
#define		DCAT_COM    0x01	/* .. Serial devices (modems)	    */
#define		DCAT_KEY    0x02	/* .. Keyboards			    */
#define		DCAT_MEM    0x03	/* .. Memory extender cards	    */
#define		DCAT_MFC    0x04	/* .. Multi-function cards	    */
#define		DCAT_MSD    0x05	/* .. Mass storage dev. controller  */
#define		DCAT_NET    0x06	/* .. Network adapters		    */
#define		DCAT_PLAT   0x07	/* .. Platform drivers		    */
#define		DCAT_OTH    0x09	/* .. Miscellaneouse (i.e, other)   */
#define		DCAT_PAR    0x0A	/* .. Parallel devices (printers)   */
#define		DCAT_PTR    0x0B	/* .. Pointing devices		    */
#define		DCAT_VID    0x0E	/* .. Video cards (CGA/VGA/SVGA)    */

	unsigned short rescnt[4];	/* Resource counts by type:	    */
#define		  RESC_Port (RESF_Port-1)
#define		  RESC_Irq  (RESF_Irq-1)
#define		  RESC_Dma  (RESF_Dma-1)
#define		  RESC_Mem  (RESF_Mem-1)

	unsigned short resoff;		/* Resource list offset		    */

	union {
		struct pnp_s {
			unsigned char csn; /* PnP isa card select number */
			unsigned char ldn; /* PnP isa logical device number  */
			unsigned char multifn; /* PnP multi function board   */
			unsigned long funcid;  /* PnP function id (eisa fmt) */
			unsigned long boardid; /* PnP board id (eisa format) */
			unsigned long serial;  /* PnP serial no		*/
			char *compat;	/* list of compatible ids	*/
			char *desc;	/* description from nvram	*/
		} *pnp;


#define	pnp_csn bus_u.pnp->csn
#define	pnp_ldn bus_u.pnp->ldn
#define	pnp_board_id bus_u.pnp->boardid
#define	pnp_serial bus_u.pnp->serial
#define	pnp_func_id bus_u.pnp->funcid
#define	pnp_multi_func bus_u.pnp->multifn
#define	pnp_compat_ids bus_u.pnp->compat
#define	pnp_desc bus_u.pnp->desc

		struct {
			unsigned char devfunc;	/* device & function number */
#define		FUNF_FCTNNUM  0x07	/* .. Function number		    */
#define		FUNF_DEVNUM   0xF8	/* .. Device number * 8 (or slot)   */
#define		FUNF_DEVSHFT  3		/* .. Device shift		    */
			unsigned char  busno;	/* Bus number */
			unsigned short cpqdid;
			unsigned short venid;	/* vendor id */
			unsigned short devid;	/* device id */
			unsigned short subvenid;	/* sub vendor id */
			unsigned short subdevid;	/* sub device id */
			unsigned long  class;	/* class code */
			unsigned char multi_func; /* multi function device */
			unsigned short ppb_bcntrl; /* ppb bridge control flag */
			pci_range_t ppb_io;	/* ppb io ppb range */
			pci_range_t ppb_mem;	/* ppb mem ppb range */
			pci_range_t ppb_pmem;	/* ppb prefetch mem range */
		} pci;

#define	pci_devfunc bus_u.pci.devfunc
#define	pci_busno bus_u.pci.busno
#define	pci_cpqdid bus_u.pci.cpqdid
#define	pci_venid bus_u.pci.venid
#define	pci_devid bus_u.pci.devid
#define	pci_subvenid bus_u.pci.subvenid
#define	pci_subdevid bus_u.pci.subdevid
#define	pci_class bus_u.pci.class
#define	pci_multi_func bus_u.pci.multi_func
#define	pci_ppb_bcntrl bus_u.pci.ppb_bcntrl
#define	pci_ppb_io bus_u.pci.ppb_io
#define	pci_ppb_mem bus_u.pci.ppb_mem
#define	pci_ppb_pmem bus_u.pci.ppb_pmem

		/*
		 * Since some acpi devices are found by pnpbios too,
		 * the pnpbios and acpi structures are merged together
		 * in the pnpb_acpi union so they can share each other
		 * information.
		 */
		struct {
			struct {
				unsigned char base_type;
				unsigned char sub_type;
				unsigned char interface_type;
			} pnpbios;
			struct {
				unsigned long id;
				unsigned long hid;
				unsigned long uid;
				unsigned long nameseg;
				unsigned long status;
			} acpi;
		} pnpb_acpi;

/* pnpbios related */
#define	pnpbios_base_type bus_u.pnpb_acpi.pnpbios.base_type
#define	pnpbios_sub_type bus_u.pnpb_acpi.pnpbios.sub_type
#define	pnpbios_interface_type bus_u.pnpb_acpi.pnpbios.interface_type
/* acpi related */
#define	acpi_id bus_u.pnpb_acpi.acpi.id
#define	acpi_hid bus_u.pnpb_acpi.acpi.hid
#define	acpi_uid bus_u.pnpb_acpi.acpi.uid
#define	acpi_nseg bus_u.pnpb_acpi.acpi.nameseg
#define	acpi_status bus_u.pnpb_acpi.acpi.status
/*
 * acpi_status definitions
 */
#define	ACPI_CLEAR	0x0
#define	ACPI_CONFLICT	0x1

	} bus_u;

	unsigned long var;		/* Variable-length portion!	    */

#define	resource_count(p) (unsigned)					      \
(									      \
	/*								      \
	 *  Get total resource count for function:  Returns the sum of the    \
	 *  per-type resource counts in the function record at "p"	      \
	 */								      \
	(p)->rescnt[RESC_Mem]						      \
		    + (p)->rescnt[RESC_Irq]				      \
		    + (p)->rescnt[RESC_Dma]				      \
		    + (p)->rescnt[RESC_Port]				      \
)

#define	resource_list(p)						      \
(									      \
	/*								      \
	 *  Get resource list:  Returns a pointer to the first entry in this  \
	 *  function's resource list (or NULL if the list is empty).	      \
	 */								      \
	(Resource *)((char *)(p) + (p)->resoff)			      \
)

#define	programable_device(p) (short)					      \
(									      \
	/*								      \
	 *  Programmable device predicate:  Returns TRUE if the board record  \
	 *  at "p" describes a programmable device.			      \
	 */								      \
	((p)->flags & BRDF_PGM) != 0					      \
)

} Board;

/*
 * Resource type flags:
 *
 * These bits appear in the "flags" entry of each resource descriptor
 * The bit patterns must match the equivalently defined bit fields in
 * the "xxxInfo" structs defined in <escd.h>
 */
		/*	*** Memory flags ***	    */

#define	RES_MEM_WRITE	  0x0001	/* RAM if set, ROM otherwise	    */
#define	RES_MEM_CACHED	  0x0002	/* Cached memory if set		    */
#define	RES_MEM_TYPE	  0x0018	/* Memory type code:		    */
#define	  RES_MEM_TYPE_SYS  0x0000	/* .. system (local bus) memory	    */
#define	  RES_MEM_TYPE_EXP  0x0008	/* .. expansion bus memory	    */
#define	  RES_MEM_TYPE_VIR  0x0010	/* .. virtual memory		    */
#define	  RES_MEM_TYPE_OTH  0x0018	/* .. something else!		    */
#define	RES_MEM_SHARE	  0x0020	/* Memory may be shared		    */
#define	RES_MEM_SIZE	  0x0300	/* Memory size code		    */
#define	  RES_MEM_SIZE_BYTE 0x0000	/* .. 8 bits			    */
#define	  RES_MEM_SIZE_WORD 0x0100	/* .. 16 bits			    */
#define	  RES_MEM_SIZE_LONG 0x0200	/* .. 32 bits			    */
#define	  RES_MEM_SIZE_HUGE 0x0300	/* .. 64 bits			    */
#define	RES_MEM_DECODE	  0x0C00	/* Decode size (address bits)	    */
#define	  RES_MEM_CODE_1MB  0x0000	/* .. 20 bits (1MB address space)   */
#define	  RES_MEM_CODE_16MB 0x0400	/* .. 24 bits (16MB address space)  */
#define	  RES_MEM_CODE_4GB  0x0800	/* .. 32 bits (4GB address space)   */

		/*	  *** IRQ flags ***	    */

#define	RES_IRQ_TRIGGER	   0x0020	/* Level triggered if set	    */
#define	RES_IRQ_SHARE	   0x0040	/* Devices may share IRQ	    */

		/*	  *** DMA flags ***	    */

#define	RES_DMA_SHARE	  0x0040	/* DMA channel may be shared	    */
#define	RES_DMA_SIZE	  0x0C00	/* DMA transfer size		    */
#define	  RES_DMA_SIZE_8B   0x0000	/* .. 8 bits			    */
#define	  RES_DMA_SIZE_16B  0x0400	/* .. 16 bits			    */
#define	  RES_DMA_SIZE_32B  0x0800	/* .. 32 bits			    */
#define	  RES_DMA_SIZE_64B  0x0C00	/* .. 64 bit			    */
#define	RES_DMA_TIME	  0x3000	/* DMA timing code		    */
#define	  RES_DMA_TIME_ISA  0x0000	/* .. ISA compatible		    */
#define	  RES_DMA_TIME_A    0x1000	/* .. Type "A"			    */
#define	  RES_DMA_TIME_B    0x2000	/* .. Type "B"			    */
#define	  RES_DMA_TIME_C    0x3000	/* .. Burst			    */

		/*	*** I/O Port flags ***	    */

#define	RES_PORT_SHARE	   0x0040	/* Devices may share port	    */

		/*	  *** Slot Flags ***	    */

#define	RES_SLOT_DUPID	  0x000F	/* Duplicate config file number	    */
#define	RES_SLOT_TYPE	  0x0030	/* Slot type:			    */
#define	  RES_SLOT_TYPE_EXP 0x0000	/* .. expansion slot		    */
#define	  RES_SLOT_TYPE_MB  0x0010	/* .. embedded slot (motherboard)   */
#define	  RES_SLOT_TYPE_VIR 0x0020	/* .. virtual slot		    */
#define	RES_SLOT_NOREAD	  0x0040	/* Not readable if set		    */
#define	RES_SLOT_DUPPRES  0x0080	/* Duplicate .cfg present if set    */
#define	RES_SLOT_ENABSUP  0x0100	/* Board enable supported if set    */
#define	RES_SLOT_IOCHKERR 0x0200	/* IOCHKERR supported if set	    */
#define	RES_SLOT_LOCK	  0x0400	/* Configuration locked if set	    */
#define	RES_SLOT_PROBE	  0x0800	/* Device found by probe	    */
#define	RES_SLOT_EISA	  0x2000	/* EISA slot if set		    */
#define	RES_SLOT_NOCFG	  0x4000	/* No .cfg file if set		    */
#define	RES_SLOT_CFGSTAT  0x8000	/* Configuration incomplete if set  */

#define	MASTER_LINE_MAX 2000		/* Maximum size of master line	    */

#pragma pack()

typedef struct devtrans {
	/*
	 *  Device translation table:
	 *
	 *    Entries in this table establish the correspondence between an
	 *    EISA device type and the Solaris x86 realmode driver ("*.bef"
	 *    file) that supports devices of that type.  This information is
	 *    initially extracted from the Solaris realmode device database.
	 */

	unsigned long devid;	/* EISA/PCI device ID			    */
	unsigned char bustype;  /* Bus type (see RES_BUS_* in <befext.h>).  */
	char real_driver[9];	/* Realmode driver name (without ".bef")    */
	char unix_driver[17];   /* Unix driver name			    */
	unsigned char category;	/* Device category (DCAT_* in <boards.h>)   */
	devprop *proplist;	/* Device node property list		    */
	char *dname;		/* Descriptive (ASCII) name		    */
	unsigned short seq;	/* Sort sequencer			    */
} devtrans;

#ifdef	__cplusplus
}
#endif

#endif	/* _BOARD_RES_H */
