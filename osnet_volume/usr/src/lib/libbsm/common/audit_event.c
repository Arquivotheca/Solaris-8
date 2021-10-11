#ifndef lint
static char	sccsid[] = "@(#)audit_event.c 1.23 99/08/30 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Interfaces to audit_event(5)  (/etc/security/audit_event)
 */

/*
 * This routine is obsolete.  I have removed its inclusion by removing
 * the .o from the makefile.  Please use cacheauevent() or any of the
 * getauev* routines.
 */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <synch.h>

static au_class_t flagstohex(char *);

static char	au_event_fname[PATH_MAX] = AUDITEVENTFILE;
static FILE *au_event_file = (FILE *)0;
static mutex_t mutex_eventfile = DEFAULTMUTEX;
static mutex_t mutex_eventcache = DEFAULTMUTEX;

extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);

int
#ifdef __STDC__
setaueventfile(char *fname)
#else
setaueventfile(fname)
	char	*fname;
#endif
{
	_mutex_lock(&mutex_eventfile);
	if (fname) {
		(void) strcpy(au_event_fname, fname);
	}
	_mutex_unlock(&mutex_eventfile);
	return (0);
}


void
setauevent()
{
	_mutex_lock(&mutex_eventfile);
	if (au_event_file) {
		(void) fseek(au_event_file, 0L, 0);
	}
	_mutex_unlock(&mutex_eventfile);
}


void
endauevent()
{
	_mutex_lock(&mutex_eventfile);
	if (au_event_file) {
		(void) fclose(au_event_file);
		au_event_file = (FILE *) 0;
	}
	_mutex_unlock(&mutex_eventfile);
}

au_event_ent_t *
getauevent()
{
	static au_event_ent_t au_event_entry;
	static char	ename[AU_EVENT_NAME_MAX];
	static char	edesc[AU_EVENT_DESC_MAX];

	/* initialize au_event_entry structure */
	au_event_entry.ae_name = ename;
	au_event_entry.ae_desc = edesc;

	return (getauevent_r(&au_event_entry));
}

au_event_ent_t *
getauevent_r(au_event_entry)
	au_event_ent_t *au_event_entry;
{
	int	i, error = 0, found = 0;
	char	*s, input[AU_EVENT_LINE_MAX];
	char	trim_buf[AU_EVENT_NAME_MAX];
	int	empty_len;

	/* open audit event file if it isn't already */
	_mutex_lock(&mutex_eventfile);
	if (!au_event_file)
		if (!(au_event_file = fopen(au_event_fname, "r"))) {
			_mutex_unlock(&mutex_eventfile);
			return ((au_event_ent_t *)0);
		}

	while (fgets(input, AU_EVENT_LINE_MAX, au_event_file)) {
		if (input[0] != '#') {
			empty_len = strspn(input, " \t\r\n");
			if (empty_len == strlen(input) ||
				input[empty_len] == '#') {
				continue;
			}
			found = 1;
			s = input;

			/* parse number */
			i = strcspn(s, ":");
			s[i] = '\0';
			(void) sscanf(s, "%hd", &au_event_entry->ae_number);
			s = &s[i+1];

			/* parse event name */
			i = strcspn(s, ":");
			s[i] = '\0';
			sscanf(s, "%s", trim_buf);
			(void) strncpy(au_event_entry->ae_name, trim_buf,
				AU_EVENT_NAME_MAX);
			s = &s[i+1];

			/* parse event description */
			i = strcspn(s, ":");
			s[i] = '\0';
			(void) strncpy(au_event_entry->ae_desc, s,
				AU_EVENT_DESC_MAX);
			s = &s[i+1];

			/* parse class */
			i = strcspn(s, "\n\0");
			s[i] = '\0';
			sscanf(s, "%s", trim_buf);
			au_event_entry->ae_class = flagstohex(trim_buf);

			break;
		}
	}
	_mutex_unlock(&mutex_eventfile);

	if (!error && found) {
		return (au_event_entry);
	} else {
		return ((au_event_ent_t *)0);
	}
}


au_event_ent_t *
#ifdef __STDC__
getauevnam(char *name)
#else
getauevnam(name)
	char *name;
#endif
{
	static au_event_ent_t au_event_entry;
	static char	ename[AU_EVENT_NAME_MAX];
	static char	edesc[AU_EVENT_DESC_MAX];

	/* initialize au_event_entry structure */
	au_event_entry.ae_name = ename;
	au_event_entry.ae_desc = edesc;

	return (getauevnam_r(&au_event_entry, name));
}

au_event_ent_t *
#ifdef __STDC__
getauevnam_r(au_event_ent_t *e, char *name)
#else
getauevnam_r(e, name)
	au_event_ent_t &e;
	char *name;
#endif
{
	setauevent();
	while (getauevent_r(e) != NULL) {
		if (strcmp(e->ae_name, name) == 0) {
			endauevent();
			return (e);
		}
	}
	endauevent();
	return ((au_event_ent_t *)NULL);
}

au_event_ent_t *
#ifdef __STDC__
getauevnum_r(au_event_ent_t *e, au_event_t event_number)
#else
getauevnum_r(e, event_number)
	au_event_ent_t *e;
	au_event_t event_number;
#endif
{
	setauevent();
	while (getauevent_r(e) != NULL) {
		if (e->ae_number == event_number) {
			endauevent();
			return (e);
		}
	}
	endauevent();
	return ((au_event_ent_t *)NULL);
}

au_event_ent_t *
#ifdef __STDC__
getauevnum(au_event_t event_number)
#else
getauevnum(event_number)
	au_event_t event_number;
#endif
{
	static au_event_ent_t e;
	static char	ename[AU_EVENT_NAME_MAX];
	static char	edesc[AU_EVENT_DESC_MAX];

	/* initialize au_event_entry structure */
	e.ae_name = ename;
	e.ae_desc = edesc;

	return (getauevnum_r(&e, event_number));
}

au_event_t
#ifdef __STDC__
getauevnonam(char *event_name)
#else
getauevnonam(event_name)
	char	*event_name;
#endif
{
	au_event_ent_t e;
	char ename[AU_EVENT_NAME_MAX];
	char edesc[AU_EVENT_DESC_MAX];

	/* initialize au_event_entry structure */
	e.ae_name = ename;
	e.ae_desc = edesc;

	if (getauevnam_r(&e, event_name) == (au_event_ent_t *)0) {
		return (-1);
	}
	return (e.ae_number);
}

/*
 * cacheauevent:
 *	Read the entire audit_event file into memory.
 *	Set a pointer to the requested entry in the cache
 *	or a pointer to an invalid entry if the event number
 *	is not known.
 *
 *	Return < 0, if error.
 *	Return   0, if event number not in cache.
 *	Return   1, if event number is in cache.
 */

int
#ifdef __STDC__
cacheauevent(au_event_ent_t **result, au_event_t event_number)
#else
cacheauevent(result, event_number)
	au_event_ent_t **result; /* set this pointer to an entry in the cache */
	au_event_t event_number; /* request this event number */
#endif
{
	static u_short	max; /* the highest event number in the file */
	static u_short	min; /* the lowest event number in the file */
	static int	invalid; /* 1+index of the highest event number */
	static au_event_ent_t **index_tbl;
	static au_event_ent_t **p_tbl;
	static int	called_once = 0;

	char	line[AU_EVENT_LINE_MAX];
	int	lines = 0;
	FILE * fp;
	au_event_ent_t * p_event;
	int	i, size;
	int	hit = 0;

	_mutex_lock(&mutex_eventcache);
	if (called_once == 0) {

		/* Count number of lines in the events file */
		if ((fp = fopen(au_event_fname, "r")) == NULL) {
			_mutex_unlock(&mutex_eventcache);
			return (-1);
		}
		while (fgets(line, AU_EVENT_LINE_MAX, fp) != NULL) {
			if (line[0] != '#')
				lines++;
		}
		(void) fclose(fp);
		size = lines;

		/*
		 * Make an array in which each element in an entry in the
		 * events file.  Make the next to last element an invalid
		 * event.  Make the last element a NULL pointer.
		 */

		p_tbl = (au_event_ent_t * *)calloc(lines + 1,
			sizeof (au_event_ent_t));
		if (p_tbl == NULL) {
			_mutex_unlock(&mutex_eventcache);
			return (-2);
		}
		lines = 0;
		max = 0;
		min = 65535;
		setauevent();
		while ((p_event = getauevent()) != NULL) {
			p_tbl[lines] = (au_event_ent_t *)
				malloc(sizeof (au_event_ent_t));
			if (p_tbl[lines] == NULL) {
				_mutex_unlock(&mutex_eventcache);
				return (-3);
			}
			p_tbl[lines]->ae_number = p_event->ae_number;
			p_tbl[lines]->ae_name   = strdup(p_event->ae_name);
			p_tbl[lines]->ae_desc   = strdup(p_event->ae_desc);
			p_tbl[lines]->ae_class  = p_event->ae_class;
#ifdef DEBUG2
			printevent(p_tbl[lines]);
#endif
			if ((u_short) p_event->ae_number > max) {
				max = p_event->ae_number;
			}
			if ((u_short) p_event->ae_number < min) {
				min = p_event->ae_number;
			}
			lines++;
		}
		endauevent();
		invalid = lines;
		p_tbl[invalid] = (au_event_ent_t *)
			malloc(sizeof (au_event_ent_t));
		if (p_tbl[invalid] == NULL) {
			_mutex_unlock(&mutex_eventcache);
			return (-4);
		}
		p_tbl[invalid]->ae_number = -1;
		p_tbl[invalid]->ae_name   = "invalid event number";
		p_tbl[invalid]->ae_desc   = p_tbl[invalid]->ae_name;
		p_tbl[invalid]->ae_class  = (au_class_t)-1;

#ifdef DEBUG2
		for (i = 0; i < size; i++) {
			printf("%d:%s:%s:%d\n", p_tbl[i]->ae_number,
				p_tbl[i]->ae_name, p_tbl[i]->ae_desc,
				p_tbl[i]->ae_class);
		}
#endif

		/* get space for the index_tbl */
		index_tbl = (au_event_ent_t * *)
			calloc(max+1, sizeof (au_event_ent_t *));
		if (index_tbl == NULL) {
			_mutex_unlock(&mutex_eventcache);
			return (-5);
		}

		/* intialize the index_tbl to the invalid event number */
		for (i = 0; (u_short)i < max; i++) {
			index_tbl[i] = p_tbl[invalid];
		}

		/* point each index_tbl element at the corresponding event */
		for (i = 0; i < size; i++) {
			index_tbl[(u_short) p_tbl[i]->ae_number] = p_tbl[i];
		}

		called_once = 1;

	}

	if ((u_short) event_number > max || (u_short) event_number < min) {
		*result = index_tbl[invalid];
	} else {
		*result = index_tbl[(u_short) event_number];
		hit = 1;
	}
	_mutex_unlock(&mutex_eventcache);
	return (hit);
}


static au_class_t
flagstohex(char *flags)
{
	au_class_ent_t * p_class;
	unsigned int	hex = 0;
	char	*comma = ",";
	char	*s;

	s = strtok(flags, comma);
	while (s != NULL) {
		cacheauclassnam(&p_class, s);
		hex |= p_class->ac_class;
		s = strtok(NULL, comma);
	}
	return (hex);
}


#ifdef DEBUG2
void
printevent(p_event)
au_event_ent_t *p_event;
{
	printf("%d:%s:%s:%d\n", p_event->ae_number, p_event->ae_name,
		p_event->ae_desc, p_event->ae_class);
	fflush(stdout);
}


#endif
