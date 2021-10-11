/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dump.c	1.1	99/08/13 SMI"

#include	<sys/types.h>
#include	<stdio.h>
#include	<errno.h>
#include	<unistd.h>
#include	<string.h>
#include	<wait.h>
#include	<limits.h>
#include	"machdep.h"
#include	"sgs.h"
#include	"rtc.h"
#include	"_crle.h"
#include	"msg.h"

/*
 * Having gathered together any dependencies, dldump(3x) any necessary images.
 *
 * All dldump(3x) processing is carried out from the audit library.  The
 * temporary configuration file is read and all alternative marked files are
 * dumped.  If a -E application requires RTLD_REL_EXEC then that application
 * acts as the new process, otherwise lddstub is used.
 *
 * Besides dldump(3x)'ing any images the audit library returns the address
 * range of the images which will used to update the configuration file.
 */
int
dump(Crle_desc * crle)
{
	const char *	orgapp = (const char *)crle->c_app;
	int		fildes[2], pid;

	if (orgapp == 0) {
		if (crle->c_class == ELFCLASS32)
			orgapp = MSG_ORIG(MSG_PTH_LDDSTUB);
		else
			orgapp = MSG_ORIG(MSG_PTH_LDDSTUB_64);
	}

	/*
	 * Set up a pipe through which the audit library will write the image
	 * address ranges.
	 */
	if (pipe(fildes) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_PIPE),
		    crle->c_name, strerror(err));
		return (1);
	}

	/*
	 * Fork ourselves to run the application and collect its dependencies.
	 */
	if ((pid = fork()) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_FORK),
		    crle->c_name, strerror(err));
		return (1);
	}

	if (pid) {
		/*
		 * Parent. Read memory range entries from the audit library.
		 * The read side of the pipe is attached to stdio to make
		 * obtaining the individual dependencies easier.
		 */
		int	error = 0, status;
		FILE *	fd;
		char	buffer[PATH_MAX];

		(void) close(fildes[1]);
		if ((fd = fdopen(fildes[0], MSG_ORIG(MSG_STR_READ))) != NULL) {
			char *		str;
			Rtc_head *	rtc = (Rtc_head *)crle->c_tempaddr;

			while (fgets(buffer, PATH_MAX, fd) != NULL) {
				/*
				 * Make sure we recognize the message, remove
				 * the newline (which allowed fgets() use) and
				 * register the memory range entry;
				 */
				if (strncmp(MSG_ORIG(MSG_AUD_PRF), buffer,
				    MSG_AUD_PRF_SIZE))
					continue;

				str = strrchr(buffer, '\n');
				*str = '\0';
				str = buffer + MSG_AUD_PRF_SIZE;

				if (strncmp(MSG_ORIG(MSG_AUD_RESBGN),
				    str, MSG_AUD_RESBGN_SIZE) == 0) {
					rtc->ch_resbgn =
					    strtoull(str + MSG_AUD_RESBGN_SIZE,
						(char **)NULL, 0);
				} else if (strncmp(MSG_ORIG(MSG_AUD_RESEND),
				    str, MSG_AUD_RESEND_SIZE) == 0) {
					rtc->ch_resend =
					    strtoull(str + MSG_AUD_RESEND_SIZE,
						(char **)NULL, 0);
				} else
					continue;
			}
			(void) fclose(fd);
		} else
			error = errno;

		while (wait(&status) != pid)
			;
		if (status) {
			if (WIFSIGNALED(status)) {
				(void) fprintf(stderr,
				    MSG_INTL(MSG_SYS_EXEC), crle->c_name,
				    orgapp, (WSIGMASK & status),
				    ((status & WCOREFLG) ?
				    MSG_INTL(MSG_SYS_CORE) :
				    MSG_ORIG(MSG_STR_EMPTY)));
			}
			return (status);
		}
		return (error);
	} else {
		char	efds[MSG_ENV_AUD_FD_SIZE + 10];
		char	eflg[MSG_ENV_AUD_FLAGS_SIZE + 10];
		char	ecnf[PATH_MAX];

		(void) close(fildes[0]);

		/*
		 * Child. Set up environment variables to enable and identify
		 * auditing.
		 */
		(void) sprintf(efds, MSG_ORIG(MSG_ENV_AUD_FD), fildes[1]);
		(void) sprintf(eflg, MSG_ORIG(MSG_ENV_AUD_FLAGS),
		    crle->c_dlflags);
		(void) sprintf(ecnf, MSG_ORIG(MSG_ENV_LD_CONFIG),
		    crle->c_tempname);

		/*
		 * Put strings in the environment for exec().
		 */
		if ((putenv(efds) != 0) || (putenv(eflg) != 0) ||
		    (putenv(ecnf) != 0) || (putenv(crle->c_audit) != 0) ||
		    (putenv((char *)MSG_ORIG(MSG_ENV_LD_FLAGS)) != 0)) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_PUTENV),
			    crle->c_name, strerror(err));
			return (1);
		}

		if (execlp(orgapp, orgapp, 0) == -1) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXECLP),
			    crle->c_name, orgapp, strerror(err));
			_exit(err);
			/* NOTREACHED */
		}
	}
	/* NOTREACHED */
}
