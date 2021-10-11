/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)arch.c	1.7	99/05/04 SMI"

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<sys/systeminfo.h>
#include	<sys/param.h>
#include	<sys/auxv.h>
#include	"_conv.h"
#include	"arch_msg.h"


/*
 * For compilation on pre-2.6 machines.
 */
#ifndef SI_ISALIST
#define	SI_ISALIST		514
#endif

#ifndef	AT_SUN_EXECNAME
#define	AT_SUN_EXECNAME		2014
#endif


/*
 * Global "conv_arch64_name" is the 64-bit ISA associated
 * with the current architecture (e.g. sparcv9, ia64)
 */
#if	defined(sparc)
const char *conv_arch64_name = MSG_ORIG(MSG_ARCH_SPARCV9);
#elif	defined(i386) || defined(__ia64)
const char *conv_arch64_name = MSG_ORIG(MSG_ARCH_IA64);
#else
#error	unknown architecture!
#endif


/*
 * Determine if the 32-bit or 64-bit kernel is running.
 * Return the corresponding EI_CLASS constant.
 */
int
conv_sys_eclass()
{
	char buf[BUFSIZ];

	/*
	 * SI_ISALIST will return -1 on pre-2.6 machines,
	 * which is fine - it can't be a 64-bit kernel.
	 */
	if (sysinfo(SI_ISALIST, buf, BUFSIZ) == -1)
		return (ELFCLASS32);

	if (strstr(buf, conv_arch64_name) != NULL)
		return (ELFCLASS64);

	return (ELFCLASS32);
}

#if	defined(_LP64)
/* ARGSUSED */
void
conv_check_native(char **argv, char **envp, char *sub_name)
{
	/* 64-bit version does nothing */
}

#else
/*
 * Find the start of the aux vector to get at the execname.
 * Note, this is faster then getexecname.3c which uses /proc
 * to get the aux vector.
 */
static int
get_auxv(char **envp, int type, auxv_t *auxvp)
{
	char		**ptr;
	auxv_t		*vp;

	/*
	 * First we find the auxv - it's above below the list
	 * of envp on the stack.  So - walk through all the
	 * envps first;
	 */
	for (ptr = envp; *ptr; ptr++)
		;
	/*
	 * Step past 0 word
	 */
	ptr++;
	for (vp = (auxv_t *)ptr; vp->a_type != AT_NULL; vp++) {
		if (vp->a_type == type) {
			*auxvp = *vp;
			return (1);
		}
	}
	return (0);
}


/*
 * If successfully exec's a 64-bit binary, this doesn't
 * return.  If not, it's the caller's job to handle it.
 */
void
conv_check_native(char **argv, char **envp, char *sub_name)
{
	char *name;
	char *ev;
	char *av0;
	char buf[MAXPATHLEN];
	auxv_t aux;

	/*
	 * LD_NOEXEC_64 defined in the environment provides
	 * a way to prevent the exec.  This is used by the
	 * test suite to test 32-bit support libraries.
	 */
	if (((ev = getenv(MSG_ORIG(MSG_LD_NOEXEC64))) != NULL) && *ev)
		return;

	if (conv_sys_eclass() != ELFCLASS64)
		return;

	if (get_auxv(envp, AT_SUN_EXECNAME, &aux) == 0)
		return;

	/*
	 * Backup from ".../prog" and look for a `prog' in
	 * a 64-bit directory (e.g. ".../sparcv9/prog")
	 */
	(void) strcpy(buf, (const char *)aux.a_un.a_ptr);
	if (name = strrchr(buf, '/'))
		name++;
	else
		name = buf;
	*name = NULL;

	/*
	 * Tack on the 64-bit arch name (e.g "sparcv9")
	 * and put the basename back at the end.
	 */
	(void) strcpy(name, conv_arch64_name);
	if (sub_name != NULL)
		name = sub_name;
	else if (name = strrchr((const char *)aux.a_un.a_ptr, '/'))
		name++;
	else
		name = (char *)aux.a_un.a_ptr;

	(void) strcat(buf, MSG_ORIG(MSG_STR_SLASH));
	(void) strcat(buf, name);

	/*
	 * If the exec fails, return and let the native
	 * 32-bit binary handle it.
	 */
	av0 = argv[0];
	argv[0] = buf;
	if (execv(buf, argv) == -1)
		argv[0] = av0;
}
#endif /* !defined(_LP64) */
