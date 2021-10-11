
#ifndef lint
#pragma ident	"@(#)prompts.c	1.6	97/05/08 SMI"
#endif	lint

/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

/*
 * This file contains functions to prompt the user for various
 * disk characteristics.  By isolating these into functions,
 * we can guarantee that prompts, defaults, etc are identical.
 */
#include "global.h"
#include "prompts.h"
#include "io.h"
#include "param.h"
#include "startup.h"

#ifdef sparc
#include <sys/hdio.h>
#endif

/*
 * Prompt for number of cylinders
 */
int
get_ncyl()
{
	u_ioparam_t	ioparam;

	ioparam.io_bounds.lower = 1;
	ioparam.io_bounds.upper = MAX_CYLS;
	return (input(FIO_INT, "Enter number of data cylinders",
	    ':', &ioparam, (int *)NULL, DATA_INPUT));
}

/*
 * Prompt for number of alternate cylinders
 */
int
get_acyl(n_cyls)
	int		n_cyls;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 2;
	ioparam.io_bounds.upper = MAX_CYLS - n_cyls;
	deflt = 2;
	return (input(FIO_INT, "Enter number of alternate cylinders", ':',
	    &ioparam, &deflt, DATA_INPUT));
}

/*
 * Prompt for number of physical cylinders
 */
int
get_pcyl(n_cyls, a_cyls)
	int		n_cyls;
	int		a_cyls;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = n_cyls + a_cyls;
	ioparam.io_bounds.upper = MAX_CYLS;
	deflt = n_cyls + a_cyls;
	return (input(FIO_INT, "Enter number of physical cylinders", ':',
	    &ioparam, &deflt, DATA_INPUT));
}

/*
 * Prompt for number of heads
 */
int
get_nhead()
{
	u_ioparam_t	ioparam;

	ioparam.io_bounds.lower = 1;
	ioparam.io_bounds.upper = MAX_HEADS;
	return (input(FIO_INT, "Enter number of heads", ':',
	    &ioparam, (int *)NULL, DATA_INPUT));
}

/*
 * Prompt for number of physical heads
 */
int
get_phead(n_heads, options)
	int		n_heads;
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	if (SCSI) {
		ioparam.io_bounds.lower = n_heads;
		ioparam.io_bounds.upper = INFINITY;
		if (input(FIO_OPINT, "Enter physical number of heads",
				':', &ioparam, &deflt, DATA_INPUT)) {
			*options |= SUP_PHEAD;
			return (deflt);
		}
	}
	return (0);
}


/*
 * Prompt for number of sectors per track
 */
int
get_nsect()
{
	u_ioparam_t	ioparam;

	ioparam.io_bounds.lower = 1;
	ioparam.io_bounds.upper = MAX_SECTS;
	return (input(FIO_INT,
	    "Enter number of data sectors/track", ':',
	    &ioparam, (int *)NULL, DATA_INPUT));
}

/*
 * Prompt for number of physical sectors per track
 */
int
get_psect(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	if (SCSI) {
		ioparam.io_bounds.lower = 0;
		ioparam.io_bounds.upper = INFINITY;
		if (input(FIO_OPINT, "Enter number of physical sectors/track",
				':', &ioparam, &deflt, DATA_INPUT)) {
			*options |= SUP_PSECT;
			return (deflt);
		}
	}
	return (0);
}

/*
 * Prompt for bytes per track
 */
int
get_bpt(n_sects, options)
	int		n_sects;
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	if (SMD) {
		*options |= SUP_BPT;
		ioparam.io_bounds.lower = 1;
		ioparam.io_bounds.upper = INFINITY;
		deflt = n_sects * SECSIZE;
		return (input(FIO_INT, "Enter number of bytes/track",
				':', &ioparam, &deflt, DATA_INPUT));
	}

	return (0);
}

/*
 * Prompt for rpm
 */
int
get_rpm()
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = MIN_RPM;
	ioparam.io_bounds.upper = MAX_RPM;
	deflt = AVG_RPM;
	return (input(FIO_INT, "Enter rpm of drive", ':',
	    &ioparam, &deflt, DATA_INPUT));
}

/*
 * Prompt for formatting time
 */
int
get_fmt_time(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter format time", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_FMTTIME;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for cylinder skew
 */
int
get_cyl_skew(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter cylinder skew", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_CYLSKEW;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for track skew
 */
int
get_trk_skew(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter track skew", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_TRKSKEW;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for tracks per zone
 */
int
get_trks_zone(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter tracks per zone", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_TRKS_ZONE;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for alternate tracks
 */
int
get_atrks(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter alternate tracks", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_ATRKS;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for alternate sectors
 */
int
get_asect(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter alternate sectors", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_ASECT;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for cache setting
 */
int
get_cache(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = 0xff;
	if (input(FIO_OPINT, "Enter cache control", ':',
			&ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_CACHE;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for prefetch threshold
 */
int
get_threshold(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter prefetch threshold",
			':', &ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_PREFETCH;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for minimum prefetch
 */
int
get_min_prefetch(options)
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = 0;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter minimum prefetch",
			':', &ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_CACHE_MIN;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for maximum prefetch
 */
int
get_max_prefetch(min_prefetch, options)
	int		min_prefetch;
	u_long		*options;
{
	u_ioparam_t	ioparam;
	int		deflt;

	ioparam.io_bounds.lower = min_prefetch;
	ioparam.io_bounds.upper = INFINITY;
	if (input(FIO_OPINT, "Enter maximum prefetch",
			':', &ioparam, &deflt, DATA_INPUT)) {
		*options |= SUP_CACHE_MAX;
		return (deflt);
	}
	return (0);
}

/*
 * Prompt for bytes per sector
 */
int
get_bps()
{
	u_ioparam_t	ioparam;
	int		deflt;

	if (cur_ctype->ctype_flags & CF_SMD_DEFS) {
		ioparam.io_bounds.lower = MIN_BPS;
		ioparam.io_bounds.upper = MAX_BPS;
		deflt = AVG_BPS;
		return (input(FIO_INT, "Enter bytes per sector",
			':', &ioparam, &deflt, DATA_INPUT));
	}

	return (0);
}

/*
 * Prompt for ascii label
 */
char *
get_asciilabel()
{
	return ((char *)input(FIO_OSTR,
	    "Enter disk type name (remember quotes)", ':',
	    (u_ioparam_t *)NULL, (int *)NULL, DATA_INPUT));
}
