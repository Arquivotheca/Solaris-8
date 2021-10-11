/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)acltext.c 1.8 98/08/05 SMI"
/*LINTLIBRARY*/

#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/acl.h>

/* id: LOGNAME_MAX = 8 */
#define	TYPELEN 16	/* entry type */
#define	PERMLEN	4	/* permission */

#define	ACLRD	04
#define	ACLWR	02
#define	ACLEX	01

static char *strappend(char *, char *);
static char *convert_perm(char *, o_mode_t);

/*
 * Convert internal acl representation to external representation
 */
char *
acltotext(aclent_t *aclp, int aclcnt)
{
	char		*aclexport;
	char		*where;
	struct group	*groupp;
	struct passwd	*passwdp;
	int		i;

	if (aclp == NULL)
		return (NULL);
	aclexport = malloc(aclcnt * (LOGNAME_MAX + TYPELEN + PERMLEN));
	if (aclexport == NULL)
		return (NULL);
	*aclexport = '\0';	/* make it a null string */
	where = aclexport;

	for (i = 0; i < aclcnt; i++, aclp++) {
		switch (aclp->a_type) {
		case DEF_USER_OBJ:
		case USER_OBJ:
			if (aclp->a_type == USER_OBJ)
				where = strappend(where, "user::");
			else
				where = strappend(where, "defaultuser::");
			where = convert_perm(where, aclp->a_perm);
			break;
		case DEF_USER:
		case USER:
			if (aclp->a_type == USER)
				where = strappend(where, "user:");
			else
				where = strappend(where, "defaultuser:");
			passwdp = getpwuid(aclp->a_id);
			if (passwdp == (struct passwd *) NULL) {
				/* put in uid instead */
				(void) sprintf(where, "%d", aclp->a_id);
			} else
				where = strappend(where, passwdp->pw_name);
			where = strappend(where, ":");
			where = convert_perm(where, aclp->a_perm);
			break;
		case DEF_GROUP_OBJ:
		case GROUP_OBJ:
			if (aclp->a_type == GROUP_OBJ)
				where = strappend(where, "group::");
			else
				where = strappend(where, "defaultgroup::");
			where = convert_perm(where, aclp->a_perm);
			break;
		case DEF_GROUP:
		case GROUP:
			if (aclp->a_type == GROUP)
				where = strappend(where, "group:");
			else
				where = strappend(where, "defaultgroup:");
			groupp = getgrgid(aclp->a_id);
			if (groupp == (struct group *) NULL) {
				/* put in gid instead */
				(void) sprintf(where, "%d", aclp->a_id);
			} else
				where = strappend(where, groupp->gr_name);
			where = strappend(where, ":");
			where = convert_perm(where, aclp->a_perm);
			break;
		case DEF_CLASS_OBJ:
		case CLASS_OBJ:
			if (aclp->a_type == CLASS_OBJ)
				where = strappend(where, "mask::");
			else
				where = strappend(where, "defaultmask::");
			where = convert_perm(where, aclp->a_perm);
			break;
		case DEF_OTHER_OBJ:
		case OTHER_OBJ:
			if (aclp->a_type == OTHER_OBJ)
				where = strappend(where, "other::");
			else
				where = strappend(where, "defaultother::");
			where = convert_perm(where, aclp->a_perm);
			break;
		default:
			free(aclexport);
			return (NULL);

		}
	}
	/* There is an extra comma at the end, but it's ok. */
	return (aclexport);
}

/*
 * Convert external acl representation to internal representation
 */
aclent_t *
aclfromtext(char *aclimport, int *aclcnt)
{
	char		*fieldp;
	char		*tp;
	char		*nextp;
	int		entry_type;
	int		id;
	o_mode_t	perm;
	aclent_t	*tmpaclp;
	aclent_t	*aclp;
	struct group	*groupp;
	struct passwd	*passwdp;

	*aclcnt = 0;
	aclp = NULL;
	if (aclimport == NULL)
		return (NULL);
	for (;;) {
		/* look for an ACL entry */
		tp = strchr(aclimport, ',');
		if (tp == NULL)
			break;	/* done */
		else
			*tp = '\0';
		*aclcnt += 1;

		/*
		 * get additional memory:
		 * can be more efficient by allocating a bigger block
		 * each time.
		 */
		if (*aclcnt > 1)
			tmpaclp = (aclent_t *) realloc(aclp,
			    sizeof (aclent_t) * (*aclcnt));
		else
			tmpaclp = (aclent_t *) malloc(sizeof (aclent_t));
		if (tmpaclp == NULL)
			return (NULL);
		aclp = tmpaclp;
		tmpaclp = aclp + (*aclcnt - 1);

		nextp = tp + 1;		/* next acl entry starts here */

		/* look for entry type field */
		tp = strchr(aclimport, ':');
		if (tp == NULL) {
			free(aclp);
			return (NULL);
		} else
			*tp = '\0';
		if (strcmp(aclimport, "user") == 0) {
			if (*(tp+1) == ':')
				entry_type = USER_OBJ;
			else
				entry_type = USER;
		} else if (strcmp(aclimport, "group") == 0) {
			if (*(tp+1) == ':')
				entry_type = GROUP_OBJ;
			else
				entry_type = GROUP;
		} else if (strcmp(aclimport, "other") == 0)
			entry_type = OTHER_OBJ;
		else if (strcmp(aclimport, "mask") == 0)
			entry_type = CLASS_OBJ;
		else if (strcmp(aclimport, "defaultuser") == 0) {
			if (*(tp+1) == ':')
				entry_type = DEF_USER_OBJ;
			else
				entry_type = DEF_USER;
		} else if (strcmp(aclimport, "defaultgroup") == 0) {
			if (*(tp+1) == ':')
				entry_type = DEF_GROUP_OBJ;
			else
				entry_type = DEF_GROUP;
		} else if (strcmp(aclimport, "defaultmask") == 0)
			entry_type = DEF_CLASS_OBJ;
		else if (strcmp(aclimport, "defaultother") == 0)
			entry_type = DEF_OTHER_OBJ;
		else {
			free(aclp);
			return (NULL);
		}

		/* look for user/group name */
		fieldp = tp + 1;
		tp = strchr(fieldp, ':');
		if (tp == NULL) {
			free(aclp);
			return (NULL);
		} else
			*tp = '\0';
		if (fieldp != tp) {
			/*
			 * The second field could be empty. We only care
			 * when the field has user/group name.
			 */
			if (entry_type == USER || entry_type == DEF_USER) {
				/*
				 * The reentrant interface getpwnam_r()
				 * is uncommitted and subject to change.
				 * Use the friendlier interface getpwnam().
				 */
				passwdp = getpwnam(fieldp);
				if (passwdp == NULL) {
					(void) fprintf(stderr, "user %s not found\n",
					    fieldp);
					id = UID_NOBODY; /* nobody */
				}
				else
					id = passwdp->pw_uid;
			} else {
				if (entry_type == GROUP ||
				    entry_type == DEF_GROUP) {
					groupp = getgrnam(fieldp);
					if (groupp == NULL) {
						(void) fprintf(stderr,
						"group %s not found\n", fieldp);
						id = GID_NOBODY; /* no group? */
					}
					else
						id = groupp->gr_gid;
				} else {
					(void) fprintf(stderr, "acl import errors\n");
					free(aclp);
					return (NULL);
				}
			}
		} else {
			/*
			 * The second field is empty.
			 * other, mask, and etc.: treat it as undefined (-1)
			 */
			id = -1;
		}

		/* next field: permission */
		fieldp = tp + 1;
		if (strlen(fieldp) != 3) {
			/*  not "rwx" format */
			free(aclp);
			return (NULL);
		} else {
			perm = 0;
			if (*fieldp == 'r')
				perm |= ACLRD;
			else
				if (*fieldp != '-') {
					free(aclp);
					return (NULL);
				}
			fieldp++;
			if (*fieldp == 'w')
				perm |= ACLWR;
			else
				if (*fieldp != '-') {
					free(aclp);
					return (NULL);
				}
			fieldp++;
			if (*fieldp == 'x')
				perm |= ACLEX;
			else
				if (*fieldp != '-') {
					free(aclp);
					return (NULL);
				}
		}

		tmpaclp->a_type = entry_type;
		tmpaclp->a_id = id;
		tmpaclp->a_perm = perm;
		aclimport = nextp;
	}
	return (aclp);
}

static char *
strappend(char *where, char *newstr)
{
	(void) strcat(where, newstr);
	return (where + strlen(newstr));
}

static char *
convert_perm(char *where, o_mode_t perm)
{
	if (perm & 04)
		where = strappend(where, "r");
	else
		where = strappend(where, "-");
	if (perm & 02)
		where = strappend(where, "w");
	else
		where = strappend(where, "-");
	if (perm & 01)
		where = strappend(where, "x");
	else
		where = strappend(where, "-");
	/* perm is the last field */
	where = strappend(where, ",");
	return (where);
}
