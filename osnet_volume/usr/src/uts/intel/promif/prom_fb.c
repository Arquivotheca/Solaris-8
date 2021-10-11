/*
 * Copyright (c) 1998,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_fb.c	1.6	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)

/*
 * return true if stdout is the frame buffer
 */
int
prom_stdout_is_framebuffer(void)
{
	static int remember = -1;
	dnode_t options;
	static char screen[] = "screen";
	static char output_device[] = "output-device";
	char buf[32];

	if (remember != -1)
		return (remember);

	options = prom_optionsnode();
	if (options == OBP_NONODE || options == OBP_BADNODE) {
		remember = 0;
		return (0);
	}
	if (prom_getproplen(options, output_device) != sizeof (screen)) {
		remember = 0;
		return (0);
	}
	buf[0] = (char)0;
	(void) prom_getprop(options, output_device, buf);
	remember = ((prom_strcmp(screen, buf) == 0) ? 1 : 0);
	return (remember);
}
#endif
