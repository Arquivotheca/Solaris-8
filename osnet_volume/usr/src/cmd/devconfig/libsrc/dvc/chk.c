/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)chk.c 1.6 95/02/28 SMI"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dvc.h"
#include "conf.h"
#include "util.h"

static int
getnum(char *str, char **strp, int *usint)
{
	unsigned int un = (int)strtoul(str, strp, 0);
	int n = strtol(str, strp, 0);
	char *ptr = str;

	if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'))
		*usint = TRUE;
	while (*str && (str == *strp)) {
		++str;
		un = (int)strtoul(str, strp, 0);
		n  = strtoul(str, strp, 0);
		if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'))
			*usint = TRUE;
	}

	if (*usint)
		return ((int)un);
	return (n);
}

/* Count the number of allowable choices in a numeric specification. */
int
count_numeric(char *fmt)
{
	int cnt = 0;
	int usint = 0;		   /* place holder required  */

	fmt += sizeof (NUMERIC_STRING) - 1; /* Point to spec part. */

	while (*fmt) {
		int a = getnum(fmt, &fmt, &usint);
		/* Check for a range of values. */
		if (*fmt == ':') {
			int b = getnum(fmt, &fmt, &usint);
			cnt += (b-a) + 1; /* Ranges are inclusive. */
		} else {
			++cnt;
			while (*fmt && ((*fmt == ',') || isspace(*fmt)))
				++fmt;
		}
	}
	return (cnt);
}

void
next_numeric(char **var_fmt, int *pn, int *usint)
{
	char *fmt = *var_fmt;
	int n;

	if (match(fmt, NUMERIC_STRING))
		fmt += sizeof (NUMERIC_STRING) - 1;

	if (*fmt == ':') {
		char *strp = fmt;
		n = getnum(fmt, &strp, usint);

		if (*pn < n) {
			++(*pn);
			return;
		}

		fmt = strp;
	}

	if (*fmt) {
		n = getnum(fmt, &fmt, usint);

		/* Check for a range of values. */
		if (*fmt != ':') {
			while (*fmt && ((*fmt == ',') || isspace(*fmt)))
				++fmt;
		}
	}
	*var_fmt = fmt;
	/* if (*usint) */
		/* *pn = (unsigned int) n; */
	*pn = n;
}

int
chk_num(char *chk, int num)
{
	char *cp;
	int usint;

	if (!match(chk, NUMERIC_STRING))
		return (0);
	/* Pointer to the rest of the match string. */
	cp = chk + sizeof (NUMERIC_STRING) - 1;

	while (*cp) {
		int a;
		a = getnum(cp, &cp, &usint);
		if (*cp == ':') {
			int b = getnum(cp, &cp, &usint);
			if (a <= num && num <= b)
				return (1);
		}
		if (a == num)
			return (1);
		while (*cp && ((*cp == ',') || isspace(*cp)))
			++cp;
	}

	return (0);
}

static char *
nextmatch(char **cp)
{
	char *c1 = *cp;
	char *ret = NULL;
	size_t size;

	if (*c1 == '\0')
		return (NULL);
	while (*c1 && (*c1 != ',')) {
		/* Escape comma if preceded with a backslash. */
		if (*c1 == '\\') {
			if ((*(c1+1) == ',') || (*(c1+1) == '\\'))
				++c1;
		}
		++c1;
	}
	size = c1 - *cp;

	if (*c1)
		++c1;

	if (size) {
		ret = (char *)xmalloc(size+1);
		ret[size] = 0;
		memcpy(ret, *cp, size);
	}

	*cp = c1;

	return (ret);
}

int
count_string(char *fmt)
{
	char *ccp;
	int	cnt = 0;

	if (match(fmt, STRING_STRING))
		fmt += sizeof (STRING_STRING) - 1;

	while (ccp = nextmatch(&fmt)) {
		xfree(ccp);
		++cnt;
	}

	return (cnt);
}

char *
next_string(char **var_fmt)
{
	if (match(*var_fmt, STRING_STRING))
		*var_fmt += sizeof (STRING_STRING) - 1;

	return (nextmatch(var_fmt));
}
