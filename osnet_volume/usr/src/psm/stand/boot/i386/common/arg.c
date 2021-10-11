/*
 * Copyright (c) 1992, 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident	"@(#)arg.c	1.5	96/04/08 SMI"

/* boot shell argument handling routines */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/salib.h>

extern void *memcpy(void *s1, void *s2, size_t n);

/* init_arg() - initialize arg structure */

void
init_arg(struct arg *argp)
{
	argp->argc = 0;
}


/* cleanup_arg() - cleanup arg structure */

void
cleanup_arg(struct arg *argp)
{
	int	i;
	char	*str;

	/* printf("cleanup_arg()\n"); */

	/* free argument strings */
	for (i = argp->argc; --i >= 0; ) {
		str = argp->argv[i];
		bkmem_free(str, argp->lenv[i]);
	}
}


/* put_arg() - put an argument word into argv[] */

void
put_arg(word, argp, word_len)
	unsigned char *word;
	struct arg *argp;
	int word_len;
{
	char    *addr;

	if (argp->argc >= ARGSIZ) {
		printf("put_arg(): too many arguments to command\n");
		return;
	}
	word_len += 1; /* Include the terminating null! */
	if ((addr = bkmem_alloc(word_len)) == NULL) {
		printf("put_arg(): no memory for arg\n");
		return;
	}
	(void) memcpy(addr, word, word_len);
	argp->argv[argp->argc] = addr;
	argp->lenv[argp->argc++] = word_len;
}

#ifdef DEBUG
void
print_args(struct arg *argp)
{
	int a;
	printf("There are %d args is. Args are:\n", argp->argc);
	for (a = 0; a < argp->argc; a++) {
		printf("arg %d: ", a);
		printf("%s\n", argp->argv[a]);
	}
}
#endif /* DEBUG */
