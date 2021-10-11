/*
 * Copyright (c) 1991-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROMIMPL_H
#define	_SYS_PROMIMPL_H

#pragma ident	"@(#)promimpl.h	1.26	98/07/10 SMI"

/*
 * promif implementation header file; private to promif implementation.
 *
 * These interfaces are not 'exported' in the same sense as
 * those described in promif.h
 *
 * Used so that the kernel and other stand-alones (eg boot)
 * don't have to directly reference the prom (of which there
 * are now several completely different variants).
 */

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/openprom.h>
#include <sys/comvec.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int obp_romvec_version;

/*
 * XXX for chatty stuff in prom_stdinpath/prom_stdoutpath until proposed
 * changes from romvec pathnames to root node properties for stdin/stdout
 * pathnames.
 */

/* #define	PROM_DEBUG_STDPATH	1 */

/*
 * Debugging macros for the promif functions.
 */

#define	PROMIF_DMSG_VERBOSE		2
#define	PROMIF_DMSG_NORMAL		1

extern int promif_debug;		/* externally patchable */

#define	PROMIF_DEBUG			/* define this to enable debugging */
#define	PROMIF_DEBUG_P1275		/* Debug 1275 client interface calls */

#ifdef PROMIF_DEBUG
#define	PROMIF_DPRINTF(args)				\
	if (promif_debug) { 				\
		if (promif_debug == PROMIF_DMSG_VERBOSE)	\
			prom_printf("file %s line %d: ", __FILE__, __LINE__); \
		prom_printf args;			\
	}
#else
#define	PROMIF_DPRINTF(args)
#endif /* PROMIF_DEBUG */


#define	prom_decode_int(v)	(v)

/*
 * minimum alignment required by prom
 */
#define	PROMIF_MIN_ALIGN	1

/*
 * Private utility routines (not exported as part of the interface)
 */

extern	char		*prom_strcpy(char *s1, char *s2);
extern	char		*prom_strncpy(char *s1, char *s2, size_t n);
extern	int		prom_strcmp(char *s1, char *s2);
extern	int		prom_strncmp(char *s1, char *s2, size_t n);
extern	int		prom_strlen(char *s);
extern	char		*prom_strrchr(char *s1, char c);
extern	char		*prom_strcat(char *s1, char *s2);
extern	char		*prom_strchr(const char *, int);

/*
 * IEEE 1275 Routines defined by each platform using IEEE 1275:
 */

extern	void		*p1275_cif_init(void *);
extern	int		p1275_cif_call(void *);

#if defined(PROM_32BIT_ADDRS)
/*
 * Client programs defining PROM_32BIT_ADDRS need to provide two
 * callbacks to allow the promif routines to allocate and free memory
 * allocated from the bottom 32-bits of the 64-bit address space.
 */
extern void		*promplat_alloc(size_t);
extern void		promplat_free(void *, size_t);
extern void		promplat_bcopy(const void *s1, void *s2, size_t n);
#endif

/*
 * More private globals
 */
extern	int		prom_aligned_allocator;
extern	union sunromvec *romp;		/* romvec cookie; private to promif */
extern	void		*p1275cif;	/* P1275 client interface cookie */

/*
 * Every call into the prom is wrappered with these calls so that
 * the caller can ensure that e.g. pre-emption is disabled
 * while we're in the firmware.  See 1109602.
 */
extern	void		(*promif_preprom)(void);
extern	void		(*promif_postprom)(void);

extern	void		(*promif_setprop_preprom)(void);
extern	void		(*promif_setprop_postprom)(void);

/*
 * The default allocator used in IEEE 1275 mode:
 */
extern	caddr_t		(*promif_allocator)(caddr_t, uint_t, uint_t);

/*
 * The prom interface uses this string internally for prefixing error
 * messages so that the "client" of the given instance of
 * promif can be identified e.g. "boot", "kadb" or "kernel".
 *
 * It is passed into the library via prom_init().
 */
extern	char		promif_clntname[];

/*
 * The routine called when all else fails (and there may be no firmware
 * interface at all!)
 */
extern	void		prom_fatal_error(const char *);

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PROMIMPL_H */
