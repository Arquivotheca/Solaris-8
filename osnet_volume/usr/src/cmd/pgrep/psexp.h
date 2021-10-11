/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PSEXP_H
#define	_PSEXP_H

#pragma ident	"@(#)psexp.h	1.1	97/12/08 SMI"

#include <sys/types.h>
#include <procfs.h>
#include <regex.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include "idtab.h"

#define	PSEXP_PSARGS	0x1	/* Match against psargs rather than fname */
#define	PSEXP_EXACT	0x2	/* Match must be exact (entire string) */

typedef struct psexp {
	idtab_t ps_euids;	/* Table of effective uids to match */
	idtab_t ps_ruids;	/* Table of real uids to match */
	idtab_t ps_rgids;	/* Table of real gids to match */
	idtab_t ps_ppids;	/* Table of parent process-ids to match */
	idtab_t ps_pgids;	/* Table of process group-ids to match */
	idtab_t ps_sids;	/* Table of process session-ids to match */
	idtab_t ps_ttys;	/* Table of tty dev_t values to match */
	const char *ps_pat;	/* Uncompiled fname/psargs regexp pattern */
	regex_t ps_reg;		/* Compiled fname/psargs regexp */
} psexp_t;

extern void psexp_create(psexp_t *);
extern void psexp_destroy(psexp_t *);
extern int psexp_compile(psexp_t *);
extern int psexp_match(psexp_t *, psinfo_t *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _PSEXP_H */
