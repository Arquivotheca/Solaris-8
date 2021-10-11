/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _SYS_LINK_H
#define	_SYS_LINK_H

#pragma ident	"@(#)link.h	1.50	99/10/07 SMI"	/* SVr4.0 1.9	*/

#ifndef	_ASM
#include <sys/types.h>
#include <sys/elftypes.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Communication structures for the run-time linker.
 */

/*
 * The following data structure provides a self-identifying union consisting
 * of a tag from a known list and a value.
 */
#ifndef	_ASM
typedef struct {
	Elf32_Sword d_tag;		/* how to interpret value */
	union {
		Elf32_Word	d_val;
		Elf32_Addr	d_ptr;
		Elf32_Off	d_off;
	} d_un;
} Elf32_Dyn;

#if (defined(_LP64) || ((__STDC__ - 0 == 0) && (!defined(_NO_LONGLONG))))
typedef struct {
	Elf64_Xword d_tag;		/* how to interpret value */
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;
#endif	/* (defined(_LP64) || ((__STDC__ - 0 == 0) ... */
#endif

/*
 * Tag values
 */
#define	DT_NULL		0	/* last entry in list */
#define	DT_NEEDED	1	/* a needed object */
#define	DT_PLTRELSZ	2	/* size of relocations for the PLT */
#define	DT_PLTGOT	3	/* addresses used by procedure linkage table */
#define	DT_HASH		4	/* hash table */
#define	DT_STRTAB	5	/* string table */
#define	DT_SYMTAB	6	/* symbol table */
#define	DT_RELA		7	/* addr of relocation entries */
#define	DT_RELASZ	8	/* size of relocation table */
#define	DT_RELAENT	9	/* base size of relocation entry */
#define	DT_STRSZ	10	/* size of string table */
#define	DT_SYMENT	11	/* size of symbol table entry */
#define	DT_INIT		12	/* _init addr */
#define	DT_FINI		13	/* _fini addr */
#define	DT_SONAME	14	/* name of this shared object */
#define	DT_RPATH	15	/* run-time search path */
#define	DT_SYMBOLIC	16	/* shared object linked -Bsymbolic */
#define	DT_REL		17	/* addr of relocation entries */
#define	DT_RELSZ	18	/* size of relocation table */
#define	DT_RELENT	19	/* base size of relocation entry */
#define	DT_PLTREL	20	/* relocation type for PLT entry */
#define	DT_DEBUG	21	/* pointer to r_debug structure */
#define	DT_TEXTREL	22	/* text relocations remain for this object */
#define	DT_JMPREL	23	/* pointer to the PLT relocation entries */

#define	DT_MAXPOSTAGS	24	/* number of positive tags */


/*
 * The following values have been deprecated and remain here to allow
 * compatibility with older binaries.
 */
#define	DT_DEPRECATED_SPARC_REGISTER	0x7000001

/*
 * DT_* entries which fall between DT_VALRNGHI & DT_VALRNGLO use the
 * Dyn.d_un.d_val field of the Elf*_Dyn structure.
 */
#define	DT_VALRNGLO	0x6ffffd00
#define	DT_CHECKSUM	0x6ffffdf8	/* elf checksum */
#define	DT_PLTPADSZ	0x6ffffdf9	/* pltpadding size */
#define	DT_MOVEENT	0x6ffffdfa	/* move table entry size */
#define	DT_MOVESZ	0x6ffffdfb	/* move table size */
#define	DT_FEATURE_1	0x6ffffdfc	/* feature holder */
#define	DT_POSFLAG_1	0x6ffffdfd	/* flags for DT_* entries, effecting */
					/*	the following DT_* entry. */
					/*	See DF_P1_* definitions */
#define	DT_SYMINSZ	0x6ffffdfe	/* syminfo table size (in bytes) */
#define	DT_SYMINENT	0x6ffffdff	/* syminfo entry size (in bytes) */
#define	DT_VALRNGHI	0x6ffffdff

/*
 * DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
 * Dyn.d_un.d_ptr field of the Elf*_Dyn structure.
 *
 * If any adjustment is made to the ELF object after it has been
 * built these entries will need to be adjusted.
 */
#define	DT_ADDRRNGLO	0x6ffffe00
#define	DT_CONFIG	0x6ffffefa	/* configuration information */
#define	DT_DEPAUDIT	0x6ffffefb	/* dependency auditing */
#define	DT_AUDIT	0x6ffffefc	/* object auditing */
#define	DT_PLTPAD	0x6ffffefd	/* pltpadding (sparcv9) */
#define	DT_MOVETAB	0x6ffffefe	/* move table */
#define	DT_SYMINFO	0x6ffffeff	/* syminfo table */
#define	DT_ADDRRNGHI	0x6ffffeff

#define	DT_RELACOUNT	0x6ffffff9	/* number of RELATIVE relocations */
#define	DT_RELCOUNT	0x6ffffffa	/* number of RELATIVE relocations */

#define	DT_FLAGS_1	0x6ffffffb	/* stat flags - see DF_1_* defs */
#define	DT_VERDEF	0x6ffffffc	/* version definition table and */
#define	DT_VERDEFNUM	0x6ffffffd	/*	associated no. of entries */
#define	DT_VERNEED	0x6ffffffe	/* version needed table and */
#define	DT_VERNEEDNUM	0x6fffffff	/* 	associated no. of entries */
#define	DT_LOPROC	0x70000000	/* processor specific range */
#define	DT_AUXILIARY	0x7ffffffd	/* shared library auxiliary name */
#define	DT_USED		0x7ffffffe	/* ignored - same as needed */
#define	DT_FILTER	0x7fffffff	/* shared library filter name */
#define	DT_HIPROC	0x7fffffff

/*
 * Values for the DT_POSFLAG_1 .dynamic entry.
 * These values only affect the following DT_* entry.
 */
#define	DF_P1_LAZYLOAD	0x00000001	/* following object is to be */
					/*    lazy loaded */
#define	DF_P1_GROUPPERM	0x00000002	/* following object's symbols are */
					/*	not available for general */
					/*	symbol bindings. */

/*
 * Values for the DT_FLAGS_1 .dynamic entry.
 */
#define	DF_1_NOW	0x00000001	/* set RTLD_NOW for this object */
#define	DF_1_GLOBAL	0x00000002	/* set RTLD_GLOBAL for this object */
#define	DF_1_GROUP	0x00000004	/* set RTLD_GROUP for this object */
#define	DF_1_NODELETE	0x00000008	/* set RTLD_NODELETE for this object */
#define	DF_1_LOADFLTR	0x00000010	/* trigger filtee loading at runtime */
#define	DF_1_INITFIRST	0x00000020	/* set RTLD_INITFIRST for this object */
#define	DF_1_NOOPEN	0x00000040	/* set RTLD_NOOPEN for this object */
#define	DF_1_ORIGIN	0x00000080	/* ORIGIN processing required */
#define	DF_1_DIRECT	0x00000100	/* direct binding enabled */
#define	DF_1_TRANS	0x00000200
#define	DF_1_INTERPOSE	0x00000400	/* object is an 'interposer' */
#define	DF_1_NODEFLIB	0x00000800	/* ignore default library search path */
#define	DF_1_NODUMP	0x00001000	/* object can't be dldump(3x)'ed */
#define	DF_1_CONFALT	0x00002000	/* configuration alternative created */
#define	DF_1_ENDFILTEE	0x00004000	/* filtee terminates filters search */

/*
 * Values set to DT_FEATURE tag's d_val.
 */
#define	DTF_1_PARINIT	0x00000001	/* partially initialization feature */
#define	DTF_1_CONFEXP	0x00000002	/* configuration file expected */



/*
 * Version structures.  There are three types of version structure:
 *
 *  o	A definition of the versions within the image itself.
 *	Each version definition is assigned a unique index (starting from
 *	VER_NDX_BGNDEF)	which is used to cross-reference symbols associated to
 *	the version.  Each version can have one or more dependencies on other
 *	version definitions within the image.  The version name, and any
 *	dependency names, are specified in the version definition auxiliary
 *	array.  Version definition entries require a version symbol index table.
 *
 *  o	A version requirement on a needed dependency.  Each needed entry
 *	specifies the shared object dependency (as specified in DT_NEEDED).
 *	One or more versions required from this dependency are specified in the
 *	version needed auxiliary array.
 *
 *  o	A version symbol index table.  Each symbol indexes into this array
 *	to determine its version index.  Index values of VER_NDX_BGNDEF or
 *	greater indicate the version definition to which a symbol is associated.
 *	(the size of a symbol index entry is recorded in the sh_info field).
 */
#ifndef	_ASM

typedef struct {			/* Version Definition Structure. */
	Elf32_Half	vd_version;	/* this structures version revision */
	Elf32_Half	vd_flags;	/* version information */
	Elf32_Half	vd_ndx;		/* version index */
	Elf32_Half	vd_cnt;		/* no. of associated aux entries */
	Elf32_Word	vd_hash;	/* version name hash value */
	Elf32_Word	vd_aux;		/* no. of bytes from start of this */
					/*	verdef to verdaux array */
	Elf32_Word	vd_next;	/* no. of bytes from start of this */
} Elf32_Verdef;				/*	verdef to next verdef entry */

typedef struct {			/* Verdef Auxiliary Structure. */
	Elf32_Word	vda_name;	/* first element defines the version */
					/*	name. Additional entries */
					/*	define dependency names. */
	Elf32_Word	vda_next;	/* no. of bytes from start of this */
} Elf32_Verdaux;			/*	verdaux to next verdaux entry */


typedef	struct {			/* Version Requirement Structure. */
	Elf32_Half	vn_version;	/* this structures version revision */
	Elf32_Half	vn_cnt;		/* no. of associated aux entries */
	Elf32_Word	vn_file;	/* name of needed dependency (file) */
	Elf32_Word	vn_aux;		/* no. of bytes from start of this */
					/*	verneed to vernaux array */
	Elf32_Word	vn_next;	/* no. of bytes from start of this */
} Elf32_Verneed;			/*	verneed to next verneed entry */

typedef struct {			/* Verneed Auxiliary Structure. */
	Elf32_Word	vna_hash;	/* version name hash value */
	Elf32_Half	vna_flags;	/* version information */
	Elf32_Half	vna_other;
	Elf32_Word	vna_name;	/* version name */
	Elf32_Word	vna_next;	/* no. of bytes from start of this */
} Elf32_Vernaux;			/*	vernaux to next vernaux entry */

typedef	Elf32_Half 	Elf32_Versym;	/* Version symbol index array */

typedef struct {
	Elf32_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf32_Half	si_flags;	/* per symbol flags */
} Elf32_Syminfo;


#if (defined(_LP64) || ((__STDC__ - 0 == 0) && (!defined(_NO_LONGLONG))))
typedef struct {
	Elf64_Half	vd_version;	/* this structures version revision */
	Elf64_Half	vd_flags;	/* version information */
	Elf64_Half	vd_ndx;		/* version index */
	Elf64_Half	vd_cnt;		/* no. of associated aux entries */
	Elf64_Word	vd_hash;	/* version name hash value */
	Elf64_Word	vd_aux;		/* no. of bytes from start of this */
					/*	verdef to verdaux array */
	Elf64_Word	vd_next;	/* no. of bytes from start of this */
} Elf64_Verdef;				/*	verdef to next verdef entry */

typedef struct {
	Elf64_Word	vda_name;	/* first element defines the version */
					/*	name. Additional entries */
					/*	define dependency names. */
	Elf64_Word	vda_next;	/* no. of bytes from start of this */
} Elf64_Verdaux;			/*	verdaux to next verdaux entry */

typedef struct {
	Elf64_Half	vn_version;	/* this structures version revision */
	Elf64_Half	vn_cnt;		/* no. of associated aux entries */
	Elf64_Word	vn_file;	/* name of needed dependency (file) */
	Elf64_Word	vn_aux;		/* no. of bytes from start of this */
					/*	verneed to vernaux array */
	Elf64_Word	vn_next;	/* no. of bytes from start of this */
} Elf64_Verneed;			/*	verneed to next verneed entry */

typedef struct {
	Elf64_Word	vna_hash;	/* version name hash value */
	Elf64_Half	vna_flags;	/* version information */
	Elf64_Half	vna_other;
	Elf64_Word	vna_name;	/* version name */
	Elf64_Word	vna_next;	/* no. of bytes from start of this */
} Elf64_Vernaux;			/*	vernaux to next vernaux entry */

typedef	Elf64_Half	Elf64_Versym;

typedef struct {
	Elf64_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf64_Half	si_flags;	/* per symbol flags */
} Elf64_Syminfo;
#endif	/* (defined(_LP64) || ((__STDC__ - 0 == 0) ... */

#endif

/*
 * Versym symbol index values.  Values greater than VER_NDX_GLOBAL
 * and less then VER_NDX_LORESERVE associate symbols with user
 * specified version descriptors.
 */
#define	VER_NDX_LOCAL		0	/* symbol is local */
#define	VER_NDX_GLOBAL		1	/* symbol is global and assigned to */
					/*	the base version */
#define	VER_NDX_LORESERVE	0xff00	/* beginning of RESERVED entries */
#define	VER_NDX_ELIMINATE	0xff01	/* symbol is to be eliminated */

/*
 * Verdef and Verneed (via Veraux) flags values.
 */
#define	VER_FLG_BASE		0x1	/* version definition of file itself */
#define	VER_FLG_WEAK		0x2	/* weak version identifier */

/*
 * Verdef version values.
 */
#define	VER_DEF_NONE		0	/* Ver_def version */
#define	VER_DEF_CURRENT		1
#define	VER_DEF_NUM		2

/*
 * Verneed version values.
 */
#define	VER_NEED_NONE		0	/* Ver_need version */
#define	VER_NEED_CURRENT	1
#define	VER_NEED_NUM		2


/*
 * Syminfo flag values
 */
#define	SYMINFO_FLG_DIRECT	0x0001	/* direct bound symbol */
#define	SYMINFO_FLG_PASSTHRU	0x0002	/* pass-thru symbol for translator */
#define	SYMINFO_FLG_COPY	0x0004	/* symbol is a copy-reloc */
#define	SYMINFO_FLG_LAZYLOAD	0x0008	/* symbol bound to object to be lazy */
					/*	loaded */

/*
 * key values for Syminfo.si_boundto
 */
#define	SYMINFO_BT_SELF		0xffff	/* symbol bound to self */
#define	SYMINFO_BT_PARENT	0xfffe	/* symbol bound to parent */
#define	SYMINFO_BT_LOWRESERVE	0xff00	/* beginning of reserved entries */

/*
 * Syminfo version values.
 */
#define	SYMINFO_NONE		0	/* Syminfo version */
#define	SYMINFO_CURRENT		1
#define	SYMINFO_NUM		2


/*
 * Public structure defined and maintained within the run-time linker
 */
#ifndef	_ASM

typedef struct link_map	Link_map;

struct link_map {
	unsigned long	l_addr;		/* address at which object is mapped */
	char *		l_name;		/* full name of loaded object */
#ifdef _LP64
	Elf64_Dyn *	l_ld;		/* dynamic structure of object */
#else
	Elf32_Dyn *	l_ld;		/* dynamic structure of object */
#endif
	Link_map *	l_next;		/* next link object */
	Link_map *	l_prev;		/* previous link object */
	char *		l_refname;	/* filters reference name */
};

#ifdef _SYSCALL32
typedef struct link_map32 Link_map32;

struct link_map32 {
	Elf32_Word	l_addr;
	Elf32_Addr	l_name;
	Elf32_Addr	l_ld;
	Elf32_Addr	l_next;
	Elf32_Addr	l_prev;
	Elf32_Addr	l_refname;
};
#endif

typedef enum {
	RT_CONSISTENT,
	RT_ADD,
	RT_DELETE
} r_state_e;

typedef enum {
	RD_FL_NONE = 0,		/* no flags */
	RD_FL_ODBG = (1<<0),	/* old style debugger present */
	RD_FL_DBG = (1<<1)	/* debugging enabled */
} rd_flags_e;



/*
 * Debugging events enabled inside of the run-time linker.  To
 * access these events see the librtld_db interface.
 */
typedef enum {
	RD_NONE = 0,		/* no event */
	RD_PREINIT,		/* the Initial rendezvous before .init */
	RD_POSTINIT,		/* the Second rendezvous after .init */
	RD_DLACTIVITY		/* a dlopen or dlclose has happened */
} rd_event_e;

struct r_debug {
	int		r_version;	/* debugging info version no. */
	Link_map *	r_map;		/* address of link_map */
	unsigned long	r_brk;		/* address of update routine */
	r_state_e	r_state;
	unsigned long	r_ldbase;	/* base addr of ld.so */
	Link_map *	r_ldsomap;	/* address of ld.so.1's link map */
	rd_event_e	r_rdevent;	/* debug event */
	rd_flags_e	r_flags;	/* misc flags. */
};

#ifdef _SYSCALL32
struct r_debug32 {
	Elf32_Word	r_version;	/* debugging info version no. */
	Elf32_Addr	r_map;		/* address of link_map */
	Elf32_Word	r_brk;		/* address of update routine */
	r_state_e	r_state;
	Elf32_Word	r_ldbase;	/* base addr of ld.so */
	Elf32_Addr	r_ldsomap;	/* address of ld.so.1's link map */
	rd_event_e	r_rdevent;	/* debug event */
	rd_flags_e	r_flags;	/* misc flags. */
};
#endif


#define	R_DEBUG_VERSION	2		/* current r_debug version */
#endif	/* _ASM */

/*
 * Attribute/value structures used to bootstrap ELF-based dynamic linker.
 */
#ifndef	_ASM
typedef struct {
	Elf32_Sword eb_tag;		/* what this one is */
	union {				/* possible values */
		Elf32_Word eb_val;
		Elf32_Addr eb_ptr;
		Elf32_Off  eb_off;
	} eb_un;
} Elf32_Boot;

#if (defined(_LP64) || ((__STDC__ - 0 == 0) && (!defined(_NO_LONGLONG))))
typedef struct {
	Elf64_Xword eb_tag;		/* what this one is */
	union {				/* possible values */
		Elf64_Xword eb_val;
		Elf64_Addr eb_ptr;
		Elf64_Off eb_off;
	} eb_un;
} Elf64_Boot;
#endif	/* (defined(_LP64) || ((__STDC__ - 0 == 0) ... */
#endif

/*
 * Attributes
 */
#define	EB_NULL		0		/* (void) last entry */
#define	EB_DYNAMIC	1		/* (*) dynamic structure of subject */
#define	EB_LDSO_BASE	2		/* (caddr_t) base address of ld.so */
#define	EB_ARGV		3		/* (caddr_t) argument vector */
#define	EB_ENVP		4		/* (char **) environment strings */
#define	EB_AUXV		5		/* (auxv_t *) auxiliary vector */
#define	EB_DEVZERO	6		/* (int) fd for /dev/zero */
#define	EB_PAGESIZE	7		/* (int) page size */
#define	EB_MAX		8		/* number of "EBs" */


#ifndef	_ASM

#ifdef __STDC__

/*
 * Concurrency communication structure for threads library, and libc callbacks.
 */
extern void	_ld_concurrency(void *);
extern void	_ld_libc(void *);
#else /* __STDC__ */
extern void	_ld_concurrency();
extern void	_ld_libc();
#endif /* __STDC__ */
#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LINK_H */
