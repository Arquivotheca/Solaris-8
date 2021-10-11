/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Policy backing functions for kpolicy=suser,profiles=yes
 *
 */

#pragma ident	"@(#)sh_policy.c	1.1	99/05/11 SMI"

#include <sys/param.h>
#include <grp.h>
#include <pwd.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "sh_policy.h"


static const char *username;

/*
 * get the ruid and passwd name
 */
void
secpolicy_init(void)
{
	uid_t		ruid;
	struct passwd	*passwd_ent;

	ruid = getuid();

	if ((passwd_ent = getpwuid(ruid)) == NULL) {
		secpolicy_print(SECPOLICY_ERROR, ERR_PASSWD);
	} else if ((username = strdup(passwd_ent->pw_name)) == NULL) {
		secpolicy_print(SECPOLICY_ERROR, ERR_MEM);
	}
}


/*
 * stuff pfexec full path at the begining of the argument vector
 * for the command to be pfexec'd
 *
 * return newly allocated argv on success, else return NULL.
 */
static char **
secpolicy_set_argv(char **arg_v)
{
	register int	i, j;
	register int	arglen = 0;
	char		**pfarg_v = (char **)NULL;

	if (*arg_v == NULL) {
		return (pfarg_v);
	}
	for (i = 0; arg_v[i] != 0; i++) {
		arglen += strlen(arg_v[i]);
	}
	arglen += strlen(PFEXEC);
	arglen++;	/* for null termination */
	if ((pfarg_v = (char **)calloc(1, arglen)) == NULL) {
		return (pfarg_v);
	}
	pfarg_v[0] = (char *)PFEXEC;
	for (i = 0, j = 1; arg_v[i] != 0; i++, j++) {
		pfarg_v[j] = arg_v[i];
	}
	pfarg_v[j] = 0;

	return (pfarg_v);
}


/*
 * gets realpath for cmd.
 * return 0 on success,  else return ENOENT.
 */
static int
secpolicy_getrealpath(const char *cmd, char *cmd_realpath)
{
	register char	*mover;
	char	cwd[MAXPATHLEN + 1];

	/*
	 * What about relative paths?  Were we passed one?
	 */
	mover = (char *)cmd;
	if (*mover != '/') {
		int len_cwd;

		/*
		 * Everything in here will be considered a relative
		 * path, and therefore we need to prepend cwd to it.
		 */
		if (getcwd(cwd, MAXPATHLEN) == NULL) {
			secpolicy_print(SECPOLICY_ERROR, ERR_CWD);
		}
		strcat(cwd, "/");
		len_cwd = strlen(cwd);
		strncpy((cwd + len_cwd), cmd, (MAXPATHLEN - len_cwd));

		if (cwd[MAXPATHLEN] != '\0') {
			return (ENOENT);
		}
		mover = cwd;
	}
	/*
	 * Resolve ".." and other such nonsense.
	 * Now, is there *REALLY* a file there?
	 */
	if (realpath(mover, cmd_realpath) == NULL) {
		return (ENOENT);
	}

	return (0);
}


/*
 * check if the command has execution attributes
 * return -
 *    - NOATTRS   : command in profile but has no execution attributes
 *    - ENOMEM    : memory allocation errors
 *    - ENOENT    : command not in profile
 */

int
secpolicy_pfexec(const char *command, char **arg_v, const char **xecenv)
{
	register int	status = NOATTRS;
	char		**pfarg_v = (char **)NULL;
	char		cmd_realpath[MAXPATHLEN + 1];
	execattr_t	*exec;

	if ((status = secpolicy_getrealpath(command, cmd_realpath)) != 0) {
		return (status);
	}
	if ((exec = getexecuser(username, KV_COMMAND,
	    (const char *)cmd_realpath, GET_ONE)) == NULL) {
		/*
		 * command not in profile
		 */
		return (ENOENT);
	}
	/*
	 * In case of "All" profile, we'd go through pfexec
	 * if it had any attributes.
	 */
	if ((exec->attr != NULL) && (exec->attr->length != 0)) {
		/*
		 * command in profile and has attributes
		 */
		free_execattr(exec);
		arg_v[0] = cmd_realpath;
		pfarg_v = secpolicy_set_argv(arg_v);
		if (pfarg_v != NULL) {
			errno = 0;
			if (xecenv == NULL) {
				execv(PFEXEC, (char *const *)pfarg_v);
			} else {
				execve(PFEXEC, (char *const *)pfarg_v,
				    (char *const *)xecenv);
			}
			free(pfarg_v);
			status = errno;
		} else {
			status = ENOMEM;
		}
	} else {
		/*
		 * command in profile, but has no attributes
		 */
		free_execattr(exec);
		status = NOATTRS;
	}


	return (status);
}
