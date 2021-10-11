/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NFSLOGTAB_H
#define	_NFSLOGTAB_H

#pragma ident	"@(#)nfslogtab.h	1.4	99/08/24 SMI"

/*
 * Defines logtab
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define	NFSLOGTAB "/etc/nfs/nfslogtab"
#define	MAXBUFSIZE 4096
#define	LES_INACTIVE 0		/* entry is inactive */
#define	LES_ACTIVE 1		/* entry is active */

struct logtab_ent {
	char *le_buffer;
	char *le_path;
	char *le_tag;
	int le_state;
};

struct logtab_ent_list {
	struct logtab_ent *lel_le;
	struct logtab_ent_list *lel_next;
};

int logtab_getent(FILE *, struct logtab_ent **);
int logtab_putent(FILE *, struct logtab_ent *);
int logtab_rement(FILE *, char *, char *, char *, int);
void logtab_ent_free(struct logtab_ent *lep);
int logtab_deactivate(FILE *, char *, char *, char *);
int logtab_deactivate_after_boot(FILE *);

#ifdef __cplusplus
}
#endif

#endif /* _NFSLOGTAB_H */
