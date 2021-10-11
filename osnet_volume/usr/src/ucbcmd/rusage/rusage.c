#ident	"@(#)rusage.c	1.10	98/05/01 SMI"

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * rusage
 */

#include <locale.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>

void
fprintt(s, tv)
	char *s;
	struct timeval *tv;
{

	(void) fprintf(stderr, gettext("%d.%02d %s "), 
		tv->tv_sec, tv->tv_usec/10000, s);
}

main(argc, argv)
	int argc;
	char **argv;
{
	union wait status;
	int options=0;
	register int p;
	struct timeval before, after;
	struct rusage ru;
	struct timezone tz;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc<=1)
		exit(0);
	(void) gettimeofday(&before, &tz);

	/* fork a child process to run the command */

	p = fork();
	if (p < 0) {
		perror("rusage");
		exit(1);
	}

	if (p == 0) {

		/* exec the command specified */

		execvp(argv[1], &argv[1]);
		perror(argv[1]);
		exit(1);
	}

	/* parent code - wait for command to complete */

	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	while (wait3(&status.w_status, options, &ru) != p)
		;

	/* get closing time of day */
	(void) gettimeofday(&after, &tz);

	/* check for exit status of command */

	if ((status.w_termsig) != 0)
		(void) fprintf(stderr, gettext("Command terminated abnormally.\n"));

	/* print an accounting summary line */

	after.tv_sec -= before.tv_sec;
	after.tv_usec -= before.tv_usec;
	if (after.tv_usec < 0) {
		after.tv_sec--;
		after.tv_usec += 1000000;
	}
	fprintt(gettext("real"), &after);
	fprintt(gettext("user"), &ru.ru_utime);
	fprintt(gettext("sys"), &ru.ru_stime);
	(void) fprintf(stderr, gettext("%d pf %d pr %d sw"),
		ru.ru_majflt,
		ru.ru_minflt,
		ru.ru_nswap);
	(void) fprintf(stderr, gettext(" %d rb %d wb %d vcx %d icx"),
		ru.ru_inblock,
		ru.ru_oublock,
		ru.ru_nvcsw,
		ru.ru_nivcsw);
	(void) fprintf(stderr, gettext(" %d mx %d ix %d id %d is"),
		ru.ru_maxrss,
		ru.ru_ixrss,
		ru.ru_idrss,
		ru.ru_isrss);

	(void) fprintf(stderr, "\n");
	exit((int)status.w_retcode);
	/*NOTREACHED*/
}

