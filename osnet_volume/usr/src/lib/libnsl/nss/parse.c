/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)parse.c	1.1	99/04/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

char *_strpbrk_escape(char *, char *);

/*
 * _strtok_escape()
 *   Like strtok_r, except we don't break on a token if it is escaped
 *   with the escape character (\).
 */
char *
_strtok_escape(char *string, char *sepset, char **lasts)
{
	char	*q, *r;

	/* first or subsequent call */
	if (string == NULL)
		string = *lasts;

	if (string == 0)		/* return if no tokens remaining */
		return (NULL);

	if (*string == '\0')		/* return if no tokens remaining */
		return (NULL);

	if ((r = _strpbrk_escape(string, sepset)) == NULL)/* move past token */
		*lasts = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		*lasts = r+1;
	}
	return (string);
}

/*
 * Return ptr to first occurrence of any non-escaped character from `brkset'
 * in the character string `string'; NULL if none exists.
 */
char *
_strpbrk_escape(char *string, char *brkset)
{
	const char *p;
	const char *first = string;

	do {
		for (p = brkset; *p != '\0' && *p != *string; ++p)
			;
		if (p == string)
			return ((char *)string);
		if (*p != '\0') {
			if (*(string-1) != '\\')
				return ((char *)string);
		}
	} while (*string++);

	return (NULL);
}


char   *
_escape(char *s, char *esc)
{
	int	nescs = 0;	/* number of escapes to place in s */
	int	i, j;
	int	len_s;
	char	*tmp;

	if (s == NULL || esc == NULL)
		return (NULL);

	len_s = strlen(s);
	for (i = 0; i < len_s; i++)
		if (strchr(esc, s[i]))
			nescs++;
	if ((tmp = (char *) malloc(nescs + len_s + 1)) == NULL)
		return (NULL);
	for (i = 0, j = 0; i < len_s; i++) {
		if (strchr(esc, s[i])) {
			tmp[j++] = '\\';
		}
		tmp[j++] = s[i];
	}
	tmp[len_s + nescs] = '\0';
	return (tmp);
}


char *
_unescape(char *s, char *esc)
{
	int	len_s;
	int	i, j;
	char	*tmp;

	if (s == NULL || esc == NULL)
		return ((char *)NULL);

	len_s = strlen(s);
	if ((tmp = (char *) malloc(len_s + 1)) == NULL)
		return ((char *)NULL);
	for (i = 0, j = 0; i < len_s; i++) {
		if (s[i] == '\\' && strchr(esc, s[i + 1]))
			tmp[j++] = s[++i];
		else
			tmp[j++] = s[i];
	}
	tmp[j] = NULL;
	return (tmp);
}

char *
_strdup_null(char *s)
{
	return (strdup(s ? s : ""));
}


/*
 * read a line into buffer from a mmap'ed file.
 * return length of line read.
 */
int
_readbufline(char *mapbuf,	/* input mmap buffer */
    int mapsize,		/* input size */
    char *buffer,		/* output storage */
    int buflen,			/* output size */
    int *lastlen)		/* input read till here last time */
{
	int	linelen;

	while (1) {
		linelen = 0;
		while (linelen < buflen - 1) {	/* "- 1" saves room for \n\0 */
			if (*lastlen >= mapsize) {
				if (linelen == 0 ||
					buffer[linelen - 1] == '\\') {
						return (-1);
					} else {
						buffer[linelen] = '\n';
						buffer[linelen + 1] = '\0';
						return (linelen);
					}
			}
			switch (mapbuf[*lastlen]) {
			case '\n':
				(*lastlen)++;
				if (linelen > 0 &&
				    buffer[linelen - 1] == '\\') {
					--linelen;	/* remove the '\\' */
				} else {
					buffer[linelen] = '\n';
					buffer[linelen + 1] = '\0';
					return (linelen);
				}
				break;
			default:
				buffer[linelen] = mapbuf[*lastlen];
				(*lastlen)++;
				linelen++;
			}
		}
		/* Buffer overflow -- eat rest of line and loop again */
		while (mapbuf[*lastlen] != '\n') {
			if (mapbuf[*lastlen] == EOF) {
				return (-1);
			}
			(*lastlen)++;
		};
	}
}
