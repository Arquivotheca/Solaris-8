/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)if.c	1.4	96/03/23 SMI"

/* if/else/elseif/endif processing */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/salib.h>

extern int expr(int argc, char *argv[]);

struct if_tbl if_tbl[IFTBLSIZ]; /* if cmd nesting table */
int if_tbl_x = -1;		/* current index into if_tbl */

/* if_cmd() - implements if command */

void
if_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	if (if_tbl_x >= (IFTBLSIZ-1)) {
		printf("boot: if: too many nested ifs\n");
		return;
	}
	++if_tbl_x;
	if_tbl[if_tbl_x].is_true = if_tbl[if_tbl_x].was_true =
		expr(argc - 1, &argv[1]);
	if_tbl[if_tbl_x].count = 0;
}

/* else_cmd() - implements else command */

/*ARGSUSED*/
void
else_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	if (if_tbl_x < 0) {
		printf("boot: else: no preceding if\n");
		return;
	}

	if (if_tbl[if_tbl_x].was_true)
		if_tbl[if_tbl_x].is_true = 0;
	else
		if_tbl[if_tbl_x].is_true = if_tbl[if_tbl_x].was_true = 1;
}

/* elseif_cmd() - implements elseif command */

void
elseif_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	if (if_tbl_x < 0) {
		printf("boot: elseif: no preceding if\n");
		return;
	}

	if (if_tbl[if_tbl_x].was_true)
		if_tbl[if_tbl_x].is_true = 0;
	else
		if_tbl[if_tbl_x].is_true = if_tbl[if_tbl_x].was_true =
			expr(argc - 1, &argv[1]);
}

/* endif_cmd() - implements endif command */

/*ARGSUSED*/
void
endif_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	if (if_tbl_x < 0) {
		printf("boot: endif: no preceding if\n");
		return;
	}
	--if_tbl_x;
}

/*
 * if_yes() - decide whether to execute the next command
 *	    - handles embedded if/else/elseif/endif
 */
int
if_yes(argp)
	struct arg *argp;
{
	char    *arg_zero;

	/*
	printf("if_yes(): if_tbl_x = %d", if_tbl_x);
	if (if_tbl_x < 0)
		printf("\n");
	else
		printf("  is_true = %d  was_ true = %d  count = %d\n",
			if_tbl[if_tbl_x].is_true,
			if_tbl[if_tbl_x].was_true,
			if_tbl[if_tbl_x].count);
	*/

	arg_zero = argp->argv[0];

	/* if no active ifs or current block is true, execute command */
	if (if_tbl_x < 0 || if_tbl[if_tbl_x].is_true)
		return (1);

	/* current block is not true */

	/* count embedded non-executed ifs */
	if (strcmp(arg_zero, "if") == 0) {
		++if_tbl[if_tbl_x].count;
		return (0);
	}

	/* handle endif */
	if (strcmp(arg_zero, "endif") == 0) {
		if (if_tbl[if_tbl_x].count > 0) {
			--if_tbl[if_tbl_x].count;
			return (0);
		}
		else
			return (1);
	}

	/* handle elseif and else */
	if ((strcmp(arg_zero, "elseif") == 0) ||
	    (strcmp(arg_zero, "else") == 0)) {
		if (if_tbl[if_tbl_x].count > 0)
			return (0);
		else
			return (1);
	}

	return (0);
}
