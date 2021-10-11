/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

#ifndef _SYS_PROCESSOR_H
#define	_SYS_PROCESSOR_H

#pragma ident	"@(#)processor.h	1.6	98/02/06 SMI"

#include <sys/types.h>
#include <sys/procset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for p_online and processor_info system calls.
 */

/*
 * Type for processor name (CPU number).
 */
typedef	int	processorid_t;

/*
 * Flags and return values for p_online(2), and pi_state for processor_info(2).
 */
#define	P_OFFLINE	1	/* processor is offline, as quiet as possible */
#define	P_ONLINE	2	/* processor online */
#define	P_STATUS	3	/* value passed to p_online to request status */
#define	P_BAD		4	/* unused so far but defined by USL */
#define	P_POWEROFF	5	/* processor is powered off */
#define	P_NOINTR	6	/* processor online, but no I/O interrupts */

/*
 * Structure filled in by processor_info(2).
 *
 * The string fields are guaranteed to contain a NULL.
 *
 * The pi_fputypes field contains a (possibly empty) comma-separated
 * list of floating point identifier strings.
 */
#define	PI_TYPELEN	16	/* max size of CPU type string */
#define	PI_FPUTYPE	32	/* max size of FPU types string */

typedef struct {
	int	pi_state;			/* P_ONLINE or P_OFFLINE */
						/* or P_POWEROFF */
	char	pi_processor_type[PI_TYPELEN];	/* ASCII CPU type */
	char	pi_fputypes[PI_FPUTYPE];	/* ASCII FPU types */
	int	pi_clock;			/* CPU clock freq in MHz */
} processor_info_t;


/*
 * Binding values for processor_bind(2).
 */
#define	PBIND_NONE	-1	/* LWP/thread is not bound */
#define	PBIND_QUERY	-2	/* don't set, just return the binding */

/*
 * User-level system call interface prototypes.
 */
#ifndef _KERNEL
#ifdef __STDC__

extern int	p_online(processorid_t processorid, int flag);
extern int	processor_info(processorid_t processorid,
		    processor_info_t *infop);
extern int	processor_bind(idtype_t idtype, id_t id,
		    processorid_t processorid, processorid_t *obind);

#else

extern int	p_online();
extern int	processor_info();
extern int	processor_bind();

#endif /* __STDC__ */
#endif /* ! _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PROCESSOR_H */
