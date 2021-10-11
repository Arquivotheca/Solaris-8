/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.1	99/08/13 SMI"

/*
 * Utility functions
 */
#include	<libintl.h>
#include	<stdio.h>
#include	<dlfcn.h>
#include	<string.h>
#include	<errno.h>
#include	"sgs.h"
#include	"rtc.h"
#include	"_crle.h"
#include	"msg.h"


/*
 * Append an item to the specified list, and return a pointer to the list
 * node created.
 */
Listnode *
list_append(List * lst, const void * item)
{
	Listnode *	lnp;

	if ((lnp = malloc(sizeof (Listnode))) == (Listnode *)0)
		return (0);

	lnp->data = (void *)item;
	lnp->next = NULL;

	if (lst->head == NULL)
		lst->tail = lst->head = lnp;
	else {
		lst->tail->next = lnp;
		lst->tail = lst->tail->next;
	}
	return (lnp);
}

/*
 * Add a library path. Multiple library paths are concatenated together into a
 * colon separated string suitable for runtime processing.
 */
int
addlib(Crle_desc * crle, char ** lib, const char * arg)
{
	char *		_str;
	size_t		_len, len = strlen(arg) + 1;

	if (*lib == 0) {
		if ((*lib = malloc(len)) == 0) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_MALLOC),
			    crle->c_name, strerror(err));
			return (1);
		}
		(void) strcpy(*lib, arg);
		crle->c_strsize += len;
		return (0);
	}

	/*
	 * If an original string exists, make sure this new string doesn't get
	 * duplicated.
	 */
	if ((_str = strstr(*lib, arg)) != NULL) {
		if (((_str == *lib) ||
		    (*(_str - 1) == *(MSG_ORIG(MSG_FMT_COLON)))) &&
		    (_str += len) &&
		    ((*_str == '\0') ||
		    (*_str == *(MSG_ORIG(MSG_FMT_COLON)))))
			return (0);
	}

	/*
	 * Create a concatenated string and free the old.
	 */
	_len = strlen(*lib);
	len += _len + 1;
	if ((_str = realloc((void *)*lib, len)) == 0) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_MALLOC),
		    crle->c_name, strerror(err));
		return (1);
	}
	(void) sprintf(&_str[_len], MSG_ORIG(MSG_FMT_COLON), arg);
	*lib = _str;
	crle->c_strsize += len - _len;
	return (0);
}


/*
 * -f option expansion.  Interpret its argument as a numeric or symbolic
 * representation of the dldump(3x) flags.
 */
int
dlflags(Crle_desc * crle, const char * arg)
{
	int		_flags;
	char *		tok, * _arg;
	const char *	separate = MSG_ORIG(MSG_MOD_SEPARATE);

	/*
	 * Scan the argument looking for allowable tokens.  First determine if
	 * the string is numeric, otherwise try and parse any known flags.
	 */
	if ((_flags = (int)strtol(arg, (char **)NULL, 0)) != 0)
		return (_flags);

	if ((_arg = malloc(strlen(arg) + 1)) == 0)
		return (0);
	(void) strcpy(_arg, arg);

	if ((tok = strtok(_arg, separate)) != NULL) {
		do {
		    if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_RELATIVE)) == 0)
			_flags |= RTLD_REL_RELATIVE;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_EXEC)) == 0)
			_flags |= RTLD_REL_EXEC;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_DEPENDS)) == 0)
			_flags |= RTLD_REL_DEPENDS;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_PRELOAD)) == 0)
			_flags |= RTLD_REL_PRELOAD;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_SELF)) == 0)
			_flags |= RTLD_REL_SELF;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_WEAK)) == 0)
			_flags |= RTLD_REL_WEAK;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_ALL)) == 0)
			_flags |= RTLD_REL_ALL;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_MEMORY)) == 0)
			_flags |= RTLD_MEMORY;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_STRIP)) == 0)
			_flags |= RTLD_STRIP;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_NOHEAP)) == 0)
			_flags |= RTLD_NOHEAP;
		    else if (strcmp(tok, MSG_ORIG(MSG_MOD_REL_CONFGEN)) == 0)
			_flags |= RTLD_CONFGEN;
		    else {
			(void) fprintf(stderr, MSG_INTL(MSG_ARG_FLAGS),
			    crle->c_name, tok);
			free(_arg);
			return (0);
		    }
		} while ((tok = strtok(NULL, separate)) != NULL);
	}
	if (_flags == 0)
		(void) fprintf(stderr, MSG_INTL(MSG_ARG_FLAGS),
		    crle->c_name, arg);

	free(_arg);
	return (_flags);
}

/*
 * Internationalization interface for sgsmsg(1l) use.
 */
const char *
_crle_msg(Msg mid)
{
	return (gettext(MSG_ORIG(mid)));
}
