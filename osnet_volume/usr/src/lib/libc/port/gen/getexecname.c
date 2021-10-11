/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)getexecname.c 1.1	97/05/02 SMI"

/*LINTLIBRARY*/

#pragma weak getexecname = _getexecname
#include "synonyms.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/auxv.h>

/*
 * Get auxiliary entry.
 * Returns pointer to entry, or 0 if entry does not exist.
 */
static auxv_t *
_getaux(int type)
{
	static auxv_t *auxb = 0;
	static size_t nauxv = 0;
	ssize_t i;

	/*
	 * The first time through, read the initial aux vector that was
	 * passed to the process at exec(2).  Only do this once.
	 */
	if (auxb == 0) {
		struct stat statb;
		int fd = open("/proc/self/auxv", O_RDONLY);

		if (fd < 0 || fstat(fd, &statb) != 0)
			statb.st_size = 0;
		auxb = malloc(statb.st_size + sizeof (auxv_t));
		if (fd < 0 || (i = read(fd, auxb, statb.st_size)) < 0)
			i = 0;
		nauxv = i / sizeof (auxv_t);
		auxb[nauxv].a_type = AT_NULL;
		if (fd >= 0)
			(void) close(fd);
	}

	/*
	 * Scan the auxiliary entries looking for the required type.
	 */
	for (i = 0; i < nauxv; i++)
		if (auxb[i].a_type == type)
			return (&auxb[i]);

	/*
	 * No auxiliary array (static executable) or entry not found.
	 */
	return ((auxv_t *)0);
}

#ifdef	NOT_NEEDED_YET
static long
_getauxval(int type)
{
	auxv_t *auxp;

	if ((auxp = _getaux(type)) != (auxv_t *)0)
		return (auxp->a_un.a_val);
	else
		return (0);
}
#endif

static void *
_getauxptr(int type)
{
	auxv_t *auxp;

	if ((auxp = _getaux(type)) != (auxv_t *)0)
		return (auxp->a_un.a_ptr);
	else
		return (0);
}

/*
 * Return the pointer to the fully-resolved path name of the process's
 * executable file obtained from the AT_SUN_EXECNAME aux vector entry.
 */
const char *
getexecname(void)
{
	return ((const char *)_getauxptr(AT_SUN_EXECNAME));
}
