/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauthattr.c	1.1	99/06/08 SMI"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nss_dbdefs.h>
#include <auth_attr.h>


/* Externs from libnsl */
extern authstr_t *_getauthnam(const char *, authstr_t *, char *, int, int *);
extern authstr_t *_getauthattr(authstr_t *, char *, int, int *);
extern void _setauthattr();
extern void _endauthattr(void);


static authattr_t *authstr2attr(authstr_t *);


authattr_t *
getauthattr()
{
	int		err = 0;
	char		buf[NSS_BUFLEN_AUTHATTR];
	authstr_t	auth;
	authstr_t	*tmp;

	memset(&auth, NULL, sizeof (authstr_t));
	tmp = _getauthattr(&auth, buf, NSS_BUFLEN_AUTHATTR, &err);
	return (authstr2attr(tmp));
}


authattr_t *
getauthnam(const char *name)
{
	int		err = 0;
	char		buf[NSS_BUFLEN_AUTHATTR];
	authstr_t	auth;
	authstr_t	*tmp;

	if (name == NULL) {
		return ((authattr_t *)NULL);
	}
	memset(&auth, NULL, sizeof (authstr_t));
	tmp = _getauthnam(name, &auth, buf, NSS_BUFLEN_AUTHATTR, &err);
	return (authstr2attr(tmp));
}


void
setauthattr(void)
{
	_setauthattr();
}


void
endauthattr()
{
	_endauthattr();
}


void
free_authattr(authattr_t *auth)
{
	if (auth) {
		free(auth->name);
		free(auth->res1);
		free(auth->res2);
		free(auth->short_desc);
		free(auth->long_desc);
		_kva_free(auth->attr);
		free(auth);
	}
}


static authattr_t *
authstr2attr(authstr_t *auth)
{
	authattr_t *newauth;

	if (auth == NULL)
		return ((authattr_t *)NULL);

	if ((newauth = (authattr_t *)malloc(sizeof (authattr_t))) == NULL)
		return ((authattr_t *)NULL);

	newauth->name = _do_unescape(auth->name);
	newauth->res1 = _do_unescape(auth->res1);
	newauth->res2 = _do_unescape(auth->res2);
	newauth->short_desc = _do_unescape(auth->short_desc);
	newauth->long_desc = _do_unescape(auth->long_desc);
	newauth->attr = _str2kva(auth->attr, KV_ASSIGN, KV_DELIMITER);
	return (newauth);
}


#ifdef DEBUG
void
print_authattr(authattr_t *auth)
{
	extern void print_kva(kva_t *);
	char *empty = "empty";

	if (auth == NULL) {
		printf("NULL\n");
		return;
	}

	printf("name=%s\n", auth->name ? auth->name : empty);
	printf("res1=%s\n", auth->res1 ? auth->res1 : empty);
	printf("res2=%s\n", auth->res2 ? auth->res2 : empty);
	printf("short_desc=%s\n", auth->short_desc ? auth->short_desc : empty);
	printf("long_desc=%s\n", auth->long_desc ? auth->long_desc : empty);
	printf("attr=\n");
	print_kva(auth->attr);
	fflush(stdout);
}
#endif  /* DEBUG */
