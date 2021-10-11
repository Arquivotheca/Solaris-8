/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _KEYBOARD_H
#define	_KEYBOARD_H

#pragma ident	"@(#)keyboard.h	1.1	99/10/29 SMI"

/*
 * Interfaces to the miniature keyboard driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

int kb_ischar(_char_io_p);
char kb_getchar(_char_io_p);

#ifdef __cplusplus
}
#endif

#endif /* _KEYBOARD_H */
