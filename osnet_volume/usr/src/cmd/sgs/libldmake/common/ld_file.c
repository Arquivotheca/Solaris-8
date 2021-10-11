/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)ld_file.c	1.7	98/08/28 SMI"

#pragma init(ld_support_init)

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <sys/param.h>
#include <link.h>

#define	SUNPRO_DEPENDENCIES	"SUNPRO_DEPENDENCIES"

/*
 * Linked list of strings - used to keep lists of names
 * of directories or files.
 */
struct Stritem {
	char *		str;
	void *		next;
};

typedef struct Stritem 	Stritem;

static char 		* depend_file = NULL;
static Stritem		* list = NULL;

void
ld_support_init()
{
	depend_file = getenv(SUNPRO_DEPENDENCIES);
}


static void
prepend_str(Stritem **list, const char * str)
{
	Stritem *	new;
	char 	*	newstr;
	const char *	lib = "libldmake.so";

	if (!(new = calloc(1, sizeof (Stritem)))) {
		perror(lib);
		return;
	}

	if (!(newstr = malloc(strlen(str) + 1))) {
		perror(lib);
		return;
	}

	new->str = strcpy(newstr, str);
	new->next = *list;
	*list = new;

}

/* ARGSUSED */
void
ld_file(const char * file, const Elf_Kind ekind, int flags, Elf *elf)
{
	/*
	 * SUNPRO_DEPENDENCIES wasn't set, we don't collect .make.state
	 * information.
	 */
	if (!depend_file)
		return;

	if ((flags & LD_SUP_DERIVED) && !(flags & LD_SUP_EXTRACTED))
		prepend_str(&list, file);
}

void
ld_file64(const char * file, const Elf_Kind ekind, int flags, Elf *elf)
{
	ld_file(file, ekind, flags, elf);
}


void
ld_atexit(int exit_code)
{
	Stritem 	* cur;
	char		  lockfile[MAXPATHLEN],	* err, * space, * target;
	FILE		* ofp;
	extern char 	* file_lock(char *, char *, int);

	if (!depend_file || exit_code)
		return;

	if ((space = strchr(depend_file, ' ')) == NULL)
		return;
	*space = '\0';
	target = &space[1];

	(void) sprintf(lockfile, "%s.lock", depend_file);
	if ((err = file_lock(depend_file, lockfile, 0))) {
		(void) fprintf(stderr, "%s\n", err);
		return;
	}

	if (!(ofp = fopen(depend_file, "a")))
		return;

	if (list)
		(void) fprintf(ofp, "%s: ", target);

	for (cur = list; cur; cur = cur->next)
		(void) fprintf(ofp, " %s", cur->str);

	(void) fputc('\n', ofp);

	(void) fclose(ofp);
	(void) unlink(lockfile);
	*space = ' ';
}

void
ld_atexit64(int exit_code)
{
	ld_atexit(exit_code);
}
