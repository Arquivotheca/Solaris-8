/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CG8REG_H
#define	_SYS_CG8REG_H

#pragma ident	"@(#)cg8reg.h	1.10	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	cg8reg.h:
 *	This file had two incarnations, one for the original P4 TC
 *	(IBIS frame) buffer and one for various flavors of the
 *	SBus-based TC Card.  Only the latter remains.
 */

/*
 * Some initial definitions common to all versions of SBus TC Card
 */

#ifdef TC1REG_DEBUG
#include <sys/types.h>
#include <stdio.h>
#include <sys/pixrect.h>
#endif	/* TC1REG_DEBUG */

#include <sys/param.h>

/* Frame Buffer Attributes Description (from SBus TC Card on-board PROMs) */
typedef struct	{	/* Fb attributes (from on-board PROM): */
	uint_t	selection_enable:1; /* 1 if there's a selection enable bit. */
	uint_t	pip_possible:1;	/* 1 if pip option possible. */
	uint_t	simultaneous_8_24:1; /* 1 if sep. luts for 8-bit & 24-bit. */
	uint_t	monochrome:1;	/* 1 if monochrome frame buffer present. */
	uint_t	selection:1;	/* 1 if selection memory present. */
	uint_t	true_color:1;	/* 1 if true color frame buffer present. */
	uint_t	eight_bit_hardware:1; /* 1 if eight bit frame buffer present */
	uint_t	unused:9;	/* Unused flag area. */
	uint_t	model:8;	/* Frame buffer model. */
	uint_t	depth:8;	/* Default frame buffer's depth (= 1). */
}	FB_Attribute_Flags;

#define	RASTEROPS_TC	1	/* Value "model": TC 1152x900 card. */
#define	RASTEROPS_TCP	2	/* Value "model": TCP 1152x900 card. */
#define	RASTEROPS_TCS	3	/* Value "model": TCS 640x480 card. */
#define	RASTEROPS_TCL	4	/* Value "model": TCL 1280x1024 card. */

typedef union {	/* Construction for fb attributes: */
	FB_Attribute_Flags	flags;	/* Representation as flags. */
	int	integer;	/* Representation as integer. */
}	FB_Attributes;


/*
 * Board layout of SBus TCP Card (24-bit frame buffer with Picture in a
 * Picture option.
 */
#define	TC_OMAP_SIZE	4	/* Monochrome lookup table size. */
#define	TC_CMAP_SIZE	256	/* 24-bit lookup table sizes. */
/* #define TC_RAMDAC_CMAPSIZE TC_CMAP_SIZE */ /* Alias used by pixrect code. */

#define	TCP_NPLL 16	/* Number of pll registers. */

typedef union {	/* PLL register map: */
	uchar_t	vector[TCP_NPLL]; /* ... registers as a vector. */
	struct	{		/* ... individual register names. */
	uchar_t	nh_low;		/* ... ... low 8 bits of NH register. */
	uchar_t	nh_mid_low;	/* ... ... next to low order 8 bits of NH */
	uchar_t	nh_mid_high;	/* ... ... next to high order 8 bits of NH */
	uchar_t	nh_high;	/* ... ... high order 8 bits of NH. */
	uchar_t	r_low;		/* ... ... low 8 bits of R register. */
	uchar_t	r_middle;	/* ... ... middle 8 bits of R register. */
	uchar_t	r_high;		/* ... ... high order 8 bits of R. */
	uchar_t	s;		/* ... ... s register. */
	uchar_t	l;		/* ... ... l register. */
	uchar_t	p;		/* ... ... p register. */
	uchar_t	ttl_lpf;	/* ... ... ttl/lpf register. */
	uchar_t	enable;		/* ... ... pll enable register. */
	uchar_t	vco_iring;	/* ... ... vco/iring register. */
	uchar_t	rl_clock;	/* ... ... rl clock register. */
	uchar_t	s_delay;	/* ... ... s delay register. */
	uchar_t	g_delay;	/* ... ... g delay register. */
	}	registers;
}	PLL_Regs;

/*
 *	Brooktree RAMDAC address layouts:
 */
typedef struct {	/* Brooktree 457 layout: */
	uchar_t	all_address;	/* All 3 RAMDACs' address registers */
	uchar_t	all_color;	/* All 3 RAMDACs' color value registers */
	uchar_t	all_control;	/* All 3 RAMDACs' control registers */
	uchar_t	all_overlay;	/* All 3 RAMDACs' overlay value registers */

	uchar_t	red_address;	/* Red gun RAMDAC's address register */
	uchar_t	red_color;	/* Red gun RAMDAC's color value register */
	uchar_t	red_control;	/* Red gun RAMDAC's control register */
	uchar_t	red_overlay;	/* Red gun RAMDAC's overlay value register */

	uchar_t	green_address;	/* Green gun RAMDAC's address register */
	uchar_t	green_color;	/* Green gun RAMDAC's color value register */
	uchar_t	green_control;	/* Green gun RAMDAC's control register */
	uchar_t	green_overlay;	/* Green gun RAMDAC's overlay value register */

	uchar_t	blue_address;	/* Blue gun RAMDAC's address register */
	uchar_t	blue_color;	/* Blue gun RAMDAC's color value register */
	uchar_t	blue_control;	/* Blue gun RAMDAC's control register */
	uchar_t	blue_overlay;	/* Blue gun RAMDAC's overlay value register */
} Bt457_Regs;

typedef struct {	/* Brooktree 463 layout: */
	uchar_t	address_low;	/* Address register low order bits. */
	uchar_t	address_high;	/* Address register high order bits. */
	uchar_t	control;	/* Control/data register. */
	uchar_t	color;		/* Color palette register. */
	uchar_t	fill[12];	/* Match up to size of Bt 457 layout. */
} Bt463_Regs;

typedef struct {	/* Brooktree 473 layout: */
	uchar_t	ram_write_addr;	/* Address register: writes to color palette */
	uchar_t	color;		/* Data register for color palette. */
	uchar_t	read_mask;	/* Pixel read mask register. */
	uchar_t	ram_read_addr;	/* Address register: read from color palette */
	uchar_t	overlay_write_addr; /* Address register: writes overlay */
				/* palette. */
	uchar_t	overlay;	/* Data register for overlay. */
	uchar_t	control;	/* Control register. */
	uchar_t	overlay_read_addr; /* Address register: reads overlay */
				/* palette. */
} Bt473_Regs;

typedef struct {	/* Venus frame buffer controller chip layout: */
	uchar_t	control1;		/* 00: control register 1. */
	uchar_t	control2;		/* 01: control register 2. */
	uchar_t	control3;		/* 02: control register 3. */
	uchar_t	control4;		/* 03: control register 4. */
	uchar_t	status;			/* 04: status register. */
	uchar_t	refresh_interval;	/* 05: refresh interval: */
	ushort_t io_config;		/* 06: general i/o config. */

	uint_t	display_start;		/* 08: display start. */
	uint_t	half_row_incr;		/* 0C: half row increment. */

	uint_t	display_pitch;		/* 10: display pitch. */
	uchar_t	cas_mask;		/* 14: CAS mask. */
	uchar_t	horiz_latency;		/* 15: horizontal latency. */

	ushort_t horiz_end_sync;	/* 16: horizontal end sync. */
	ushort_t horiz_end_blank;	/* 18: horizontal end blank. */
	ushort_t horiz_start_blank;	/* 1A: horizontal start blank. */
	ushort_t horiz_total;		/* 1C: horizontal total. */
	ushort_t horiz_half_line;	/* 1E: horizontal half_line. */
	ushort_t horiz_count_load;	/* 20: horizontal count_load. */

	uchar_t	vert_end_sync;		/* 22: vertical end_sync. */
	uchar_t	vert_end_blank;		/* 23: vertical end_blank. */
	ushort_t vert_start_blank;	/* 24: vertical start blank. */
	ushort_t vert_total;		/* 26: vertical total. */
	ushort_t vert_count_load;	/* 28: vertical count_load. */
	ushort_t vert_interrupt;	/* 2a: vertical interrupt line. */

	ushort_t io_general;		/* 2c: general i/o. */
	uchar_t	y_zoom;			/* 2e: Y Zoom register. */
	uchar_t	soft_register;		/* 2f: soft register. */
} Venus_Regs;

#define	VENUS_TCS_MODEL_A	0x8000	/* io_general: version A of tcs card */
#define	VENUS_TIMING_MASK	0x6000	/* io_general: timing switch values */
#define	VENUS_TIMING_NATIVE	0x0000	/* ...	native (apple) timing. */
#define	VENUS_TIMING_NTSC	0x2000	/* ...	ntsc timing. */
#define	VENUS_TIMING_PAL	0x4000	/* ...	pal timing. */
#define	VENUS_NO_GENLOCK	0x0020	/* io_general: no genlock source. */
#define	VENUS_SOFT_RESET	0x0010	/* io_general: software reset. */

#define	VENUS_VERT_INT_ENA	1	/* control4: vert. interrupt enabled. */
#define	VENUS_VERT_INT		1	/* status: vert. interrupt pending. */

/*
 * The device as presented by the "mmap" system call.  It seems to the mmap
 * user that the board begins, at its 0 offset, with the overlay plane,
 * followed by the enable plane and the color framebuffer.  At 8MB, there
 * is the ramdac followed by the p4 register and the boot prom.
 */
#define	CG8_VADDR_FB	0
#define	CG8_VADDR_DAC	0x800000

/*
 * Layout of device registers for controlling the SBus TCS Card:
 */
typedef struct {
	Venus_Regs	tc_venus;	/* Venus chip controlling true color */
	uchar_t		fill1[0x040-sizeof (Venus_Regs)];
	Venus_Regs	mono_venus;	/* Venus chip controlling */
					/*  selection/monochrome. */
	uchar_t		fill2[0x040-sizeof (Venus_Regs)];
	Bt473_Regs	dac;		/* Brootree 473 RAMDAC. */
} Tcs_Device_Map;

/*
 * Layout of device registers for controlling the SBus TC, TCP, and
 * TCL Cards
 */
typedef struct	{
	struct mfb_reg	sun_mfb_reg;	/* Standard Sun memory frame */
					/* buffer registers */
	union			 /* RAMDAC support: */
	{
	Bt457_Regs	bt457;	 /* ... Brooktree 457 (TC, TCP cards.) */
	Bt463_Regs	bt463;	 /* ... Brooktree 463 (TCL cards.) */
	}	dacs;
	uchar_t	control_status;	 /* TC control/status register 0. */
	uchar_t	control_status1; /* TC control/status register 1. */
	uchar_t	fill1[14];	 /* Unused. */
	ushort_t x_source_start; /* X sig start (subset of incoming signal) */
	ushort_t x_source_end;	 /* X sig end (subset of incoming signal). */
	ushort_t x_source_scale; /* X sig scaling:pixel drop rate multiplier */
	ushort_t fill2;		 /* Unused. */
	ushort_t y_source_start; /* Y sig start (subset of incoming signal) */
	ushort_t y_source_end;	 /* Y sig end (subset of incoming signal). */
	ushort_t y_source_scale; /* Y sig scaling:scan line drop rate mult. */
	ushort_t fb_pitch;	 /* # pixels(visible & not) bet. scan lines */
	ulong_t	pip_start_offset; /* Starting offset in fb to start PIP at. */
	uchar_t	control_status2; /* TC control/status register 2. */
	uchar_t	fill3[11];	 /* Unused. */
	PLL_Regs pll;		 /* PLL registers. */
} Tc1_Device_Map;

/*
 * Register "addresses" for Brooktree internal registers. These are written to
 * the address register (xxx_address) to select the internal register to be
 * accessed by reads or writes to the control register (xxx_control).
 */
#define	BT457_BLINK_MASK	5	/* Blink mask register. */
#define	BT457_COMMAND		6	/* Command register. */
#define	BT457_CONTROL		7	/* Control / test register. */
#define	BT457_READ_MASK		4	/* Read mask register. */

/*
 * Old style TC card control and status register bit definitions.
 */
#define	TC1_SELECTION_ENABLE	0x80	/* Turn on selection memory. */


/*
 * Control Status Register 0 definitions:
 */
#define	TCP_CSR0_PIP_ONE_SHOT	0x40	/* Set for one-shot frame capture */
#define	TCP_CSR0_PIP_IS_ON	0x40	/* 1 on read: pip is turned on. */
#define	TCP_CSR0_PIP_IS_ACTIVE	0x20	/* 1 on read: actively gen. images */
#define	TCP_CSR0_TURN_PIP_ON	0x20	/* Set for pip active, 0 to stop */
#define	TCP_CSR0_PIP_INSTALLED	0x10	/* = 1 if pip present in system. */
#define	TCP_CSR0_SOURCE_TYPE_MASK 0x03	/* Mask to get video source value. */
#define	TCP_CSR0_COMPOSITE_SOURCE 0x00	/* ... source is composite (1 wire) */
#define	TCP_CSR0_S_VIDEO_SOURCE	0x01	/* ... source is s-video (2 wire.) */
#define	TCP_CSR0_RGB_SOURCE	0x02	/* ... source is rgb (3 wire.) */

/*
 * Control Status Register 1 definitions:
 */
#define	TCP_CSR1_I2C_DATA	0x80	/* I2C bus interface ser. data bit */
#define	TCP_CSR1_I2C_CLOCK	0x40	/* I2C bus interface clock bit. */
#define	TCP_CSR1_FIELD_ONLY	0x10	/* 1 to make image w/only 1 field. */
#define	TCP_CSR1_NO_PIP		0x02	/* 1 if there is no pip present. */
#define	TCP_CSR1_INPUT_CONNECTED 0x01	/* 1 if pip input connected. */

/*
 * Control Status Register 2 definitions:
 */
#define	TCP_CSR2_ALTERNATE_PLL	0x01	/* 1 to use VCR PLL (slow timing.) */
#define	TCP_CSR2_COUNT_DOWN	0x02	/* 1 if addr counts down (hor flip) */
#define	TCP_CSR2_FIELD_INVERT	0x04	/* 1 to show odd before even field. */

/*
 * I-squared C bus device addresses and command definitions:
 */
#define	I2C_ADDR_EEPROM (uint_t)0xa0	/* Dev. address: eeprom for confg. */
#define	I2C_ADDR_DACS	(uint_t)0x88	/* Dev. address: d to a converters */
#define	I2C_DAC0_CMD	(uint_t)0x00	/* Com: d to a converter 0 control */
#define	I2C_DAC1_CMD	(uint_t)0x01	/* Com: d to a converter 1 control */
#define	I2C_DAC2_CMD	(uint_t)0x02	/* Com: d to a converter 2 control */
#define	I2C_DAC3_CMD	(uint_t)0x03	/* Com: d to a converter 3 control */
#define	I2C_POD_CMD	(uint_t)0x08	/* Com: port output data control. */
#define	I2C_POD_NTSC	(uint_t)0x00	/* Data: ntsc timing - port out data */
#define	I2C_POD_PAL	(uint_t)0x14	/* Data: pal timing - port out data */

#define	I2C_BRIGHTNESS	I2C_DAC0_CMD	/* Com: access brightness control. */
#define	I2C_CONTRAST	I2C_DAC1_CMD	/* Com: access contrast control. */
#define	I2C_HUE		I2C_DAC3_CMD	/* Com: access hue control. */
#define	I2C_SATURATION	I2C_DAC2_CMD	/* Com: access saturation control. */
#define	I2C_HALF_LEVEL	(uint_t)0x20	/* Data: half level for d/a conv. */

/*
 *	Layout of EEPROM Storage and Related Structures:
 */
typedef struct {	/* PIP Initialization record: */
	uchar_t	timing_mode;	/* ... timing mode specification. */

	uchar_t	brightness;	/* ... d to a converter brightness value. */
	uchar_t	contrast;	/* ... d to a converter contrast value. */
	uchar_t	saturation;	/* ... d to a converter saturation value. */
	uchar_t	hue;		/* ... d to a converter hue value. */

	uchar_t	fill;		/* ... *** UNUSED *** */

	ushort_t x_source_start; /* ... starting pixel of signal in x */
	ushort_t x_source_end;	/* ... ending pixel of signal in x */
	ushort_t y_source_start; /* ... starting pixel of signal in y */
	ushort_t y_source_end;	/* ... ending pixel of signal in y */

	uchar_t	rgb_brightness; /* ... d to a converter rgb brightness. */
	uchar_t	rgb_contrast;	/* ... d to a converter rgb brightness. */

	PLL_Regs pll;		/* ... phase lock loop initialization values. */
}	Pip_Init_Record;

#define	EEPROM_FACTORY	-1	/* Use factory timing def. (loc. 0 in eeprom) */
#define	EEPROM_NTSC	 0	/* Default timing mode is NTSC. */
#define	EEPROM_PAL	 1	/* Default timing mode is PAL. */
#define	EEPROM_NUM_MODES EEPROM_PAL+1

typedef struct {	/* EEPROM storage layout: */
	uchar_t		default_mode;	/* ... Default mode selection. */
	uchar_t		fill[15];	/* ... *** UNUSED *** */
	Pip_Init_Record mode[EEPROM_NUM_MODES];
					/* ... NTSC, PAL timing defaults. */
	PLL_Regs	d1[EEPROM_NUM_MODES];
					/* ... Digital 1 pll timing info. */
	PLL_Regs	d2[EEPROM_NUM_MODES];
					/* ... Digital 2 pll timing info. */
}	EEPROM_Record;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CG8REG_H */
