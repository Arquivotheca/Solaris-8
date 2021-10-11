/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)audit.c	1.1	99/08/13 SMI"

/* LINTLIBRARY */

#include	<link.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<strings.h>
#include	<limits.h>
#include	"rtld.h"
#include	"_crle.h"
#include	"msg.h"

/*
 * This file provides the LD_AUDIT interfaces for libcrle.so.1, which are
 * called for one of two reasons:
 *
 * CRLE_AUD_DEPENDS
 *		under this mode the dependencies of the application are
 *		gathered (similar to ldd(1)) and written back to the calling
 *		process.
 *
 * CRLE_AUD_DLDUMP
 *		under this mode the LD_CONFIG file is read to determine which
 *		objects are to be dldump()'ed. The memory range occupied by
 *		the dumped images is written back to the calling process.
 *
 * Both of these interfaces are invoked via the crle(1) calling process.  The
 * following environment variables are used to communication between the two:
 *
 * CRLE_FD	the file descriptor on which to communicate to the calling
 *		process (used for CRLE_AUD_DEPENDS and CRLE_AUD_DUMP).
 *
 * CRLE_FLAGS 	this signals CRLE_AUD_DLDUMP and indicates the required flags
 *		for the dldump(3x) calls.
 */

static int	auflag;

int		pfd;
int		dlflag = RTLD_CONFSET;

/*
 * Initial audit handshake, establish audit mode.
 */
uint_t
/* ARGSUSED */
la_version(uint_t version)
{
	char *	str;

	/*
	 * Establish the file desciptor to communicate with the calling process,
	 * If there are any errors terminate the process.
	 */
	if ((str = getenv(MSG_ORIG(MSG_ENV_AUD_FD))) == NULL)
		exit(0);
	pfd = atoi(str);

	/*
	 * Determine which audit mode is required based on the existance of
	 * CRLE_FLAGS.
	 */
	if ((str = getenv(MSG_ORIG(MSG_ENV_AUD_FLAGS))) == NULL) {
		auflag = CRLE_AUD_DEPENDS;
	} else {
		auflag = CRLE_AUD_DLDUMP;
		dlflag |= atoi(str);

		/*
		 * Fill any memory holes before anything gets mapped.
		 */
		if (filladdr() != 0)
			exit(0);
	}

	/*
	 * We only need the basic audit interface.
	 */
	return (1);
}

/*
 * Audit interface called for each dependency.  If in CRLE_AUD_DEPENDS mode
 * return each dependency of the primary link-map to the caller.
 */
uint_t
/* ARGSUSED2 */
la_objopen(Link_map * lmp, Lmid_t lmid, uintptr_t * cookie)
{
	if (auflag == CRLE_AUD_DLDUMP)
		return (0);

	if ((lmid == LM_ID_BASE) && !(FLAGS((Rt_map *)lmp) & FLG_RT_ISMAIN)) {
		char	buffer[PATH_MAX];

		(void) sprintf(buffer, MSG_ORIG(MSG_AUD_PRF), lmp->l_name);
		(void) write(pfd, buffer, strlen(buffer));
	}
	return (0);
}

/*
 * Audit interface called before transfer of control to application.  If in
 * CRLE_AUD_DLDUMP mode read the configuration file and dldump() all necessary
 * objects.
 */
void
/* ARGSUSED */
la_preinit(uintptr_t * cookie)
{
	if (auflag == CRLE_AUD_DLDUMP) {
		if (dumpconfig() != 0)
			exit(1);
	}
	exit(0);
}
