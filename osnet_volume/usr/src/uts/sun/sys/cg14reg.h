/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CG14REG_H
#define	_SYS_CG14REG_H

#pragma ident	"@(#)cg14reg.h	1.11	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Memory Display Interface (MDI) chip register defintions, ioctl(2) commands,
 * and data types. MDI is integrated into the Video SIMM on Campus-II family
 * of workstations and is a high performance video display controller featuring
 * upto three Color Look Up Tables (CLUTs), hardware cursor support, transparent
 * overlay support with blending and fully programmatic monitor timing. The
 * MDI processes 8-bit greyscale, 8-bit psuedo color, 16-bit (8+8) and 32-bit
 * pixels.
 * Combined with 4MB VRAM, a 3x8 bit Digital To Analog Converter (DAC), a pixel
 * clock and the SMC memory controller the MDI chip provides all the signals
 * necessary to support a 1152x900 pixel display at 76HZ running in full true
 * color mode. Support is also provided for 1280x1024 and 1600x1280 monitors
 * running at 76HZ. With sufficient VRAM (8MB), the MDI will support these
 * monitors in 32-bit true color mode.
 *
-------------------------------------------------------------------------
|	VRAM SIMM							|
|									|
|	---------					------		|
|	|	|					|    |		|
|	|	|					|    |		|
|	| VRAM	|					| C  |		|
|	|	|	---------			| O  |		|
|	|	|	|	|			| N  |		|
|	| Video | < --- |	|			| N  |		|
|	| Reload|	|  MDI  |	------		| E  |		|
|	| Logic |	|	| ----> |    | -RED---> | C  | -RED-->	|
|	|	| ----> | CLUTs | < --- | D  |		| T  |		|
|	|	|	| XLUT, |	| A  | -GREEN-> | O  | -GREEN->	|
|	|	|	|CURSOR |	| C  |		| R  |		|
|	|	|	---------  ---->|    | --BLUE-> |    | -BLUE-->	|
|	|	|	   |	   |   ------		|    |		|
|	---------	   |	   |			------		|
|			   |	   |    ------				|
|			   |	   -----|    |				|
|			   |		| P  |				|
|			   ------------>| C  | (Programmable		|
|					| G  |  Clock			|
|					|    |  Generator)		|
|					------				|
-------------------------------------------------------------------------
 */


#define	MDI_MAX		0x04	/* Maximum Number of MDI's/Campus-II platform */
#define	MDI_MAX_CLUTS	0x03	/* Max number of Color look up tables/MDI */
#define	MDI_PAGESIZE	0x1000	/* MDI hardware page size */
#define	MDI_MAPSIZE	16 * MDI_PAGESIZE /* 16 Pages in MDI address space */

#define	MDI_REGOFFSET	0	/* Offset of the MDI register set */
#define	MDI_CURSOFFSET	0x1000  /* Offset of the MDI cursor registers set */
#define	MDI_DACOFFSET	0x2000	/* Offset of the MDI DAC registers set */
#define	MDI_XLUTOFFSET	0x3000	/* Offset of the MDI X Look Up Table */
#define	MDI_CLUT1OFFSET	0x4000	/* Offset of the MDI Color Look Up Table 1 */
#define	MDI_CLUT2OFFSET	0x5000	/* Offset of the MDI Color Look Up Table 2 */
#define	MDI_CLUT3OFFSET	0x6000  /* Offset of the MDI Color Look Up Table 3 */
#define	MDI_AUTOOFFSET	0xf000  /* Offset of the MDI Color Look Up Table 3 */

/*
 * MDI Control/Status register set. Page 0 of the MDI address space.
 */

struct mdi_register_address {

	union {
		/* VSIMM 1 */
		struct {
			uchar_t	intr_ena:1;
			uchar_t	vid_ena:1;
			uchar_t	pixmode:2;
			uchar_t	tmr:2;
			uchar_t	tm_ena:1;
			uchar_t	reset:1;
		} mcr1;

		/* VSIMM 2 */
		struct {
			uchar_t	intr_ena:1;
			uchar_t	blank:1;
			uchar_t	pixmode:2;
			uchar_t	tmr:2;
			uchar_t	tm_ena:1;
			uchar_t	vid_ena:1;
		} mcr2;
	} m_mcr;		/* Master control register	*/

	uchar_t	m_ppr;		/* Packed pixel register	*/
	uchar_t	m_tmsr0;	/* Test mode status register 0  */
	uchar_t	m_tmsr1;	/* Test mode status register 1  */
	uchar_t	m_msr;		/* Master status register	*/
	uchar_t	m_fsr;		/* Fault status register	*/

	struct {
		uchar_t	revision:4;	/* MDI revision */
		uchar_t	impl:4;		/* MDI implementation */
	} m_rsr;

	uchar_t	m_ccr;		/* Clock control register	*/
	uint_t 	m_tmr;		/* Test mode Read Back register */
	uchar_t	m_mod;		/* Monitor Operation data reg	*/
	uchar_t	m_acr;		/* Aux control reg		*/
	uchar_t	m_pad0[6];	/* Reserved			*/
	ushort_t m_hct;		/* Horizontal Counter		*/
	ushort_t m_vct;		/* Vertical Counter		*/
	ushort_t m_hbs;		/* Horizontal Blank Start	*/
	ushort_t m_hbc;		/* Horizontal Blank Clear	*/
	ushort_t m_hss;		/* Horizontal Sync Set		*/
	ushort_t m_hsc;		/* Horizontal Sync Set		*/
	ushort_t m_csc;		/* Composite sync clear		*/
	ushort_t m_vbs;		/* Vertical blank start		*/
	ushort_t m_vbc;		/* Vertical Blank Clear	*/
	ushort_t m_vss;		/* Verical Sync Set		*/
	ushort_t m_vsc;		/* Verical Sync Clear		*/
	ushort_t m_xcs;		/* XXX Gone in VSIMM 2 */
	ushort_t m_xcc;		/* XXX Gone in VSIMM 2 */
	ushort_t m_fsa;		/* Fault status address		*/
	ushort_t m_adr;		/* Address register (autoincrements) */
	uchar_t	m_pad2[0xce];	/* Reserved			*/

	/* PCG registers */
	uchar_t	m_pcg[0x100];	/* Pixel Clock generator regs	*/

	/* VBC registers */
	struct {
		uint_t	res0:8;
		uint_t	framebase:12;	/* frame base row addr */
		uint_t	res1:12;
	} v_vbr;

	union {
		/* VSIMM 1 only (also called RCR) */
		struct {
			uint_t	res0:21;
			uint_t	vconfig:2;	/* vconfig */
			uint_t	r_setup:9;	/* reload setup time */
		} v_mcr1;

		/* VSIMM 2 only */
		struct {
			uint_t	res0:28;
			uint_t	fbconfig:2;	/* fb config */
			uint_t	trc:1;		/* test row cntr */
			uint_t	refresh:1;	/* refresh enable */
		} v_mcr2;
	} v_mcr;

	union {
		struct {
			uint_t	res0:21;
			uint_t	ref_ena:1;	/* refresh enable */
			uint_t	ref_req:10;	/* refresh req interval */
		} v_vcr1;

		struct {
			uint_t	res0:21;
			uint_t	ref_req:10;	/* refresh req interval */
		} v_vcr2;
	} v_vcr;

	struct {
		uint_t	res0:18;
		uint_t	hires:1;	/* 1 = 8MB */
		uint_t	ramspeed:1;	/* 1 = 60ns */
		uint_t	version:2;	/* version */
		uint_t	cad:10;		/* current row address */
	} v_vca;

	uchar_t	m_pad3[0xf0];	/* Reserved 			*/

};

/* Flags for the MDI_MCR_PIXMASK bits */
#define	MDI_MCR_8PIX	0x00		/* MDI interprets incoming data as */
					/*  16 8-bit pixels		    */
#define	MDI_MCR_16PIX	0x02		/* MDI interprets incoming data as */
					/*  8 16-bit pixels		    */
#define	MDI_MCR_32PIX	0x03		/* MDI interprets incoming data as */
					/*  4 32-bit pixels		    */
/*
 * Test mode readback pixel selection. Selects the active pipeline for
 * diagnostics.
 */
#define	MDI_MCR_PIXPIPE0	0x0
#define	MDI_MCR_PIXPIPE1	0x04
#define	MDI_MCR_PIXPIPE2	0x08
#define	MDI_MCR_PIXPIPE3	0x0c
#define	MDI_MCS_PIXSHIFT	0x02	/* To set pixel pipe selection bits */


/*
 * Flags for the packed pixel register.
 */
#define	MDI_PPR_GREY	0x00
#define	MDI_PPR_LUT1	0x01
#define	MDI_PPR_LUT2	0x02
#define	MDI_PPR_LUT3	0x03		/* Reserved */

#define	MDI_PPR_LEFT		0xc0 /* Mask for blend select left */
#define	MDI_PPR_RIGHT		0x30 /* Mask for blend select right */
#define	MDI_PPR_LEFTSHIFT	0x06 /* For setting values of left channel  */
#define	MDI_PPR_RIGHTSHIFT	0x04 /* For setting values of right channel */

/*
 *  Flags for Test Mode status regs
 */
#define	MDI_TMS0_MUX_STATE	0x08
#define	MDI_TMS0_IVS		0x07

#define	MDI_TMS1_CET		0xF0
#define	MDI_TMS1_CST		0x0F

/*
 * Flags for the master status register.
 * XXX No information available as yet on monitor sensing .
 */
#define	MDI_MSR_INTPEND		0x20	/* Interrupt pending		   */
#define	MDI_MSR_VINT		0x10	/* Vertical interrupt pending	   */
#define	MDI_MSR_FAULT		0x01	/* Fault detected. 		*/

/*
 * Flags for the fault status register.
 */
#define	MDI_FSR_WERR	0x01	/* Attempt write at read only address */
#define	MDI_FSR_UNIMP	0x04	/* Unimplemented address access fault */

/*
 * Flags for the revision status register.
 */
#define	MDI_RSR_REVMASK		0xf0	/* MDI Revision code	*/
#define	MDI_RSR_IMPLMASK	0x0f	/* MDI Implementation code */

/*
 * MDI implementation codes.  Codes 0x4 - 0xF are reserved for future releases.
 */

#define	DOUBLE_2CLUTS	0x0
#define	DOUBLE_3CLUTS	0x1
#define	SINGLE_2CLUTS	0x2
#define	SINGLE_3CLUTS	0x3

/*
 * Flags for VSIMM #1 Clock control register
 */
#define	MDI_CCR_PCGCLK_ENABLE	0x04
#define	MDI_CCR_CLKSEL1_ENABLE	0x02
#define	MDI_CCR_CLKSEL0_ENABLE	0x01

/*
 *  Flags for VSIMM #2 Clock Control Register
 */
#define	MDI_CCR_PCGSCLK		0x1
#define	MDI_CCR_PCGSDAT		0x2
#define	MDI_CCR_PCGSDAT_DIRSEL	0x4
#define	MDI_CCR_PCGASXSEL	0x8
#define	MDI_CCR_PCGAXS		0x10
#define	MDI_CCR_DATABITS	0xf0
#define	MDI_CCR_DATASHIFT	0x4
#define	MDI_ICS_NREGS		16

/*
 * Flags for the test mode read back register
 */
#define	MDI_TST_BLUE	0xff0000 /* 8-bit blue pixel value in pipeline */
#define	MDI_TST_GREEN	0x00ff00 /* 8-bit green pixel value in pipeline */
#define	MDI_TST_RED	0x0000ff /* 8-bit red pixel value in pipeline */

/*
 *  Flags for the VBC registers
 */
/* Flags for VBR register */
#define	VBC_VBR_FRAMEBASE	0x00FFF000	/* base register mask */
#define	VBC_VBR_FBSHIFT		12		/* bits to shift FRAMEBASE */

/* Flags for MCR register */
#define	VBC_MCR_VCONFIG	0x0000000C	/* VCONFIG field */
#define	VBC_MCR_TRC	0x00000002	/* Test row counter (diags only) */
#define	VBC_MCR_REN	0x00000001	/* Refresh enable */

/* Flag for VCR register */
#define	VBC_VCR_RRI	0x000003FF	/* Refresh request interval mask */

/* Flags for VCA register */
#define	VBC_VCA_HIRES	0x00002000	/* 1 => 8 MB VRAM, 0 => 4 MB    */
#define	VBC_VCA_VRAM	0x00001000	/* 1 => 60ns VRAMs, 0 => 80ns */
#define	VBC_VCA_VERS	0x00000C00	/* Verson number of VBC */
#define	VBC_VCA_VERSHIFT	10	/* bits to shift for VERSION */
#define	VBC_VCA_CAD	0x000003FF	/* Current row address to xfer */

/*
 *  Other control register flags
 */
#define	MDI_HCT		0x3ff	/*  Mask for 10 bit horizontal counter value */
#define	MDI_VCT		0xfff	/*  Mask for 12 bit vertical counter value */
#define	MDI_HBS		0x3ff	/*  Mask for 10 bit horzontal blank start */
#define	MDI_HBC		0x3ff	/*  Mask for 10 bit horzontal blank clear */
#define	MDI_HSS		0x3ff	/*  Mask for 10 bit horizontal sync set   */
#define	MDI_HSC		0x3ff	/*  Mask for 10 bit horizontal sync clear */
#define	MDI_CSC		0x3ff	/*  Mask for 12 bit composite sync clear  */
#define	MDI_VBS		0xfff	/*  Mask for 12 bit vertical blank start */
#define	MDI_VBC		0xfff	/*  Mask for 12 bit vertical blank clear */
#define	MDI_VSS		0xfff	/*  Mask for 12 bit vertical sync set   */
#define	MDI_VSC		0xfff	/*  Mask for 12 bit vertical sync clear */
#define	MDI_XCS		0x3ff	/*  Mask for 10 bit transfer cycle set   */
#define	MDI_XCC		0x3ff	/*  Mask for 10 bit transfer cycle clear */

/*
 * MDI Hardware Cursor Map
 */

#define	MDI_CURS_SIZE		32
#define	MDI_CURS_ENTRIES	2

struct mdi_cursor_address {

	ulong_t curs_cpl0[MDI_CURS_SIZE]; /* Enable plane 0 space */
	ulong_t curs_cpl1[MDI_CURS_SIZE]; /* Color selection plane */
	uchar_t curs_ccr;		 /* Cursor control register */
	uchar_t curs_pad0[3];		 /* Reserved */
	ushort_t curs_xcu;		 /* X-cursor location	*/
	ushort_t curs_ycu;		 /* Y-cursor location	*/
	uint_t curs_cc1;		 /* Cursor color register 1 */
	uint_t curs_cc2;		 /* Cursor color register 2 */
	uint_t curs_pad1[0x1bc];	 /* Reserved		*/
	ulong_t curs_cpl0i[MDI_CURS_SIZE]; /* Enable plane 0 autoinc space */
	ulong_t curs_cpl1i[MDI_CURS_SIZE]; /* Color select'n plane autoinc sp */
};

/*
 * Flags for cursor control
 */

#define	MDI_CURS_ENABLE 	0x04
#define	MDI_CURS_SELECT		0x02		/* Hardware/Full Screen */

/*
 * Double buffered full screen cursor select. When the cursor is enabled
 * and the full screen cursor is selected, the user may select which pair
 * of the four high order bits from the X-information to use for the full
 * screen cursor.
 *
 *	0 ==> Use bits (7:6), where 7=Enable, 6=Color
 *	1 ==> Use bits (5:4), where 5=Enable, 6=Color
 */

#define	MDI_CURS_DBUF		0x01

/*
 * Masks for x, y cursor location registers.
 */

#define	MDI_CURS_XMASK	0xfff	/* Bits 0-11 of X cursor loc register */
#define	MDI_CURS_YMASK	0xfff	/* Bits 0-11 of Y cursor loc register */

/*
 * Masks for cursor color register
 */

#define	MDI_CURS_ALPHAMASK	0xf8000000	/* Bits 31 - 27 of color reg */
#define	MDI_CURS_PIXVAL		0x00ffffff	/* Bits 0 - 23 of color reg */

/*
 * Hardware X Look Up Table
 */
#define	MDI_CMAP_ENTRIES 256		/* Number of color map entries */
#define	MDI_CMAP_MASK	255

struct mdi_xlut_address {

	uchar_t	x_xlut[MDI_CMAP_ENTRIES];
	uchar_t	x_xlutd[MDI_CMAP_ENTRIES];
	uchar_t	x_pad0[0x600];		/*	Reserved	*/
	uchar_t	x_xlut_inc[MDI_CMAP_ENTRIES];
	uchar_t	x_xlutd_inc[MDI_CMAP_ENTRIES];
};

/*
 * Hardware Color Look Up table. The MDI can support upto 3 Color look up tables
 * of which only two are implemented in revision 0 MDI silicon.  There is one
 * color look up table per MDI page (in the MDI address space). Each color
 * look up table has MDI_CMAP_ENTRIES (i.e 256) entries and each entry is 4
 * bytes (sizeof (int)) i.e each table is 1K in size. Each entry contains the
 * alpha, red, blue and green values.
 */


struct mdi_clut_address {
	uint_t	c_clut[MDI_CMAP_ENTRIES];
	uint_t	c_clutd[MDI_CMAP_ENTRIES];
	uint_t	c_clut_inc[MDI_CMAP_ENTRIES];
	uint_t	c_clutd_inc[MDI_CMAP_ENTRIES];
};

/*
 *  The DAC uses an autoincrement mode.  Successive writes to (gluth, glutl)
 *  will result in writes to red, green, and blue, respectively.
 */
struct mdi_dac_address {
	uchar_t	dac_addr_reg;		/* DAC address register */
	uchar_t	dac_pad0[0xff];		/* Reserved		*/
	uchar_t	dac_glut;		/* the gamma table */
	uchar_t	dac_pad1[0xff];		/* Reserved		*/
	uchar_t	dac_reg_select;		/* DAC register select */
	uchar_t	dac_pad2[0xff];		/* Reserved		*/
	uchar_t	dac_mode_reg;		/* DAC mode register */
};

/*
 *  DAC address register locations
 */
#define	DAC_PIXEL_TEST_REG	0x00
#define	DAC_TEST_REG		0x01
#define	DAC_SYNC_TEST_REG	0x02
#define	DAC_ID_REG		0x03	/* Will read as 0x8c */
#define	DAC_PIXEL_MASK_REG	0x04
#define	DAC_COMMAND_REG_2	0x06
#define	DAC_COMMAND_REG_3	0x07

/*
 * Flags for update_flag
 */

#define	UPDATE_XLUT	0x1	/* Update X Look Up Table */
#define	UPDATE_CLUT1	0x2	/* Update Color Look Up Table1 */
#define	UPDATE_CLUT2	0x4	/* Update Color Look Up Table2 */
#define	UPDATE_CLUT3	0x8	/* Update Color Look Up Table3 */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CG14REG_H */
