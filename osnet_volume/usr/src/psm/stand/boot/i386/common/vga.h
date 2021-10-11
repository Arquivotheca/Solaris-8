/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VGA_H
#define	_VGA_H

#pragma ident	"@(#)vga.h	1.5	99/02/08 SMI"

/*
 * Interface to the bootstrap's internal VGA driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern void vga_setpos(int, int);
extern void vga_getpos(int *, int *);
extern void vga_clear(int);
extern void vga_scroll(int);
extern void vga_drawc(int, int);

#ifdef __cplusplus
}
#endif

#endif /* _VGA_H */
