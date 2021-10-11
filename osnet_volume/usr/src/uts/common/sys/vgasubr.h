/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_VGASUBR_H
#define	_SYS_VGASUBR_H

#pragma ident	"@(#)vgasubr.h	1.6	99/07/14 SMI"	/* SunOS4.1 1.11 */

#ifdef	__cplusplus
extern "C" {
#endif

struct vgaregmap {
	uint8_t			*addr;
	ddi_acc_handle_t	handle;
	boolean_t		mapped;
};

extern int vga_get_crtc(struct vgaregmap *reg, int i);
extern void vga_set_crtc(struct vgaregmap *reg, int i, int v);
extern int vga_get_seq(struct vgaregmap *reg, int i);
extern void vga_set_seq(struct vgaregmap *reg, int i, int v);
extern int vga_get_grc(struct vgaregmap *reg, int i);
extern void vga_set_grc(struct vgaregmap *reg, int i, int v);
extern int vga_get_atr(struct vgaregmap *reg, int i);
extern void vga_set_atr(struct vgaregmap *reg, int i, int v);
extern void vga_put_cmap(struct vgaregmap *reg,
	int index, unsigned char r, unsigned char g, unsigned char b);
extern void vga_get_cmap(struct vgaregmap *reg,
	int index, unsigned char *r, unsigned char *g, unsigned char *b);
extern void vga_get_hardware_settings(struct vgaregmap *reg,
	int *width, int *height);
extern void vga_set_indexed(struct vgaregmap *reg, int indexreg,
	int datareg, unsigned char index, unsigned char val);
extern int vga_get_indexed(struct vgaregmap *reg, int indexreg,
	int datareg, unsigned char index);
#if	defined(DEBUG)
extern void vga_dump_regs(struct vgaregmap *reg,
	int maxseq, int maxcrtc, int maxatr, int maxgrc);
#endif


#ifdef	__cplusplus
}
#endif

#endif /* _SYS_VGASUBR_H */
