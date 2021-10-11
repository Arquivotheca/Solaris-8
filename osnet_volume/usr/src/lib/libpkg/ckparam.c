/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)ckparam.c	1.7	93/10/06 SMI"	/* SVr4.0  1.2 */

#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <pkglib.h>
#include "pkglocale.h"

#define	ERR_LEN		"length of parameter value <%s> exceeds limit"
#define	ERR_ASCII	"parameter <%s> must be ascii"
#define	ERR_ALNUM	"parameter <%s> must be alphanumeric"
#define	ERR_CHAR	"parameter <%s> has incorrect first character"
#define	ERR_UNDEF	"parameter <%s> cannot be null"

#define	MAXLEN 256
#define	TOKLEN 16

static int	proc_name(char *param, char *value);
static int	proc_arch(char *param, char *value);
static int	proc_version(char *param, char *value);
static int	proc_category(char *param, char *value);
static int	bad_first_char(char *param, char *value);
static int	not_alnum(char *param, char *pt);
static int	not_ascii(char *param, char *pt);
static int	too_long(char *param, char *pt, int len);
static int	isnull(char *param, char *pt);

int
ckparam(char *param, char *val)
{
	char *token;
	char *value = strdup(val);
	int ret_val = 0;	/* return value */

	if (strcmp(param, "NAME") == 0)
		ret_val = proc_name(param, value);

	else if (strcmp(param, "ARCH") == 0)
		ret_val = proc_arch(param, value);

	else if (strcmp(param, "VERSION") == 0)
		ret_val = proc_version(param, value);

	else if (strcmp(param, "CATEGORY") == 0)
		ret_val = proc_category(param, value);

	/* param does not match existing parameters */
	free(value);
	return (ret_val);
}

static int
proc_name(char *param, char *value)
{
	int ret_val;

	if (!(ret_val = isnull(param, value))) {
		ret_val += too_long(param, value, MAXLEN);
		ret_val += not_ascii(param, value);
	}

	return (ret_val);
}

static int
proc_arch(char *param, char *value)
{
	int ret_val;
	char *token;

	if (!(ret_val = isnull(param, value))) {
		token = strtok(value, ", ");

		while (token) {
			ret_val += too_long(param, token, TOKLEN);
			ret_val += not_ascii(param, token);
			token = strtok(NULL, ", ");
		}
	}

	return (ret_val);
}

static int
proc_version(char *param, char *value)
{
	int ret_val;

	if (!(ret_val = isnull(param, value))) {
		ret_val += bad_first_char(param, value);
		ret_val += too_long(param, value, MAXLEN);
		ret_val += not_ascii(param, value);
	}

	return (ret_val);
}

static int
proc_category(char *param, char *value)
{
	int ret_val;
	char *token;

	if (!(ret_val = isnull(param, value))) {
		token = strtok(value, ", ");

		while (token) {
			ret_val += too_long(param, token, TOKLEN);
			ret_val += not_alnum(param, token);
			token = strtok(NULL, ", ");
		}
	}

	return (ret_val);
}

static int
bad_first_char(char *param, char *value)
{
	if (*value == '(') {
		progerr(pkg_gt(ERR_CHAR), param);
		return (1);
	}

	return (0);
}

static int
isnull(char *param, char *pt)
{
	if (!pt || *pt == '\0') {
		progerr(pkg_gt(ERR_UNDEF), param);
		return (1);
	}
	return (0);
}

static int
too_long(char *param, char *pt, int len)
{
	if (strlen(pt) > (size_t) len) {
		progerr(pkg_gt(ERR_LEN), pt);
		return (1);
	}
	return (0);
}

static int
not_ascii(char *param, char *pt)
{
	while (*pt) {
		if (!(isascii(*pt))) {
			progerr(pkg_gt(ERR_ASCII), param);
			return (1);
		}
		pt++;
	}
	return (0);
}

static int
not_alnum(char *param, char *pt)
{
	while (*pt) {
		if (!(isalnum(*pt))) {
			progerr(pkg_gt(ERR_ALNUM), param);
			return (1);
		}
		pt++;
	}

	return (0);
}
