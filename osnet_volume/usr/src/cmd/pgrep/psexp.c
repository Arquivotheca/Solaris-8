/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)psexp.c	1.1	97/12/08 SMI"

#include <string.h>
#include <stdlib.h>
#include <alloca.h>

#include "utils.h"
#include "idtab.h"
#include "psexp.h"

void
psexp_create(psexp_t *psexp)
{
	idtab_create(&psexp->ps_euids);
	idtab_create(&psexp->ps_ruids);
	idtab_create(&psexp->ps_rgids);
	idtab_create(&psexp->ps_ppids);
	idtab_create(&psexp->ps_pgids);
	idtab_create(&psexp->ps_sids);
	idtab_create(&psexp->ps_ttys);

	psexp->ps_pat = NULL;
}

void
psexp_destroy(psexp_t *psexp)
{
	idtab_destroy(&psexp->ps_euids);
	idtab_destroy(&psexp->ps_ruids);
	idtab_destroy(&psexp->ps_rgids);
	idtab_destroy(&psexp->ps_ppids);
	idtab_destroy(&psexp->ps_pgids);
	idtab_destroy(&psexp->ps_sids);
	idtab_destroy(&psexp->ps_ttys);

	if (psexp->ps_pat)
		regfree(&psexp->ps_reg);
}

int
psexp_compile(psexp_t *psexp)
{
	size_t nbytes;
	char *buf;
	int err;

	idtab_sort(&psexp->ps_euids);
	idtab_sort(&psexp->ps_ruids);
	idtab_sort(&psexp->ps_rgids);
	idtab_sort(&psexp->ps_ppids);
	idtab_sort(&psexp->ps_pgids);
	idtab_sort(&psexp->ps_sids);
	idtab_sort(&psexp->ps_ttys);

	if (psexp->ps_pat != NULL) {
		if ((err = regcomp(&psexp->ps_reg, psexp->ps_pat,
		    REG_EXTENDED)) != 0) {

			nbytes = regerror(err, &psexp->ps_reg, NULL, 0);
			buf = alloca(nbytes + 1);
			(void) regerror(err, &psexp->ps_reg, buf, nbytes);
			(void) strcat(buf, "\n");
			warn(buf);
			return (-1);
		}
	}

	return (0);
}

#define	NOMATCH(__f1, __f2) \
	psexp->__f1.id_data && !idtab_search(&psexp->__f1, psinfo->__f2)

int
psexp_match(psexp_t *psexp, psinfo_t *psinfo, int flags)
{
	regmatch_t pmatch;
	const char *s;

	if (NOMATCH(ps_euids, pr_euid))
		return (0);
	if (NOMATCH(ps_ruids, pr_uid))
		return (0);
	if (NOMATCH(ps_rgids, pr_gid))
		return (0);
	if (NOMATCH(ps_ppids, pr_ppid))
		return (0);
	if (NOMATCH(ps_pgids, pr_pgid))
		return (0);
	if (NOMATCH(ps_sids, pr_sid))
		return (0);
	if (NOMATCH(ps_ttys, pr_ttydev))
		return (0);

	if (psexp->ps_pat != NULL) {
		s = (flags & PSEXP_PSARGS) ?
		    psinfo->pr_psargs : psinfo->pr_fname;

		if (regexec(&psexp->ps_reg, s, 1, &pmatch, 0) != 0)
			return (0);

		if ((flags & PSEXP_EXACT) &&
		    (pmatch.rm_so != 0 || s[pmatch.rm_eo] != '\0'))
			return (0);
	}

	return (1);
}
