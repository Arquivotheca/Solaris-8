/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)depend.c	1.1	99/08/13 SMI"

#include	<sys/types.h>
#include	<stdio.h>
#include	<errno.h>
#include	<unistd.h>
#include	<string.h>
#include	<wait.h>
#include	<limits.h>
#include	<gelf.h>
#include	"machdep.h"
#include	"sgs.h"
#include	"_crle.h"
#include	"msg.h"

/*
 * Establish the dependencies of an ELF object and add them to the internal
 * configuration information. This information is gathered by using libcrle.so.1
 * as an audit library - this is akin to using ldd(1) only simpler.
 */
int
depend(Crle_desc * crle, const char * name, GElf_Ehdr * ehdr, int flags)
{
	const char *	exename;
	const char *	preload;
	int		fildes[2], pid;

	/*
	 * If we're dealing with a dynamic executable we'll execute it,
	 * otherwise we'll preload the shared object with one of the lddstub's.
	 */
	if (ehdr->e_type == ET_EXEC) {
		exename = name;
		preload = 0;
	} else {
		if (crle->c_class == ELFCLASS32)
			exename = MSG_ORIG(MSG_PTH_LDDSTUB);
		else
			exename = MSG_ORIG(MSG_PTH_LDDSTUB_64);
		preload = name;
	}

	/*
	 * Set up a pipe through which the audit library will write the
	 * dependencies.
	 */
	if (pipe(fildes) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_PIPE),
		    crle->c_name, strerror(err));
		return (1);
	}

	/*
	 * Fork ourselves to run our executable and collect its dependencies.
	 */
	if ((pid = fork()) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_FORK),
		    crle->c_name, strerror(err));
		return (1);
	}

	if (pid) {
		/*
		 * Parent. Read each dependency from the audit library. The read
		 * side of the pipe is attached to stdio to make obtaining the
		 * individual dependencies easier.
		 */
		int	error = 0, status;
		FILE *	fd;
		char	buffer[PATH_MAX];

		(void) close(fildes[1]);
		if ((fd = fdopen(fildes[0], MSG_ORIG(MSG_STR_READ))) != NULL) {
			char *	str;

			while (fgets(buffer, PATH_MAX, fd) != NULL) {
				/*
				 * Make sure we recognize the message, remove
				 * the newline (which allowed fgets() use) and
				 * register the name;
				 */
				if (strncmp(MSG_ORIG(MSG_AUD_PRF), buffer,
				    MSG_AUD_PRF_SIZE))
					continue;

				str = strrchr(buffer, '\n');
				*str = '\0';
				str = buffer + MSG_AUD_PRF_SIZE;

				if ((error = inspect(crle, str,
				    (flags & ~CRLE_GROUP))) != 0)
					break;
			}
		} else
			error = errno;

		while (wait(&status) != pid)
			;
		if (status) {
			if (WIFSIGNALED(status)) {
				(void) fprintf(stderr,
				    MSG_INTL(MSG_SYS_EXEC), crle->c_name,
				    exename, (WSIGMASK & status),
				    ((status & WCOREFLG) ?
				    MSG_INTL(MSG_SYS_CORE) :
				    MSG_ORIG(MSG_STR_EMPTY)));
			}
			error = status;
		}
		(void) fclose(fd);

		return (error);
	} else {
		char	efds[MSG_ENV_AUD_FD_SIZE + 10];
		char	epld[PATH_MAX];
		char	eldf[PATH_MAX];

		(void) close(fildes[0]);

		/*
		 * Child. Set up environment variables to enable and identify
		 * auditing.  Initialize CRLE_FD and LD_FLAGS strings.
		 */
		(void) sprintf(efds, MSG_ORIG(MSG_ENV_AUD_FD), fildes[1]);
		(void) sprintf(eldf, MSG_ORIG(MSG_ENV_LD_FLAGS));

		/*
		 * If asked to dump a group of dependencies make sure any
		 * lazily-loaded objects get processed - (append loadavail to
		 * LD_FLAGS=confgen).
		 */
		if (flags & CRLE_GROUP)
			(void) strcat(eldf, MSG_ORIG(MSG_LDFLG_LOADAVAIL));

		/*
		 * Put LD_PRELOAD= in the environment if necessary.
		 */
		if (preload) {
			(void) sprintf(epld, MSG_ORIG(MSG_ENV_LD_PRELOAD),
			    preload);
		}

		/*
		 * Put strings in the environment for exec().
		 */
		if ((putenv(efds) != 0) || (putenv(crle->c_audit) != 0) ||
		    (putenv(eldf) != 0) || (preload && (putenv(epld) != 0))) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_PUTENV),
			    crle->c_name, strerror(err));
			return (1);
		}

		if (execlp(exename, exename, 0) == -1) {
			_exit(errno);
			/* NOTREACHED */
		}
	}
	/* NOTREACHED */
}
