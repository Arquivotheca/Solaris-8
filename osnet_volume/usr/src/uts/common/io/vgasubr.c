/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vgasubr.c	1.46	99/07/14 SMI"

/*
 * Support routines for VGA drivers
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <sys/vgareg.h>
#include <sys/vgasubr.h>
#include <sys/cmn_err.h>

#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/devops.h>
#include <sys/sunddi.h>

#include <sys/modctl.h>

#define	GET_HORIZ_END(c)	vga_get_crtc(c, VGA_CRTC_H_D_END)
#define	GET_VERT_END(c)	(vga_get_crtc(c, VGA_CRTC_VDE) \
	+ (((vga_get_crtc(c, VGA_CRTC_OVFL_REG) >> \
	    VGA_CRTC_OVFL_REG_VDE8) & 1) << 8) \
	+ (((vga_get_crtc(c, VGA_CRTC_OVFL_REG) >> \
	    VGA_CRTC_OVFL_REG_VDE9) & 1) << 9))

#define	GET_VERT_X2(c)	\
	(vga_get_crtc(c, VGA_CRTC_CRT_MD) & VGA_CRTC_CRT_MD_VT_X2)

void
vga_get_hardware_settings(struct vgaregmap *reg, int *width, int *height)
{
	*width = (GET_HORIZ_END(reg)+1)*8;
	*height = GET_VERT_END(reg)+1;
	if (GET_VERT_X2(reg)) *height *= 2;
}

#define	PUTB(reg, off, v) ddi_io_put8(reg->handle, reg->addr + (off), v)
#define	GETB(reg, off) ddi_io_get8(reg->handle, reg->addr + (off))

int
vga_get_crtc(struct vgaregmap *reg, int i)
{
	return (vga_get_indexed(reg, VGA_CRTC_ADR, VGA_CRTC_DATA, i));
}

void
vga_set_crtc(struct vgaregmap *reg, int i, int v)
{
	vga_set_indexed(reg, VGA_CRTC_ADR, VGA_CRTC_DATA, i, v);
}

int
vga_get_seq(struct vgaregmap *reg, int i)
{
	return (vga_get_indexed(reg, VGA_SEQ_ADR, VGA_SEQ_DATA, i));
}

void
vga_set_seq(struct vgaregmap *reg, int i, int v)
{
	vga_set_indexed(reg, VGA_SEQ_ADR, VGA_SEQ_DATA, i, v);
}

int
vga_get_grc(struct vgaregmap *reg, int i)
{
	return (vga_get_indexed(reg, VGA_GRC_ADR, VGA_GRC_DATA, i));
}

void
vga_set_grc(struct vgaregmap *reg, int i, int v)
{
	vga_set_indexed(reg, VGA_GRC_ADR, VGA_GRC_DATA, i, v);
}

int
vga_get_atr(struct vgaregmap *reg, int i)
{
	int ret;

	(void) GETB(reg, CGA_STAT);
	PUTB(reg, VGA_ATR_AD, i);
	ret = GETB(reg, VGA_ATR_DATA);

	(void) GETB(reg, CGA_STAT);
	PUTB(reg, VGA_ATR_AD, VGA_ATR_ENB_PLT);

	return (ret);
}

void
vga_set_atr(struct vgaregmap *reg, int i, int v)
{
	(void) GETB(reg, CGA_STAT);
	PUTB(reg, VGA_ATR_AD, i);
	PUTB(reg, VGA_ATR_AD, v);

	(void) GETB(reg, CGA_STAT);
	PUTB(reg, VGA_ATR_AD, VGA_ATR_ENB_PLT);
}

void
vga_set_indexed(
	struct vgaregmap *reg,
	int indexreg,
	int datareg,
	unsigned char index,
	unsigned char val)
{
	PUTB(reg, indexreg, index);
	PUTB(reg, datareg, val);
}

int
vga_get_indexed(
	struct vgaregmap *reg,
	int indexreg,
	int datareg,
	unsigned char index)
{
	PUTB(reg, indexreg, index);
	return (GETB(reg, datareg));
}

/*
 * VGA DAC access functions
 * Note:  These assume a VGA-style 6-bit DAC.  Some DACs are 8 bits
 * wide.  These functions are not appropriate for those DACs.
 */
void
vga_put_cmap(
	struct vgaregmap *reg,
	int index,
	unsigned char r,
	unsigned char g,
	unsigned char b)
{

	PUTB(reg, VGA_DAC_WR_AD, index);
	PUTB(reg, VGA_DAC_DATA, r >> 2);
	PUTB(reg, VGA_DAC_DATA, g >> 2);
	PUTB(reg, VGA_DAC_DATA, b >> 2);
}

void
vga_get_cmap(
	struct vgaregmap *reg,
	int index,
	unsigned char *r,
	unsigned char *g,
	unsigned char *b)
{
	PUTB(reg, VGA_DAC_RD_AD, index);
	*r = GETB(reg, VGA_DAC_DATA) << 2;
	*g = GETB(reg, VGA_DAC_DATA) << 2;
	*b = GETB(reg, VGA_DAC_DATA) << 2;
}

#if	defined(DEBUG)
void
vga_dump_regs(struct vgaregmap *reg,
	int maxseq, int maxcrtc, int maxatr, int maxgrc)
{
	int i, j;

	printf("Sequencer regs:\n");
	for (i = 0; i < maxseq; i += 0x10) {
		printf("%2x:  ", i);
		for (j = 0; j < 0x08; j++) {
			printf("%2x ", vga_get_seq(reg, i+j));
		}
		printf("- ");
		for (; j < 0x10; j++) {
			printf("%2x ", vga_get_seq(reg, i+j));
		}
		printf("\n");
	}
	printf("\nCRT Controller regs:\n");
	for (i = 0; i < maxcrtc; i += 0x10) {
		printf("%2x:  ", i);
		for (j = 0; j < 0x08; j++) {
			printf("%2x ", vga_get_crtc(reg, i+j));
		}
		printf("- ");
		for (; j < 0x10; j++) {
			printf("%2x ", vga_get_crtc(reg, i+j));
		}
		printf("\n");
	}
	printf("\nAttribute Controller regs:\n");
	for (i = 0; i < maxatr; i += 0x10) {
		printf("%2x:  ", i);
		for (j = 0; j < 0x08; j++) {
			printf("%2x ", vga_get_atr(reg, i+j));
		}
		printf("- ");
		for (; j < 0x10; j++) {
			printf("%2x ", vga_get_atr(reg, i+j));
		}
		printf("\n");
	}
	printf("\nGraphics Controller regs:\n");
	for (i = 0; i < maxgrc; i += 0x10) {
		printf("%2x:  ", i);
		for (j = 0; j < 0x08; j++) {
			printf("%2x ", vga_get_grc(reg, i+j));
		}
		printf("- ");
		for (; j < 0x10; j++) {
			printf("%2x ", vga_get_grc(reg, i+j));
		}
		printf("\n");
	}
}
#endif
