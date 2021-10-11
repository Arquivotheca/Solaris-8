/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)chkauthattr.c	1.1	99/05/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>
#include <deflt.h>
#include <auth_attr.h>
#include <prof_attr.h>
#include <user_attr.h>


static int _is_authorized(const char *authname, char *auths);
static int _get_auth_policy(const char *);


int
chkauthattr(const char *authname, const char *username)
{
	int		auth_granted = FALSE;
	char		*lasts;
	char		*auths;
	char		*prof;
	char		*profiles;
	profattr_t	*pa;
	userattr_t	*user;

	if (authname == NULL || username == NULL)
		return (0);

	auth_granted = _get_auth_policy(authname);
	if (auth_granted == TRUE) {
		return (1);
	}
	if ((user = getusernam(username)) == NULL)
		return (0);

	if ((auths = kva_match(user->attr, USERATTR_AUTHS_KW)) != NULL) {
		if (_is_authorized(authname, auths)) {
			free_userattr(user);
			return (1);
		}
	}

	if ((profiles = kva_match(user->attr, USERATTR_PROFILES_KW)) == NULL) {
		free_userattr(user);
		return (0);
	}

	for (prof = strtok_r(profiles, ",", &lasts); prof != NULL;
	    prof = strtok_r(NULL, ",", &lasts)) {
		if ((pa = getprofnam(prof)) == NULL)
			continue;
		if ((auths = kva_match(pa->attr, PROFATTR_AUTHS_KW)) != NULL) {
			if (_is_authorized(authname, auths)) {
				free_userattr(user);
				free_profattr(pa);
				return (1);
			}
		}
		free_profattr(pa);
	}

	free_userattr(user);

	/* Not Authorized */
	return (0);
}

static int
_is_authorized(const char *authname, char *auths)
{
	int	len;		/* Length of the user's authorization */
	int	found = 0;	/* have we got a match, yet */
	char	wildcard = '*';
	char	*auth;		/* current authorization being compared */
	char	*grant;		/* pointer to occurence of key word grant */
	char	*buf;
	char	*lasts;

	buf = strdup(auths);
	for (auth = strtok_r(auths, ",", &lasts); auth != NULL && !found;
	    auth = strtok_r(NULL, ",", &lasts)) {
		if (strcmp((char *)authname, auth) == 0) {
			/* Exact match.  We're done. */
			found = 1;
		} else if (strchr(auth, wildcard) != NULL) {
			len = strlen(auth);
			/*
			 * If the wildcard is not in the last
			 * position in the string, don't match
			 * against it.
			 */
			if (auth[len-1] != wildcard)
				continue;
			/*
			 * If the strings are identical up to
			 * the wildcard and authname does not
			 * end in "grant", then we have a match.
			 */
			if (strncmp(authname, auth, len-1) == 0) {
				if ((grant = strrchr(authname, '.')) != NULL) {
					++grant;
					if (strncmp(grant, "grant", 5) !=
					    NULL) {
						found = 1;
					}
				}
			}
		}
	}

	free(buf);

	return (found);
}


/*
 * read /etc/security/policy.conf for AUTHS_GRANTED.
 * return 1(TRUE) if found matching authname, else return 0(FALSE)
 */
static int
_get_auth_policy(const char *authname)
{
	char 	*auths = (char *)NULL;

	if (defopen(AUTH_POLICY) != NULL) {
		return (FALSE);
	}
	auths = defread(DEF_AUTH);
	if (auths == NULL) {
		return (FALSE);
	}
	if (_is_authorized(authname, auths)) {
		return (TRUE);
	}
}
