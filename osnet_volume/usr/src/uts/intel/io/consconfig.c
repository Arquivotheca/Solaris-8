/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consconfig.c	1.41	99/05/19 SMI"

/*
 * Console and mouse configuration
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/klwp.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/debug.h>
#include <sys/reboot.h>
#include <sys/termios.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>

#include <sys/strsubr.h>
#include <sys/errno.h>
#include <sys/devops.h>
#include <sys/note.h>

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_miscops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "console configuration 1.41"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

char _depends_on[] = "dacf/consconfig_dacf";

typedef		void (*consconfig_func)();

/*
 * Configure keyboard and mouse. Main entry here.
 */
void
consconfig(void)
{
	consconfig_func		consconfig_function;

	/*
	 * The depends_on will have forced a modload of consconfig_dacf
	 */
	consconfig_function =
		(consconfig_func)modlookup("consconfig_dacf",
			"dynamic_console_config");

	if (consconfig_function != NULL) {

		(*consconfig_function)();
	}
}
