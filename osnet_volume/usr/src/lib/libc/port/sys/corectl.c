/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)corectl.c	1.1	99/03/31 SMI"

#pragma weak core_set_options = _core_set_options
#pragma weak core_get_options = _core_get_options
#pragma weak core_set_global_path = _core_set_global_path
#pragma weak core_get_global_path = _core_get_global_path
#pragma weak core_set_process_path = _core_set_process_path
#pragma weak core_get_process_path = _core_get_process_path

#include	"synonyms.h"
#include	<sys/corectl.h>
#include	<sys/syscall.h>

int
core_set_options(int options)
{
	return (syscall(SYS_corectl, CC_SET_OPTIONS, options));
}

int
core_get_options(void)
{
	return (syscall(SYS_corectl, CC_GET_OPTIONS));
}

int
core_set_global_path(const char *buf, size_t bufsize)
{
	return (syscall(SYS_corectl, CC_SET_GLOBAL_PATH, buf, bufsize));
}

int
core_get_global_path(char *buf, size_t bufsize)
{
	return (syscall(SYS_corectl, CC_GET_GLOBAL_PATH, buf, bufsize));
}

int
core_set_process_path(const char *buf, size_t bufsize, pid_t pid)
{
	return (syscall(SYS_corectl, CC_SET_PROCESS_PATH, buf, bufsize, pid));
}

int
core_get_process_path(char *buf, size_t bufsize, pid_t pid)
{
	return (syscall(SYS_corectl, CC_GET_PROCESS_PATH, buf, bufsize, pid));
}
