/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)compat.cc	1.1	96/03/31 SMI"

#include <synch.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <syslog.h>
#include <stdio.h>

extern "C" void
fn_locked_dlsym(const char *func_name, void **func)
{
	static mutex_t	libxfn_lock = DEFAULTMUTEX;
	static mutex_t	sym_lock = DEFAULTMUTEX;
	static void	*libxfn = NULL;
	void *sym = NULL;

	if (*func != NULL)
		return;

	if (libxfn == NULL) {
		mutex_lock(&libxfn_lock);
		if (libxfn == NULL) {
			libxfn = dlopen("libxfn.so.2", RTLD_LAZY);
			if (libxfn == NULL) {
				syslog(LOG_ERR,
				    "dlopen(\"libxfn.so.2\") failed");
				fprintf(stderr,
				    "dlopen(\"libxfn.so.2\") failed.\n");
				abort();
			}
		}
		mutex_unlock(&libxfn_lock);
	}
	sym = dlsym(libxfn, func_name);
	if (sym == NULL) {
		syslog(LOG_ERR, "dlsym(\"%s\") in libxfn.so.2 failed",
		    func_name);
		fprintf(stderr, "dlsym(\"%s\") in libxfn.so.2 failed.\n",
		    func_name);
		abort();
	}

	if (*func == NULL) {
		mutex_lock(&sym_lock);
		*func = sym;
		mutex_unlock(&sym_lock);
	}
}
