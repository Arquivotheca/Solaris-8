/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)testprop.c	1.3	96/01/10 SMI"

/* Program to be built with bootprop.c for creating test program */

#include <stdio.h>

struct memlist *vfreelistp, *pfreelistp, *pinstalledp;
char *v2path, *v2args, *kernname, *systype, *my_own_name = "boot";
int	vac;
int	cache_state;

int ret;
char buffer[100];
char *retp;
extern char *bnextprop();

main()
{
	dolen("whoami");

	doget("whoami");

	donext("whoami");

	donext("memory-update");

	doset("newprop", "newval");

	dolen("newprop");

	doget("newprop");

	donext("memory-update");

	doset("new2", "val2");
	dolen("new2");
	doget("new2");
	donext("new2");
	donext("newprop");

	doset("newprop", "wow");
	dolen("newprop");
	doget("newprop");
	donext("new2");

	doset("whoami", "BOOT");
	doget("whoami");
}

dolen(s)
char *s;
{
	int ret;

	ret = bgetproplen(0, s);
	printf("bgetproplen(\"%s\") returned %d\n", s, ret);
}

doget(s)
char *s;
{
	ret = bgetprop(0, s, buffer);
	printf("bgetprop(\"%s\") returned %d\n", s, ret);
	if (ret == 0)
		printf("value for \"%s\" was \"%s\"\n", s, buffer);
}

donext(s)
char *s;
{
	retp = bnextprop(0, s);
	printf("bnextprop(\"%s\") gives \"%s\"\n",
		s, retp ? retp : "(null)");
}

doset(s, v)
char *s;
char *v;
{
	ret = bsetprop(0, s, v);
	printf("bsetprop(\"%s\", \"%s\") returned %d\n", s, v, ret);
}

update_memlist() {}

kmem_alloc(n)
{
	return (malloc(n));
}

kmem_free(loc, n)
{
	free(loc);
}
