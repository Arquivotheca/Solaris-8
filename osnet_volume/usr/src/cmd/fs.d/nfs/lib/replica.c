/*
 *	replica.c
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

/*
 * Parse replicated server lists of the form:
 *
 *	host1:/path1,host2,host3,host4:/path2,host5:/path3
 *
 * into an array containing its constituent parts:
 *
 *	host1	/path1
 *	host2	/path2
 *	host3	/path2
 *	host4	/path2
 *	host5	/path3
 * Problems indicated by null return; they will be memory allocation
 * errors worthy of an error message unless count == -1, which means
 * a parse error.
 */

#pragma ident   "@(#)replica.c 1.4     96/06/14 SMI"

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include "replica.h"

struct replica *
parse_replica(char *special, int *count)
{
	struct replica *list = NULL;
	char *root, *special2, *p, *q, *r;

	*count = 0;
	root = special2 = strdup(special);

	while (root) {
		/*
		 * Find next colon separating location:/path pair
		 */
		p = strchr(root, ':');
		if (!p) {
			*count = -1;
			free(special2);
			return (NULL);
		}
		*p = '\0';

		/*
		 * Find comma (if present), which bounds the path
		 * The comma implies that the user is trying to
		 * specify failover syntax if another colon follows
		 */
		q = strchr(p+1, ',');
		if (q) {
			if (strchr(q+1, ':'))
				*q = '\0';
			else
				q = NULL;
		}
		(*count)++;
		list = realloc(list, *count * sizeof (struct replica));
		if (!list)
			goto bad;
		list[*count-1].path = strdup(p+1);
		if (!list[*count-1].path)
			goto bad;
		if (q)
			*q = ',';

		/*
		 * Pick up each host in the location list
		 */
		while (root) {
			r = strchr(root, ',');
			if (r)
				*r = '\0';
			list[*count-1].host = strdup(root);
			if (!list[*count-1].host)
				goto bad;
			if (r) {
				*r = ',';
				root = r+1;
				(*count)++;
				list = realloc(list,
				    *count * sizeof (struct replica));
				if (!list)
					goto bad;
				list[*count-1].path =
					strdup(list[*count-2].path);
				if (!list[*count-1].path)
					goto bad;
			} else
				root = NULL;
		}

		*p = ':';
		if (q)
			root = q+1;
		else
			root = NULL;
	}

	free(special2);
	return (list);
bad:
	if (list) {
		int i;

		for (i = 0; i < *count; i++) {
			if (list[i].host)
				free(list[i].host);
			if (list[i].path)
				free(list[i].path);
		}
		free(list);
	}
	free(special2);
	return (NULL);
}

void
free_replica(struct replica *list, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		free(list[i].host);
		free(list[i].path);
	}
	free(list);
}
