#ident	"@(#)uucp_glue.c	1.9	94/01/21 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"
#include "uucp.h"
#include "uucp_glue.h"

jmp_buf	Sjbuf;			/* needed by uucp routines */
int	Cn;			/* fd returned by conn */
int	Dologin = 1;		/* go through the login chat sequence */

void	setservice(char *);	/* uucico, cu, ppp, etc. */
int	sysaccess(int);		/* determines if accessable */

void
uucp_prolog(char *service)
{
	strcpy(Progname, service);
	setservice(Progname);
	if (sysaccess(EACCESS_SYSTEMS) != 0)
		fail("%s: cannot read Systems files\n", Progname);

	if (sysaccess(EACCESS_DEVICES) != 0)
		fail("%s: cannot read Devices files\n", Progname);

	if (sysaccess(EACCESS_DIALERS) != 0)
		fail("%s: cannot read Dialers files\n", Progname);

	(void) signal(SIGHUP, cleanup);
	(void) signal(SIGQUIT, cleanup);
	(void) signal(SIGINT, cleanup);
	(void) signal(SIGTERM, cleanup);
}

void
uucp_epilog(void)
{
	struct stat	Cnsbuf;

	if (fstat(Cn, &Cnsbuf) == 0)
		Dev_mode = Cnsbuf.st_mode;
	else
		Dev_mode = R_DEVICEMODE;
	fchmod(Cn, M_DEVICEMODE);
}

void
cleanup(int code)	/* Closes device; removes lock files */
{
	CDEBUG(4, "call cleanup(%d)\r\n", code);

	if (Cn > 0) {
		fchmod(Cn, Dev_mode);
		fd_rmlock(Cn);
		(void) close(Cn);
	}

	rmlock((char *) NULL);	/* remove all lock files for this process */

	_exit(code);	/* code=negative for signal causing disconnect */
}

/*VARARGS*/
/*ARGSUSED*/
void
assert(char *s1, char *s2, int i1, char *s3, int i2)
{
	/* for ASSERT in gnamef.c */
}

/*ARGSUSED*/
void
logent(char *s1, char *s2)
{
	/* so we can load ulockf() */
}
