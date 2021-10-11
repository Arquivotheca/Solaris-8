/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef __ACTION_H
#define	__ACTION_H

#pragma ident	"@(#)action.h	1.5	94/08/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct reap {
	struct q	q;
	u_int		r_act;
	struct vol	*r_v;
	pid_t		r_pid;
	char		*r_hint;
};

extern struct q reapq;

typedef struct actprog {
	char	*ap_prog;
	char	*ap_matched;
	char	**ap_args;
	uid_t	ap_uid;
	gid_t	ap_gid;
	u_int	ap_line;
	u_int	ap_maptty;
} actprog_t;

#define	ACT_INSERT	1
#define	ACT_EJECT	2
#define	ACT_NOTIFY	3
#define	ACT_ERROR	4

int	action(u_int, struct vol *);

extern char	*actnames[];

#define	MAXARGC		100

#ifdef	__cplusplus
}
#endif

#endif /* __ACTION_H */
