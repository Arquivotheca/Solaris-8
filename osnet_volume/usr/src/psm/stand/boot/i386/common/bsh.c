/*
 * Copyright (c) 1992-1996 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)bsh.c	1.6	96/03/23 SMI"

/* boot shell interpreter */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/salib.h>

void run_command(struct arg *argp);
extern void singlestep(void);
extern void get_command(struct arg *argp);
extern int if_yes(struct arg *argp);
extern void init_arg(struct arg *argp);
extern void cleanup_arg(struct arg *argp);

void
bsh()
{

	struct arg arg;		/* argument info */

	/* interpreter loop */
	for (;;) {
		singlestep();
		init_arg(&arg);
		get_command(&arg);
		if (if_yes(&arg))
			run_command(&arg);
		cleanup_arg(&arg);
	}
}

void
run_command(struct arg *argp)
{
	int rc;
	int argc;
	char **argv;
	char *arg_zero;

	extern int cmd_count;
	extern int verbose_flag;

	struct cmd *cbot = cmd;
	struct cmd *ctop = &cmd[cmd_count];

	if (verbose_flag) {
		printf("+ ");
		argc = argp->argc;
		argv = argp->argv;
		while (--argc >= 0) {
			printf("%s%s", *argv++, (argc > 0) ? " " : "");
		}
		printf("\n");
	}

	arg_zero = argp->argv[0];

	while (ctop > cbot) {
	    /* Binary search the command table ...			*/
	    struct cmd *cmdp = cbot + ((ctop - cbot) >> 1);

	    if (!(rc = strcmp(arg_zero, cmdp->name))) {
		/* Found the command; go do it!				*/
		(*cmdp->func)(argp->argc, argp->argv, argp->lenv);
		return;

	    } else if (rc > 0) {
		/* Search upper half of remaining table			*/
		cbot = cmdp+1;

	    } else {
		/* Search lower half of remaining table			*/
		ctop = cmdp;
	    }
	}

	printf("%s: not found\n", arg_zero);
}
