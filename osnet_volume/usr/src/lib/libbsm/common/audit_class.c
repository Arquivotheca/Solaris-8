#ifndef lint
static char	sccsid[] = "@(#)audit_class.c 1.14 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Interfaces to audit_class(5)  (/etc/security/audit_class)
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <string.h>
#include <synch.h>

static char	au_class_fname[PATH_MAX] = AUDITCLASSFILE;
static FILE *au_class_file = (FILE *) 0;
static mutex_t mutex_classfile = DEFAULTMUTEX;
static mutex_t mutex_classcache = DEFAULTMUTEX;

extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);

int
#ifdef __STDC__
setauclassfile(char *fname)
#else
setauclassfile(fname)
	char	*fname;
#endif
{
	_mutex_lock(&mutex_classfile);
	if (fname) {
		(void) strcpy(au_class_fname, fname);
	}
	_mutex_unlock(&mutex_classfile);
	return (0);
}


void
setauclass()
{
	_mutex_lock(&mutex_classfile);
	if (au_class_file) {
		(void) fseek(au_class_file, 0L, 0);
	}
	_mutex_unlock(&mutex_classfile);
	return;
}


void
endauclass()
{
	_mutex_lock(&mutex_classfile);
	if (au_class_file) {
		(void) fclose(au_class_file);
		au_class_file = (FILE *) 0;
	}
	_mutex_unlock(&mutex_classfile);
}

/*
 * getauclassent():
 *	This is not MT-safe because of the static variables.
 */
au_class_ent_t *
getauclassent()
{
	static au_class_ent_t e;
	static char	cname[AU_CLASS_NAME_MAX];
	static char	cdesc[AU_CLASS_DESC_MAX];

	e.ac_name = cname;
	e.ac_desc = cdesc;

	return (getauclassent_r(&e));
}

/*
 * getauclassent_r
 *	This is MT-safe if each thread passes in its own pointer
 *	to the space where the class entry is returned.  Becareful
 *	to also allocate space from the cname and cdesc pointers
 *	in the au_class_ent structure.
 */
au_class_ent_t *
getauclassent_r(au_class_entry)
	au_class_ent_t *au_class_entry;
{
	int	i, error = 0, found = 0;
	char	*s, input[256];

	if (au_class_entry == (au_class_ent_t *)NULL ||
		au_class_entry->ac_name == (char *)NULL ||
		au_class_entry->ac_desc == (char *)NULL) {
			return ((au_class_ent_t *)NULL);
	}

	/* open audit class file if it isn't already */
	_mutex_lock(&mutex_classfile);
	if (!au_class_file) {
		if (!(au_class_file = fopen(au_class_fname, "r"))) {
			_mutex_unlock(&mutex_classfile);
			return ((au_class_ent_t *)0);
		}
	}

	while (fgets(input, 256, au_class_file)) {
		if (input[0] != '#') {
			found = 1;
			s = input;

			/* parse bitfield */
			i = strcspn(s, ":");
			s[i] = '\0';
			if (!strncmp(s, "0x", 2)) {
				sscanf(&s[2], "%lx", &au_class_entry->ac_class);
			} else {
				sscanf(s, "%ld", &au_class_entry->ac_class);
			}
			s = &s[i+1];

			/* parse class name */
			i = strcspn(s, ":");
			s[i] = '\0';
			strncpy(au_class_entry->ac_name, s, AU_CLASS_NAME_MAX);
			s = &s[i+1];

			/* parse class description */
			i = strcspn(s, "\n\0");
			s[i] = '\0';
			strncpy(au_class_entry->ac_desc, s, AU_CLASS_DESC_MAX);

			break;
		}
	}

	_mutex_unlock(&mutex_classfile);

	if (!error && found) {
		return (au_class_entry);
	} else {
		return ((au_class_ent_t *)0);
	}
}


au_class_ent_t *
#ifdef __STDC__
getauclassnam(char *name)
#else
getauclassnam(name)
	char *name;
#endif
{
	static au_class_ent_t e;
	static char	cname[AU_CLASS_NAME_MAX];
	static char	cdesc[AU_CLASS_DESC_MAX];

	e.ac_name = cname;
	e.ac_desc = cdesc;

	return (getauclassnam_r(&e, name));
}

au_class_ent_t *
#ifdef __STDC__
getauclassnam_r(au_class_ent_t *e, char *name)
#else
getauclassnam_r()
	au_class_ent_t *e;
	char *name;
#endif
{
	while (getauclassent_r(e) != NULL) {
		if (strcmp(e->ac_name, name) == 0) {
			return (e);
		}
	}
	return ((au_class_ent_t *)NULL);
}


/* xcacheauclass:
 *	Read the entire audit_class file into memory.
 *	Return a pointer to the requested entry in the cache
 *	or a pointer to an invalid entry if the the class
 *	requested is not known.
 *
 *	Return < 0, do not set result pointer, if error.
 *	Return   0, set result pointer to invalid entry, if class not in cache
.
 *	Return   1, set result pointer to a valid entry, if class is in cache.
 */
static int
xcacheauclass(result, class_name, class_no, flags)
	au_class_ent_t **result; /* set this pointer to an entry in the cache */
	char	*class_name; /* name of class to look up */
	au_class_t class_no;
int	flags;
{
	static int	invalid;
	static au_class_ent_t **class_tbl;
	static int	called_once;
	static int	lines = 0;

	char	line[256];
	FILE * fp;
	au_class_ent_t * p_class;
	int	i;
	int	hit = 0;

	_mutex_lock(&mutex_classcache);
	if (called_once == 0) {

		/* Count number of liines in the class file */
		if ((fp = fopen(au_class_fname, "r")) == NULL) {
			_mutex_unlock(&mutex_classcache);
			return (-1);
		}
		while (fgets(line, 256, fp) != NULL) {
			if (line[0] != '#')
				lines++;
		}
		(void) fclose(fp);
		class_tbl = (au_class_ent_t * *)calloc((size_t)lines + 1,
			sizeof (au_class_ent_t));
		if (class_tbl == NULL) {
			_mutex_unlock(&mutex_classcache);
			return (-2);
		}

		lines = 0;
		setauclass();
		/*
		 * This call to getauclassent is protected by
		 * mutex_classcache, so we don't need to use the thread-
		 * safe version (getauclassent_r).
		 */
		while ((p_class = getauclassent()) != NULL) {
			class_tbl[lines] = (au_class_ent_t *)
				malloc(sizeof (au_class_ent_t));
			if (class_tbl[lines] == NULL) {
				_mutex_unlock(&mutex_classcache);
				return (-3);
			}
			class_tbl[lines]->ac_name = strdup(p_class->ac_name);
			class_tbl[lines]->ac_class = p_class->ac_class;
			class_tbl[lines]->ac_desc = strdup(p_class->ac_desc);
#ifdef DEBUG2
			printclass(class_tbl[lines]);
#endif
			lines++;
		}
		endauclass();
		invalid = lines;
		class_tbl[invalid] = (au_class_ent_t *)
			malloc(sizeof (au_class_ent_t));
		if (class_tbl[invalid] == NULL) {
			_mutex_unlock(&mutex_classcache);
			return (-4);
		}
		class_tbl[invalid]->ac_name = "invalid class";
		class_tbl[invalid]->ac_class = 0;
		class_tbl[invalid]->ac_desc = class_tbl[invalid]->ac_name;

		called_once = 1;

#ifdef DEBUG2
		for (i = 0; i <= lines; i++) {
			printclass(class_tbl[i]);
		}
#endif

	} /* END if called_once */
	*result = class_tbl[invalid];
	if (flags & AU_CACHE_NAME) {
		for (i = 0; i < lines; i++) {
			if (!strcmp(class_name, class_tbl[i]->ac_name)) {
				*result = class_tbl[i];
				hit = 1;
				break;
			}
		}
	} else if (flags & AU_CACHE_NUMBER) {
		for (i = 0; i < lines; i++) {
			if (class_no == class_tbl[i]->ac_class) {
				*result = class_tbl[i];
				hit = 1;
				break;
			}
		}
	}
	_mutex_unlock(&mutex_classcache);
	return (hit);
}


int
#ifdef __STDC__
cacheauclass(au_class_ent_t **result, au_class_t class_no)
#else
cacheauclass(result, class_no)
	au_class_ent_t **result; /* set this pointer to an entry in the cache */
	au_class_t class_no;
#endif
{
	return (xcacheauclass(result, "", class_no, AU_CACHE_NUMBER));
}


int
#ifdef __STDC__
cacheauclassnam(au_class_ent_t **result, char *class_name)
#else
cacheauclassnam(result, class_name)
	au_class_ent_t **result; /* set this pointer to an entry in the cache */
	char	*class_name;
#endif
{
	return (xcacheauclass(result, class_name, (au_class_t)0,
		AU_CACHE_NAME));
}


#ifdef DEBUG2
void
printclass(p_c)
au_class_ent_t *p_c;
{
	printf("%x:%s:%s\n", p_c->ac_class, p_c->ac_name, p_c->ac_desc);
	fflush(stdout);
}


#endif
