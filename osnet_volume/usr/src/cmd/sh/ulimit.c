/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ulimit.c	1.10	97/09/28 SMI"

/*
 * ulimit builtin
 */

#include <sys/resource.h>
#include <stdlib.h>
#include "defs.h"

/*
 * order is important in this table! it is indexed by resource ID.
 */

static struct rlimtab {
	char	*name;
	char	*scale;
	rlim_t	divisor;
} rlimtab[] = {
/* RLIMIT_CPU	*/	"time",		"seconds",	1,
/* RLIMIT_FSIZE */	"file",		"blocks",	512,
/* RLIMIT_DATA	*/	"data",		"kbytes",	1024,
/* RLIMIT_STACK */	"stack",	"kbytes",	1024,
/* RLIMIT_CORE	*/	"coredump",	"blocks",	512,
/* RLIMIT_NOFILE */	"nofiles",	"descriptors",	1,
/* RLIMIT_VMEM */	"memory",	"kbytes",	1024,
};

void
sysulimit(int argc, char **argv)
{
	extern int opterr, optind;
	int savopterr, savoptind, savsp;
	char *savoptarg;
	char *args;
	int hard, soft, cnt, c, res;
	rlim_t limit, new_limit;
	struct rlimit rlimit;
	char resources[RLIM_NLIMITS];

	for (res = 0;  res < RLIM_NLIMITS; res++) {
		resources[res] = 0;
	}

	savoptind = optind;
	savopterr = opterr;
	savsp = _sp;
	savoptarg = optarg;
	optind = 1;
	_sp = 1;
	opterr = 0;
	hard = 0;
	soft = 0;
	cnt = 0;

	while ((c = getopt(argc, argv, "HSacdfnstv")) != -1) {
		switch (c) {
		case 'S':
			soft++;
			continue;
		case 'H':
			hard++;
			continue;
		case 'a':
			for (res = 0;  res < RLIM_NLIMITS; res++) {
				resources[res]++;
			}
			cnt = RLIM_NLIMITS;
			continue;
		case 'c':
			res = RLIMIT_CORE;
			break;
		case 'd':
			res = RLIMIT_DATA;
			break;
		case 'f':
			res = RLIMIT_FSIZE;
			break;
		case 'n':
			res = RLIMIT_NOFILE;
			break;
		case 's':
			res = RLIMIT_STACK;
			break;
		case 't':
			res = RLIMIT_CPU;
			break;
		case 'v':
			res = RLIMIT_VMEM;
			break;
		case '?':
			failure(usage, ulimuse);
			goto err;
		}
		resources[res]++;
		cnt++;
	}

	if (cnt == 0) {
		resources[res = RLIMIT_FSIZE]++;
		cnt++;
	}

	/*
	 * if out of arguments, then print the specified resources
	 */

	if (optind == argc) {
		if (!hard && !soft) {
			soft++;
		}
		for (res = 0; res < RLIM_NLIMITS; res++) {
			if (resources[res] == 0) {
				continue;
			}
			if (getrlimit(res, &rlimit) < 0) {
				continue;
			}
			if (cnt > 1) {
				prs_buff(rlimtab[res].name);
				prc_buff('(');
				prs_buff(rlimtab[res].scale);
				prc_buff(')');
				prc_buff(' ');
			}
			if (soft) {
				if (rlimit.rlim_cur == RLIM_INFINITY) {
					prs_buff("unlimited");
				} else  {
					prull_buff(rlimit.rlim_cur /
					    rlimtab[res].divisor);
				}
			}
			if (hard && soft) {
				prc_buff(':');
			}
			if (hard) {
				if (rlimit.rlim_max == RLIM_INFINITY) {
					prs_buff("unlimited");
				} else  {
					prull_buff(rlimit.rlim_max /
					    rlimtab[res].divisor);
				}
			}
			prc_buff('\n');
		}
		goto err;
	}

	if (cnt > 1 || optind + 1 != argc) {
		failure(usage, ulimuse);
		goto err;
	}

	if (eq(argv[optind], "unlimited")) {
		limit = RLIM_INFINITY;
	} else {
		args = argv[optind];

		new_limit = limit = 0;
		do {
			if (*args < '0' || *args > '9') {
				failure(argv[0], badulimit);
				goto err;
			}
			/* Check for overflow! */
			new_limit = (limit * 10) + (*args - '0');
			if (new_limit >= limit) {
				limit = new_limit;
			} else {
				failure(argv[0], badulimit);
				goto err;
			}
		} while (*++args);

		/* Check for overflow! */
		new_limit = limit * rlimtab[res].divisor;
		if (new_limit >= limit) {
			limit = new_limit;
		} else {
			failure(argv[0], badulimit);
			goto err;
		}
	}

	if (getrlimit(res, &rlimit) < 0) {
		failure(argv[0], badulimit);
		goto err;
	}

	if (!hard && !soft) {
		hard++;
		soft++;
	}
	if (hard) {
		rlimit.rlim_max = limit;
	}
	if (soft) {
		rlimit.rlim_cur = limit;
	}

	if (setrlimit(res, &rlimit) < 0) {
		failure(argv[0], badulimit);
	}

err:
	optind = savoptind;
	opterr = savopterr;
	_sp = savsp;
	optarg = savoptarg;
}
