/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)demangle.c	1.6	98/10/09 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "dem.h"

/*
 * C++ Demangling
 */
#define	LIBDEMANGLE	"libdemangle.so.1"
#define	DEMANGLEFUNC	"cplus_demangle"

extern char *cafe_demangle(char *, char *);

/*
 * This is a backup routine which uses routine from CAFE
 * project. The -3 value is returned when the demangling
 * did not succeed.
 * (The -1 value is intentionally not used.)
 */
/*ARGSUSED*/
static int
sgs_cafe_demangle(char *name, char *demangled_name, int limit)
{
	char *cafe_out;
	DEM dem_struct;
	int dem_ret_val;

	cafe_out = cafe_demangle(
			(char *)name,
			(char *)demangled_name);

	if (cafe_out != name) {
		return (0);
	}

	dem_ret_val = dem(name, &dem_struct, demangled_name);

	if (dem_ret_val < 0)
		return (-3);

	return (0);
}

/*
 *
 */
char *
sgs_demangle(char *name)
{
	static char *demangled_name;
	static int (*demangle_func)() = 0;
	static int first_flag = 0;
	static size = MAXDBUF;
	int ret;

	/*
	 * If this is the first time called,
	 * decide which demangling function to use.
	 */
	if (first_flag == 0) {
		void *demangle_hand;

		demangle_hand = dlopen(LIBDEMANGLE, RTLD_LAZY);
		if (demangle_hand != NULL)
			demangle_func = (int (*)(int))dlsym(
				demangle_hand, DEMANGLEFUNC);

		if (demangle_func == NULL)
			demangle_func = sgs_cafe_demangle;

		/*
		 * Allocate the buffer
		 */
		demangled_name = (char *) malloc(size);
		if (demangled_name == NULL)
			return (name);

		first_flag = 1;
	}

	/*
	 * If malloc() failed in the previous call,
	 * demangle_name is NULL. So the following codes are
	 * here.
	 */
	if (demangled_name == NULL) {
		size = MAXDBUF;
		demangled_name = (char *) malloc(size);
		if (demangled_name == NULL)
			return (name);
	}

	/*
	 * When we use the real one.
	 * The real function returns -1 when the buffer size
	 * is not sufficient.
	 *
	 * When we use the back up function, it never returns -1.
	 */
	while ((ret = (*demangle_func)(name, demangled_name, size)) == -1) {
		free(demangled_name);
		size = size + MAXDBUF;
		demangled_name = (char *) malloc(size);
		if (demangled_name == NULL)
			return (name);
	}

	if (ret != 0)
		return (name);
	return (demangled_name);
}
