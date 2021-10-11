/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)funcs.c	1.1	99/04/16 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <auth_attr.h>
#include <prof_attr.h>
#include <user_attr.h>
#include <secdb.h>
#include <pwd.h>
#include "funcs.h"

char *
getusertype(char *cmdname)
{
	static char usertype[MAX_TYPE_LENGTH];
	char *cmd;

	if (cmd = strrchr(cmdname, '/'))
		++cmd;
	else
		cmd = cmdname;

	/* get user type based on the program name */
	if (strncmp(cmd, CMD_PREFIX_USER,
	    strlen(CMD_PREFIX_USER)) == 0)
		strcpy(usertype, USERATTR_TYPE_NORMAL_KW);
	else
		strcpy(usertype, USERATTR_TYPE_NONADMIN_KW);

	return (usertype);
}

int
is_role(char *usertype)
{
	if (strcmp(usertype, USERATTR_TYPE_NONADMIN_KW) == 0)
		return (1);
	/* not a role */
	return (0);
}

/*
 * Verifies the provided list of authorizations are all valid.
 *
 * Returns NULL if all authorization names are valid.
 * Otherwise, returns the invalid authorization name
 *
 */
char *
check_auth(char *auths)
{
	char *authname;
	authattr_t *result;
	char *tmp;
	struct passwd   *pw;
	int have_grant = 0;

	tmp = strdup(auths);

	authname = strtok(tmp, AUTH_SEP);
	pw = getpwuid(getuid());
	if (pw == NULL) {
		return (authname);
	}

	while (authname != NULL) {
		char *suffix;
		char *authtoks;

		/* Find the suffix */
		if ((suffix = rindex(authname, '.')) == NULL)
			return (authname);

		/* Check for existence in auth_attr */
		suffix++;
		if (strcmp(suffix, KV_WILDCARD)) { /* Not a wildcard */
			result = getauthnam(authname);
			if (result == NULL) {
			/* can't find the auth */
				free_authattr(result);
				return (authname);
			}
			free_authattr(result);
		}

		/* Check if user has been granted this authorization */
		if (!chkauthattr(authname, pw->pw_name)) {
			return (authname);
		}

		/* Check if user can delegate this authorization */
		if (strcmp(suffix, "grant")) { /* Not a grant option */
			authtoks = strdup(authname);
			have_grant = 0;
			while ((suffix = rindex(authtoks, '.')) &&
			    !have_grant) {
				strcpy(suffix, ".grant");
				if (chkauthattr(authtoks, pw->pw_name))
					have_grant = 1;
				else
					*suffix = '\0';
			}
			if (!have_grant)
				return (authname);
		}
		authname = strtok(NULL, AUTH_SEP);
	}
	return (NULL);
}

/*
 * Verifies the provided list of profile names are valid.
 *
 * Returns NULL if all profile names are valid.
 * Otherwise, returns the invalid profile name
 *
 */
char *
check_prof(char *profs)
{
	char *profname;
	profattr_t *result;
	char *tmp;

	tmp = strdup(profs);

	profname = strtok(tmp, PROF_SEP);
	while (profname != NULL) {
		result = getprofnam(profname);
		if (result == NULL) {
		/* can't find the profile */
			return (profname);
		}
		free_profattr(result);
		profname = strtok(NULL, PROF_SEP);
	}
	return (NULL);
}


/*
 * Verifies the provided list of role names are valid.
 *
 * Returns NULL if all role names are valid.
 * Otherwise, returns the invalid role name
 *
 */
char *
check_role(char *roles)
{
	char *rolename;
	userattr_t *result;
	char *utype;
	char *tmp;

	tmp = strdup(roles);

	rolename = strtok(tmp, ROLE_SEP);
	while (rolename != NULL) {
		result = getusernam(rolename);
		if (result == NULL) {
		/* can't find the rolename */
			return (rolename);
		}
		/* Now, make sure it is a role */
		utype = kva_match(result->attr, USERATTR_TYPE_KW);
		free_userattr(result);
		if (utype == NULL) {
			/* no user type defined. not a role */
			return (rolename);
		}
		if (strcmp(utype, USERATTR_TYPE_NONADMIN_KW) != 0) {
			return (rolename);
		}
		rolename = strtok(NULL, ROLE_SEP);
	}
	return (NULL);
}
