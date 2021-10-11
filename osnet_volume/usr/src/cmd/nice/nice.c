/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)nice.c	1.13	94/12/04 SMI"

/*
**	nice
*/


#include	<stdio.h>
#include	<ctype.h>
#include	<nl_types.h>
#include	<locale.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/errno.h>

static void usage(void);

int
main(argc, argv)
int argc;
char *argv[];
{
	int	nicarg = 10;
	char	*nicarg_str;
	int	nflg = 0;
	char	*end_ptr;
	extern	errno;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);


	if (argc < 2)
		usage();
	if (strcmp(argv[1], "--") == 0) {
		argv++;
		argc--;
		if (argc < 2)
			usage();
	} else {
		if (argv[1][0] == '-') {
			register char	*p = argv[1];
			if (*++p == 'n') {	/* -n55 new form, XCU4 */
				argv++;
				argc--;
				nicarg_str = argv[1];
				p = argv[1];
				if (*p == '-')
					p++;
				nflg++;
			} else {		/* -55 obs form, XCU4 */
				nicarg_str = &argv[1][1];
					if (*p == '-')
						p++;
			}
			nicarg = strtol(nicarg_str, &end_ptr, 10);
			if (*end_ptr != '\0') {
				(void) fprintf(stderr,
				gettext("nice: argument must be numeric.\n"));
				usage();
			}
			argc--;
			argv++;
		}
		if (argc < 2)
			usage();
		if (strcmp(argv[1], "--") == 0) {
			argv++;
			argc--;
			if (argc < 2)
				usage();
		}
	}
	errno = 0;
	if (nice(nicarg) == -1) {
		/*
		 * Could be an error or a legitimate return value.
		 * The only error we care about is EINVAL, which will
		 * be returned by the scheduling class we are in if
		 * nice is invalid for this class.
		 * For any error other than EINVAL
		 * we will go ahead and exec the command even though
		 * the priority change failed.
		 */
		if (errno == EINVAL) {
			(void) fprintf(stderr, gettext(
			    "nice: invalid operation; "
			    "scheduling class does not support nice\n"));
			return (2);
		}
	}
	(void) execvp(argv[1], &argv[1]);
	(void) fprintf(stderr, gettext("%s: %s\n"), strerror(errno), argv[1]);
	/*
	 * POSIX.2 exit status:
	 * 127 if utility is not found.
	 * 126 if utility cannot be invoked.
	 */
	return (errno == ENOENT || errno == ENOTDIR ? 127 : 126);
}

static void
usage()
{
	(void) fprintf(stderr,
	gettext("nice: usage: nice [-n increment] utility [argument ...]\n"));
	exit(2);
}
