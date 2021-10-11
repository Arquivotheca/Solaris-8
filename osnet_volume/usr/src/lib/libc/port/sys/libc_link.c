/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)libc_link.c 1.3     97/08/08 SMI"

#pragma	weak link = _link

#include "synonyms.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

extern	int __xpg4; /* defined in port/gen/xpg4.c; 0 if not xpg4/xpg4v2 */

extern int __link(const char *existing, const char *new);

int
link(const char *existing, const char *new)
{
	int 	sz;
	char 	linkbuf[MAXPATHLEN];
	struct  stat64 statbuf;

	/*
	 * XPG4v2 link() requires that the link count of a symbolic
	 * link target be updated rather than the link itself.  This
	 * matches SunOS 4.x and other BSD based implementations.
	 * However, the SVR4 merge apparently introduced the change
	 * that allowed link(src, dest) when "src" was a symbolic link,
	 * to create "dest" as a hard link to "src".  Hence, the link
	 * count of the symbolic link is updated rather than the target
	 * of the symbolic link. This latter behavior remains for
	 * non-XPG4 based environments. For a more detailed discussion,
	 * see bug 1256170.
	 */
	if (__xpg4 == 1) {
	    if (lstat64(existing, &statbuf) == 0) {
		if (S_ISLNK(statbuf.st_mode)) {
		    sz = (readlink(existing, linkbuf, MAXPATHLEN-1));
		    if (sz > 0) {
			linkbuf[sz] = '\0';
			existing = linkbuf;
		    }
		}
	    }
	}
	return (__link(existing, new));
}
