/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_kbd.c	1.4	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)
/*
 * Return true if stdin is the keyboard
 * Return false if we can't check for some reason
 */
int
prom_stdin_is_keyboard(void)
{
	static int remember = -1;
	dnode_t options;
	static char input_device[] = "input-device";
	static char keyboard[] = "keyboard";
	char buf[16];

	if (remember != -1)
		return (remember);

	options = prom_optionsnode();
	if (prom_getproplen(options, input_device) != sizeof (keyboard)) {
		remember = 0;
		return (0);
	}

	buf[0] = (char)0;
	(void) prom_getprop(options, input_device, buf);

	remember = (prom_strcmp(buf, keyboard) == 0) ? 1 : 0;
	return (remember);
}
#endif
