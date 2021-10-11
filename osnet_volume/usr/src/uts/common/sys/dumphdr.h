/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DUMPHDR_H
#define	_SYS_DUMPHDR_H

#pragma ident	"@(#)dumphdr.h	1.13	99/12/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/log.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The dump header describes the contents of a crash dump.  Two headers
 * are written out: one at the beginning of the dump, and the other at
 * the very end of the dump device.  The terminal header is at a known
 * location (end of device) so we can always find it.  The initial header
 * is redundant, but helps savecore(1M) determine whether the dump has been
 * overwritten by swap activity.  See dumpadm(1M) for dump configuration.
 */
#define	DUMP_MAGIC	0xdefec8edU		/* dump magic number */
#define	DUMP_VERSION	9			/* version of this dumphdr */
#define	DUMP_WORDSIZE	(sizeof (long) * NBBY)	/* word size (32 or 64) */
#define	DUMP_PANICSIZE	200			/* Max panic string copied */
#define	DUMP_COMPRESS_RATIO	2		/* conservative; usually 2.5+ */
#define	DUMP_OFFSET	65536			/* pad at start/end of dev */
#define	DUMP_LOGSIZE	(2 * LOG_HIWAT)		/* /dev/log message save area */

typedef struct dumphdr {
	uint32_t dump_magic;		/* magic number */
	uint32_t dump_version;		/* version number */
	uint32_t dump_flags;		/* flags; see below */
	uint32_t dump_wordsize;		/* 32 or 64 */
	offset_t dump_start;		/* starting offset on dump device */
	offset_t dump_ksyms;		/* offset of compressed symbol table */
	offset_t dump_pfn;		/* offset of pfn table for all pages */
	offset_t dump_map;		/* offset of page translation map */
	offset_t dump_data;		/* offset of actual dump data */
	struct utsname dump_utsname;	/* copy of utsname structure */
	char	dump_platform[SYS_NMLN]; /* platform name (uname -i) */
	char	dump_panicstring[DUMP_PANICSIZE]; /* copy of panicstr */
	time_t	dump_crashtime;		/* time of crash */
	long	dump_pageshift;		/* log2(pagesize) */
	long	dump_pagesize;		/* pagesize */
	long	dump_hashmask;		/* page translation hash mask */
	long	dump_nvtop;		/* number of vtop table entries */
	pgcnt_t	dump_npages;		/* number of data pages */
	size_t	dump_ksyms_size;	/* kernel symbol table size */
	size_t	dump_ksyms_csize;	/* compressed symbol table size */
} dumphdr_t;

/*
 * Values for dump_flags
 */
#define	DF_VALID	0x00000001	/* Dump is valid (savecore clears) */
#define	DF_COMPLETE	0x00000002	/* All pages present as configured */
#define	DF_LIVE		0x00000004	/* Dump was taken on a live system */

/*
 * Dump translation map hash table entry.
 */
typedef struct dump_map {
	offset_t	dm_first;
	offset_t	dm_next;
	offset_t	dm_data;
	struct as	*dm_as;
	uintptr_t	dm_va;
} dump_map_t;

/*
 * Dump translation map hash function.
 */
#define	DUMP_HASH(dhp, as, va)	\
	((((uintptr_t)(as) >> 3) + ((va) >> (dhp)->dump_pageshift)) & \
	(dhp)->dump_hashmask)

#ifdef _KERNEL

extern kmutex_t dump_lock;
extern struct vnode *dumpvp;
extern u_offset_t dumpvp_size;
extern struct dumphdr *dumphdr;
extern int dump_conflags;
extern char *dumppath;
extern int dump_timeout;
extern int dump_timeleft;
extern int sync_timeleft;
extern int deadman_sync_timeleft;
extern int sync_aborted;

extern int dumpinit(struct vnode *, char *, int);
extern void dumpfini(void);
extern void dump_resize(void);
extern void dump_addpage(struct as *, void *, pfn_t);
extern void dumpsys(void);
extern void dump_messages(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DUMPHDR_H */
