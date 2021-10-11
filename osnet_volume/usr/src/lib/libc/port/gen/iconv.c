/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iconv.c	1.16	99/05/24 SMI"

/*LINTLIBRARY*/

#pragma weak iconv_open = _iconv_open
#pragma weak iconv_close = _iconv_close
#pragma weak iconv = _iconv

#include <synonyms.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "iconv.h"
#include "iconvP.h"
#include "../i18n/_loc_path.h"

#define	IMD	"UTF2"

static	iconv_p iconv_open_private(const char *, const char *);

/*
 * These functions are implemented using a shared object and the dlopen()
 * functions.   Then, the actual conversion  algorithm for a particular
 * conversion is implemented as a shared object in a separate file in
 * a loadable conversion module and linked dynamically at run time.
 * The loadable conversion module resides in
 *	/usr/lib/iconv/fromcode%tocode.so
 * where fromcode is the source encoding and tocode is the target encoding.
 * The module has 3 entries: _icv_open(), _icv_iconv(),  _icv_close().
 */

iconv_t
_iconv_open
(
	const char *tocode,
	const char *fromcode
)
{
	char 		lib_f[MAXPATHLEN];
	char 		lib_t[MAXPATHLEN];
	char		itm_f[MAXPATHLEN];
	char		itm_t[MAXPATHLEN];
	iconv_t 	cd;


	/*
	 * First, try using one direct conversion with
	 * lib, which is set to /usr/lib/iconv/fromcode%%tocode.so
	 * If direct conversion cannot be done, use the intermediate
	 * conversion.
	 */

	if ((cd = (iconv_t)malloc(sizeof (struct _iconv_info))) == NULL)
		return ((iconv_t)-1);

	(void) sprintf(lib_f, _ICONV_PATH, fromcode, tocode);
	(void) sprintf(itm_f, _GENICONVTBL_FILE, fromcode, tocode);
	cd->_to = NULL;

	if ((cd->_from = iconv_open_private(lib_f, itm_f)) == (iconv_p)-1) {
		(void) sprintf(lib_f, _ICONV_PATH,
			fromcode, IMD);
		(void) sprintf(lib_t, _ICONV_PATH, IMD, tocode);
		(void) sprintf(itm_f, _GENICONVTBL_FILE, fromcode, IMD);
		(void) sprintf(itm_t, _GENICONVTBL_FILE, IMD, tocode);

		if ((cd->_from = iconv_open_private(lib_f, itm_f))
		    == (iconv_p)-1) {
			free(cd);
			return ((iconv_t)-1);
		}
		if ((cd->_to = iconv_open_private(lib_t, itm_t))
		    == (iconv_p)-1) {
			free(cd->_from);
			free(cd);
			return ((iconv_t)-1);
		}
	}

	return (cd);
}

static char *
itm_exist_p(const char *itm_f, char *itm_path)
{
	int	itm_f_len;

	itm_f_len = strlen(itm_f);
	if ((itm_f_len + (sizeof (_GENICONVTBL_PATH))) >= MAXPATHLEN) {
		return (NULL);
	}
	sprintf(itm_path, _GENICONVTBL_PATH, itm_f);
	if (access(itm_path, R_OK) == 0) {
		return (itm_path);
	} else {
		return (NULL);
	}
}

static iconv_p
iconv_open_private(const char *lib, const char *itm)
{
	iconv_t (*fptr)(const char *);
	iconv_p cdpath;
	char	itm_path[MAXPATHLEN];

	if (itm_exist_p(itm, itm_path) != NULL) {
		lib = _GENICONVTBL_INT_PATH;
	}

	if ((cdpath = (iconv_p)malloc(sizeof (struct _iconv_fields))) == NULL)
		return ((iconv_p)-1);

	if ((cdpath->_icv_handle = dlopen(lib, RTLD_LAZY)) == 0) {
		errno = EINVAL;
		free(cdpath);
		return ((iconv_p)-1);
	}

	/* gets address of _icv_open */
	if ((fptr = (iconv_t(*)(const char *))
	    dlsym(cdpath->_icv_handle, "_icv_open"))
	    == NULL) {
		(void) dlclose(cdpath->_icv_handle);
		free(cdpath);
		return ((iconv_p)-1);
	}

	/*
	 * gets address of _icv_iconv in the loadable conversion module
	 * and stores it in cdpath->_icv_iconv
	 */

	if ((cdpath->_icv_iconv =
	    (size_t(*)(iconv_t, const char **, size_t *, char **, size_t *))
	    dlsym(cdpath->_icv_handle, "_icv_iconv")) == NULL) {
		(void) dlclose(cdpath->_icv_handle);
		free(cdpath);
		return ((iconv_p)-1);
	}

	/*
	 * gets address of _icv_close in the loadable conversion module
	 * and stores it in cd->_icv_close
	 */
	if ((cdpath->_icv_close = (void(*)(iconv_t)) dlsym(cdpath->_icv_handle,
				"_icv_close")) == NULL) {
		(void) dlclose(cdpath->_icv_handle);
		free(cdpath);
		return ((iconv_p)-1);
	}

	/* initialize the state of the actual _icv_iconv conversion routine */
	cdpath->_icv_state = (void *)(*fptr)(itm_path);
	if (cdpath->_icv_state == (struct _icv_state *)-1) {
		errno = ELIBACC;
		(void) dlclose(cdpath->_icv_handle);
		free(cdpath);
		return ((iconv_p)-1);
	}

	return (cdpath);
}

int
_iconv_close(iconv_t cd)
{
	if (cd == (iconv_t)NULL) {
		errno = EBADF;
		return (-1);
	}
	(*(cd->_from)->_icv_close)(cd->_from->_icv_state);
	(void) dlclose(cd->_from->_icv_handle);
	free(cd->_from);
	if (cd->_to != NULL) {
		(*(cd->_to)->_icv_close)(cd->_to->_icv_state);
		(void) dlclose(cd->_to->_icv_handle);
		free(cd->_to);
	}
	free(cd);
	return (0);
}

size_t
_iconv
(
	iconv_t cd,
	const char **inbuf,
	size_t *inbytesleft,
	char **outbuf,
	size_t *outbytesleft
)
{
	char 	tmp_buf[BUFSIZ];
	char	*tmp_ptr;
	size_t	tmp_size;
	size_t	ret;

	/* check if cd is valid */
	if (cd == (iconv_t)NULL) {
		errno = EBADF;
		return ((size_t)-1);
	}

	/* direct conversion */
	if (cd->_to == NULL)
		return ((*(cd->_from)->_icv_iconv)(cd->_from->_icv_state,
			inbuf, inbytesleft, outbuf, outbytesleft));

	/*
	 * use intermediate conversion codeset.  tmp_buf contains the
	 * results from the intermediate conversion.
	 *
	 * the premature or incomplete conversion will be done in the actual
	 * conversion routine itself.
	 */
	tmp_ptr = tmp_buf;
	tmp_size = BUFSIZ;	 /* number of bytes left in the output buffer */

	ret = (*(cd->_from)->_icv_iconv)(cd->_from->_icv_state, inbuf,
		inbytesleft, &tmp_ptr, &tmp_size);

	if (ret == (size_t)-1)
		return ((size_t)-1);
	else {
		tmp_ptr = tmp_buf;
		tmp_size = BUFSIZ - tmp_size;

		return ((*(cd->_to)->_icv_iconv)(cd->_to->_icv_state,
			(const char **) &tmp_ptr, &tmp_size,
			outbuf, outbytesleft));
	}
}
