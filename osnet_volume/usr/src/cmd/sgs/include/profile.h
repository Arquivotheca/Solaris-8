/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_PROFILE_H
#define	_PROFILE_H

#pragma ident	"@(#)profile.h	1.19	99/01/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

#include <sys/types.h>
#include <synch.h>
#include <link.h>

/*
 * The profile buffer created by ld.so.1 consists of 3 sections; the header,
 * the profil(2) buffer, and an array of call graph arc structures.
 */

typedef struct l_hdr {			/* Linker profile buffer header */
	unsigned int	hd_magic;	/* identifier for file */
	unsigned int	hd_version;	/* version for rtld prof file */
	lwp_mutex_t	hd_mutex;	/* Provides for process locking */
	caddr_t		hd_hpc;		/* Relative high pc address */
	unsigned int	hd_psize;	/* Size of profil(2) buffer */
	unsigned int	hd_fsize;	/* Size of file */
	unsigned int	hd_ncndx;	/* Next (and last) index into */
	unsigned int	hd_lcndx;	/*	call graph arc structure */
} L_hdr;


/*
 * The *64 structs are for gprof, as a 32-bit program,
 * to read 64-bit profiles correctly.
 */

typedef struct l_hdr64 {		/* Linker profile buffer header */
	unsigned int	hd_magic;	/* identifier for file */
	unsigned int	hd_version;	/* version for rtld prof file */
	lwp_mutex_t	hd_mutex;	/* Provides for process locking */
	u_longlong_t	hd_hpc;		/* Relative high pc address */
	unsigned int	hd_psize;	/* Size of profil(2) buffer */
	unsigned int	hd_fsize;	/* Size of file */
	unsigned int	hd_ncndx;	/* Next (and last) index into */
	unsigned int	hd_lcndx;	/*	call graph arc structure */
} L_hdr64;



typedef struct l_cgarc {		/* Linker call graph arc entry */
	caddr_t		cg_from;	/* Source of call */
	caddr_t		cg_to;		/* Destination of call */
	unsigned int	cg_count;	/* Instance count */
	unsigned int	cg_next;	/* Link index for multiple sources */
} L_cgarc;


typedef struct l_cgarc64 {		/* Linker call graph arc entry */
	u_longlong_t	cg_from;	/* Source of call */
	u_longlong_t	cg_to;		/* Destination of call */
	unsigned int	cg_count;	/* Instance count */
	unsigned int	cg_next;	/* Link index for multiple sources */
} L_cgarc64;


/*
 * Snapshots of this profile buffer are taken by `ldmon' and packaged into
 * a gmon.out file appropriate for analysis by gprof(1).  This gmon file
 * consists of three sections (taken from gmon.h); a header, a profil(2)
 * buffer, and an array of call graph arc structures.
 */

typedef struct m_hdr {			/* Monitor profile buffer header */
	char *		hd_lpc;		/* Low pc value */
	char *		hd_hpc;		/* High pc value */
	int		hd_off;		/* Offset into call graph array */
} M_hdr;

typedef struct m_hdr64 {		/* Monitor profile buffer header */
	u_longlong_t	hd_lpc;		/* Low pc value */
	u_longlong_t	hd_hpc;		/* High pc value */
	int		hd_off;		/* Offset into call graph array */
} M_hdr64;

typedef struct m_cgarc {		/* Monitor call graph arc entry */
	unsigned int	cg_from;	/* Source of call */
	unsigned int	cg_to;		/* Destination of call */
	int		cg_count;	/* Instance count */
} M_cgarc;

typedef struct m_cnt {			/* Prof(1) function count structure */
	char *		fc_fnpc;	/* Called functions address */
	int		fc_mcnt;	/* Instance count */
} M_cnt;


#define	PROF_LIBRARY	"ldprofile.so.1"

/*
 * Generic defines for creating profiled output buffer.
 */

#define	PRF_BARSIZE	2		/* No. of program bytes that */
					/* correspond to each histogram */
					/* bar in the profil(2) buffer */
#define	PRF_SCALE	0x8000		/* Scale to provide above */
					/* histogram correspondence */
#define	PRF_CGNUMB	256		/* Size of call graph extension */
#define	PRF_CGINIT	2		/* Initial symbol blocks to allocate */
					/*	for the call graph structure */
#define	PRF_OUTADDR	(caddr_t)-1	/* Function addresses outside of */
					/*	the range being monitored */
#define	PRF_OUTADDR64	(u_longlong_t)-1	/* Function addresses outside */
					/*	of the range being monitored */
#define	PRF_UNKNOWN	(caddr_t)-2	/* Unknown function address */

#define	PRF_ROUNDUP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define	PRF_ROUNDWN(x, a) ((x) & ~((a) - 1))

#define	PRF_MAGIC	0xffffffff	/* unique number to differentiate */
					/* profiled file from gmon.out for */
					/* gprof */
#define	PRF_VERSION	0x1		/* current PROF file version */
#define	PRF_VERSION_64	0x2		/* 64-bit current PROF file version */


/*
 * Related data and function definitions.
 */

extern	int		profile_rtld;		/* Rtld is being profiled */

extern	uintptr_t (*	p_cg_interp)(int, caddr_t, caddr_t);

#ifdef	PRF_RTLD
/*
 * Define MCOUNT macros that allow functions within ld.so.1 to collect
 * call count information.  Each function must supply a unique index.
 */

#define	PRF_MCOUNT(index, func) \
	if (profile_rtld) \
		(void) p_cg_interp(index, (caddr_t)caller(), (caddr_t)&func);
#else
#define	PRF_MCOUNT(index, func)
#endif

#endif

#ifdef	__cplusplus
}
#endif

#endif /* _PROFILE_H */
