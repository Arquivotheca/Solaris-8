#ifndef lint
static char sccsid[] = "@(#)audit_user.c 1.14 99/05/25 SMI";
#endif

/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 */

/*
 * Interfaces to audit_user(5)  (/etc/security/audit_user)
 */

#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <synch.h>
#include <nss_dbdefs.h>
#include <stdlib.h>


#define	MAX_USERNAME	32	/* same size as utmpx.ut_user */


static char	au_user_fname[PATH_MAX] = AUDITUSERFILE;
static FILE *au_user_file = (FILE *) 0;
static mutex_t mutex_userfile = DEFAULTMUTEX;
static int use_nsswitch = 1;
static au_user_ent_t *auuserstr2ent(au_user_ent_t *, au_user_str_t *);

extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);

/*
 * Externs from libnsl
 */
extern void _setauuser(void);
extern void _endauuser(void);
extern au_user_str_t *_getauuserent(au_user_str_t *, char *, int, int *);
extern au_user_str_t *_getauusernam(char *, au_user_str_t *, char *, int,
    int *);

#ifdef __STDC__
int
setauuserfile(char *fname)
#else
int
setauuserfile(fname)
	char *fname;
#endif
{
	_mutex_lock(&mutex_userfile);
	if (fname) {
		(void) strcpy(au_user_fname, fname);
		use_nsswitch = 0;
	}
	_mutex_unlock(&mutex_userfile);
	return (0);
}


void
setauuser()
{
	_mutex_lock(&mutex_userfile);
	if (use_nsswitch)
		_setauuser();
	else if (au_user_file)
		(void) fseek(au_user_file, 0L, 0);
	_mutex_unlock(&mutex_userfile);
}


void
endauuser()
{
	_mutex_lock(&mutex_userfile);
	if (use_nsswitch)
		_endauuser();
	else if (au_user_file) {
		(void) fclose(au_user_file);
		au_user_file = ((FILE *) 0);
	}
	_mutex_unlock(&mutex_userfile);
}

au_user_ent_t *
getauuserent()
{
	static au_user_ent_t au_user_entry;
	static char	logname[MAX_USERNAME+1];

	/* initialize au_user_entry structure */
	au_user_entry.au_name = logname;

	return (getauuserent_r(&au_user_entry));

}

au_user_ent_t *
getauuserent_r(au_user_ent_t *au_user_entry)
{
	int	i, error = 0, found = 0;
	char	*s, input[256];

	_mutex_lock(&mutex_userfile);

	if (use_nsswitch) {
		au_user_str_t us;
		au_user_str_t *tmp;
		char buf[NSS_BUFLEN_AUDITUSER];
		int errp = 0;

		memset(buf, NULL, NSS_BUFLEN_AUDITUSER);
		tmp = _getauuserent(&us, buf, NSS_BUFLEN_AUDITUSER, &errp);
		_mutex_unlock(&mutex_userfile);
		return (auuserstr2ent(au_user_entry, tmp));
	}

	/* open audit user file if it isn't already */
	if (!au_user_file)
		if (!(au_user_file = fopen(au_user_fname, "r"))) {
			_mutex_unlock(&mutex_userfile);
			return ((au_user_ent_t *) 0);
		}

	while (fgets(input, 256, au_user_file)) {
		if (input[0] != '#') {
			found = 1;
			s = input;

			/* parse login name */
			i = strcspn(s, ":");
			s[i] = '\0';
			(void) strncpy(au_user_entry->au_name, s, MAX_USERNAME);
			s = &s[i+1];

			/* parse first mask */
			i = strcspn(s, ":");
			s[i] = '\0';
			if (getauditflagsbin(s,
			    &au_user_entry->au_always) < 0)
				error = 1;
			s = &s[i+1];

			/* parse second mask */
			i = strcspn(s, "\n\0");
			s[i] = '\0';
			if (getauditflagsbin(s,
			    &au_user_entry->au_never) < 0)
				error = 1;

			break;
		}
	}
	_mutex_unlock(&mutex_userfile);

	if (!error && found) {
		return (au_user_entry);
	} else {
		return ((au_user_ent_t *) 0);
	}
}


#ifdef __STDC__
au_user_ent_t *
getauusernam(char *name)
#else
au_user_ent_t *
getauusernam(name)
	char *name;
#endif
{
	static au_user_ent_t u;
	static char	logname[MAX_USERNAME+1];

	/* initialize au_user_entry structure */
	u.au_name = logname;

	return (getauusernam_r(&u, name));
}

#ifdef __STDC__
au_user_ent_t *
getauusernam_r(au_user_ent_t *u, char *name)
#else
au_user_ent_t *
getauusernam_r(u, name)
	au_user_ent_t *u;
	char *name;
#endif
{
	if (use_nsswitch) {
		au_user_str_t us;
		au_user_str_t *tmp;
		char buf[NSS_BUFLEN_AUDITUSER];
		int errp = 0;

		if (name == NULL) {
			return ((au_user_ent_t *)NULL);
		}
		tmp = _getauusernam(name, &us, buf, NSS_BUFLEN_AUDITUSER,
		    &errp);
		return (auuserstr2ent(u, tmp));
	}
	while (getauuserent_r(u) != NULL) {
		if (strcmp(u->au_name, name) == 0) {
			return (u);
		}
	}
	return ((au_user_ent_t *)NULL);
}

static au_user_ent_t *
auuserstr2ent(au_user_ent_t *ue, au_user_str_t *us)
{
	if (us == NULL)
		return (NULL);

	if (getauditflagsbin(us->au_always, &ue->au_always) < 0) {
		return (NULL);
	}
	if (getauditflagsbin(us->au_never, &ue->au_never) < 0) {
		return (NULL);
	}
	(void) strncpy(ue->au_name, us->au_name, MAX_USERNAME);
	return (ue);
}

#ifdef DEBUG
void
print_auuser(au_user_ent_t *ue)
{
	char *empty = "empty";
	char *bad = "bad flags";
	char always[256];
	char never[256];
	int retval;

	if (ue == NULL) {
		printf("NULL\n");
		return;
	}

	printf("name=%s\n", ue->au_name ? ue->au_name : empty);
	retval = getauditflagschar(always, ue->au_always, 0);
	printf("always=%s\n", retval == 0 ? always : bad);
	retval = getauditflagschar(never, ue->au_never, 0);
	printf("never=%s\n", retval == 0 ? never : bad);
}
#endif	/* DEBUG */
