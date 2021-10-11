/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CG14_IO_H
#define	_SYS_CG14_IO_H

#pragma ident	"@(#)cg14io.h	1.18	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MDI_NAME	"cgfourteen"
#define	MIOC		('M'<<8)

/*
 * ioctl(2) commands, mmap(2) cookies and data definitions for the MDI
 * (Memory Display Interface) chip. (The MDI chip is part of the VRAM SIMM).
 */

/*
 *  In the commands to follow, the data type expected (if any) will be given
 *  as a comment following the command.
 */
#define	MDI_RESET		(MIOC|1)
#define	MDI_GET_CFGINFO		(MIOC|2) /* struct mdi_cfginfo * */

/*
 * Data type for MDI_GET_CFGINFO command
 */

struct mdi_cfginfo {

	int	mdi_ncluts;	/* Number of Color Look Up Tables/MDI */
	int	mdi_type;	/* Frame buffer type */
	int	mdi_height;	/* in pixels */
	int	mdi_width;	/* in pixels */
	int	mdi_size;	/* Total size in bytes	*/
	int	mdi_mode;	/* Current mode: 8-bit or 16-bit or 32-bit */
	int	mdi_pixfreq;	/* Current pixel clock, as given by prom */
};


/*
 * MDI pixel mode. The MDI can be programmed to interpret incoming data from
 * the Video Buffer Chip to be one of a) 16 8-bit pixels, b) 8 16-bit pixels
 * or c) 4 32-bit pixels. The input data to the MDI from the VBC is 128 bits
 * wide. The pixel mode is selected by programming the MCR
 * using the MDI_SET_PIXELMODE ioctl(2) command.
 */

#define	MDI_SET_PIXELMODE	(MIOC|3) /* int */

#define	MDI_8_PIX	0x08	/* Interpret incoming data as 16 8-bit pixels */
#define	MDI_16_PIX	0x10	/* Interpret incoming data as 8 16-bit pixels */
#define	MDI_32_PIX	0x20	/* Interpret incoming data as 4 32-bit pixels */

/*
 * command to set the horizontal and vertical counters.
 */
#define	MDI_SET_COUNTERS	(MIOC|4) /* struct mdi_set_counters * */

struct mdi_set_counters {

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
	ushort_t m_xcs;		/* Transfer cycle clear		*/
	ushort_t m_xcc;		/* Transfer cycle clear		*/
};

/*
 * Blend Selection. In the 8-bit pixel mode each pixel from the MDI input
 * mux is directed through each of direct (greyscale), color look up
 * tables 1, 2 (and 3 if implemented) and  hardware cursor table resulting
 * in 4 inputs (5 if CLUT3 is implemented) to the blend mux. Only two of the
 * input paths are blended together. The alpha value comes from the path on
 * the right. The ioctl MDI_SET_PPR allows selecting the lookup tables
 * in the 8-bit mode. In the 16 and 32-bit modes the X data provides the
 * blend selection criterion. Greyscale or direct input is always on the
 * left path.
 */

#define	MDI_SET_PPR	(MIOC|5) /* int */

/*
 *  Possible values to stick into PPR (8-bit mode only)
 */
#define	MDI_BLEND_LEFT_DIRECT	0x00
#define	MDI_BLEND_LEFT_CLUT1	0x40
#define	MDI_BLEND_LEFT_CLUT2	0x80
#define	MDI_BLEND_LEFT_CLUT3	0xc0

#define	MDI_BLEND_RIGHT_DIRECT	0x00
#define	MDI_BLEND_RIGHT_CLUT1	0x10
#define	MDI_BLEND_RIGHT_CLUT2	0x20
#define	MDI_BLEND_RIGHT_CLUT3	0x30

/*
 *  Possible values to stick into X channel (16/32-bit mode only).
 */
#define	MDI_SOURCE_LEFT_TRUE	0x00
#define	MDI_SOURCE_LEFT_B	0x04
#define	MDI_SOURCE_LEFT_G	0x08
#define	MDI_SOURCE_LEFT_R	0x0c

#define	MDI_SOURCE_RIGHT_X	0x00
#define	MDI_SOURCE_RIGHT_B	0x01
#define	MDI_SOURCE_RIGHT_G	0x02
#define	MDI_SOURCE_RIGHT_R	0x03

/*
 * Enable/Disable vertical retrace interrupts
 */
#define	MDI_VRT_CNTL		(MIOC|6) /* int */

/*
 * Possible values to MDI_VRT_CNTL command...
 */
#define	MDI_ENABLE_VIRQ		0x01 /* Enable vertical retrace interrupts */
#define	MDI_DISABLE_VIRQ	0x02 /* Disable vertical retrace interrupts */

/*
 * Color look up tables update command.
 */

#define	MDI_SET_CLUT	(MIOC|7) /* struct mdi_set_clut * */
#define	MDI_GET_CLUT	(MIOC|8) /* struct mdi_set_clut * */

struct	mdi_set_clut {
	int lut;		/* Color look up table number */
	int index;		/* Starting index into the table */
	int count;		/* Count of entries to update */
	uchar_t *alpha;		/* Alpha values of the entry	*/
	uchar_t *red;		/* Red component of an entry	*/
	uchar_t *green;		/* Green component of an entry */
	uchar_t *blue;		/* Blue component of an entry	*/
};

/*
 * X and Color look up table definitions.
 */

#define	MDI_XLUT	0x01
#define	MDI_CLUT1	0x02
#define	MDI_CLUT2	0x04
#define	MDI_CLUT3	0x08	/* Not supported in revision 0 */
#define	MDI_GAMMALUT	0x10
#define	MDI_CLUT_ALL	0x17

/*
 * Commands to update the X look up table. Each entry of the X look up table
 * is 8 bits wide of which the low order 4 bits are used for left channel
 * blend table and source component selection and the high order 4 bits
 * are used for the right channel blend table and source selection.
 */

#define	MDI_SET_XLUT	(MIOC|9) /* struct mdi_set_xlut * */
#define	MDI_GET_XLUT	(MIOC|10) /* struct mdi_set_xlut * */

struct	mdi_set_xlut {

	int index;	 /* Starting index into the table */
	int count;	 /* Count of entries to update */
	uchar_t *xbuf;	 /* Component value of an entry	*/
	uchar_t *maskbuf; /* Array of mask of bits to preserved. */
			/* Mask is typically different for each entry. */
	int mask;	 /* Mask of bits to preserve if mask_buf is NULL */
};

#define	MDI_SET_RESOLUTION	(MIOC|19) /* struct mdi_set_resolution * */

typedef struct whf {
	uint_t width;
	uint_t height;
	uint_t vfreq;
} whf_t;


struct mdi_set_resolution {
	int	pixelfreq;	/* Pixel clock freq in hz */
	int	hfreq;		/* Horizontal Frequency in hz */
	int	hfporch;	/* Horizontal Front Porch in pels */
	int	hsync;		/* Horizontal Sync Width in pels */
	int	hbporch;	/* Horizontal Back Porch in pels */
	int	hvistime;	/* Horizontal Visible Time in pels */
	int	hblanking;	/* Horizontal Blanking time in pels */
	int	vfreq;		/* Vertical Frequency in hz */
	int	vfporch;	/* Vertical Front Porch in lines */
	int	vsync;		/* Vertical Sync in lines */
	int	vbporch;	/* Vertical Back Porch in lines */
	int	vvistime;	/* Vertical Visible Time in lines */
	int	vblanking;	/* Vertical Blanking in lines */
};


typedef	struct mon_spec {
	whf_t	ms_whf;
	struct	mdi_set_resolution	ms_msr;
} mon_spec_t;


/*
 *  This ioctl will set a global flag which tells the driver
 *  whether subsequent accesses to the color maps should be
 *  degamma-corrected or not.  But regardless of its setting,
 *  in all cases applications will read the same data with MDI_GET_CLUT
 *  as they wrote with MDI_SET_CLUT.
 *  Setting gamma correction "off" implies setting de-gamma
 *  correction "on".
 *  These take effect immediately.  That is, the current
 *  contents of the cluts will be "de-gamma corrected"
 *  and inserted back into the same clut.  This is done
 *  for all cluts.
 *  The MDI_GAMMA_CORRECT arg must be set to one of the values below.
 */
#define	MDI_GAMMA_CORRECT	(MIOC|11)

#define	MDI_GAMMA_CORRECTION_OFF		0
#define	MDI_GAMMA_CORRECTION_ON			1

/*
 * Commands to update the DAC (gamma) color table. The DAC has 256 entries
 * and each entry is 30 bits wide: 10 bits each for the red, green and
 * blue components.  The MDI always outputs 8 bits per channel, but
 * the DAC may remap these into gamma space with its 10 bits.
 */

#define	MDI_SET_GAMMALUT	(MIOC|12) /* struct mdi_set_gammalut * */
#define	MDI_GET_GAMMALUT	(MIOC|13) /* struct mdi_set_gammalut * */

struct mdi_set_gammalut {

	int	index;		/* index into the DAC look up table */
	int	count;		/* count of entries to read/update */
	unsigned short	*red;	/* Gamma-corrected red table */
	unsigned short	*green;	/* Gamma-corrected green table */
	unsigned short	*blue;	/* Gamma-corrected blue table */
};

#define	MDI_SET_DEGAMMALUT	(MIOC|14) /* struct mdi_set_degamma_lut * */
#define	MDI_GET_DEGAMMALUT	(MIOC|15) /* struct mdi_set_degamma_lut * */

struct	mdi_set_degammalut {

	int index;	/* Index into the degamma table */
	int count;	/* Count of entries to read/update */
	uchar_t *degamma; /* Table of degamma entries */
};


#define	MDI_GET_BUFFER_INFO	(MIOC|16) /* struct mdi_get_buffer_info * */

struct mdi_buffer_info {
	int	b_num_bufs;	/* Number of possible buffers for this mon */
	int	b_cur_buf;	/* index of currently displayed buffer */
	off_t	b_buf_offset;	/* offset of displayed buf from fb_start */
};

#define	MDI_SET_CURSOR	(MIOC|17) /* struct mdi_set_cursor_info * */

struct mdi_cursor_info {
	uint_t	curs_enable0[32];	/* Cursor enable plane */
	uchar_t	curs_ctl;		/* Cursor control reg */
	ushort_t curs_xpos;		/* Cursor x position */
	ushort_t curs_ypos;		/* Cursor y position */
};

#define	MDI_GET_DIAGINFO	(MIOC|18) /* struct mdi_diaginfo * */

/*
 * Data type for MDI_GET_DIAGINFO command
 */
struct mdi_diaginfo {
	struct mdi_cfginfo mdi_cfg;
	int	mdi_mihdel;	/* Memory inhibit delay from SMC */
	int	mdi_gstate;	/* State flag for gamma/degamma */
};

#define	SET_MONITOR_POWER	(MIOC|20) /* turn monitor on/off */

/*
 * Mappings cookies to be used as the "offset" parameter of mmap(2) call.
 * vaddr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED, cookie, mdifd );
 *
 * These constants must be page aligned for mmap(2) to be successful.
 *
 */

#define	MDI_NMAPS	13	/* Number of objects to which mappings are */
				/* supported by MDI devices. This includes */
				/* one mapping for cgthree emulation	*/

#define	MDI_DIRECT_MAP		0x10000000  /* Pages 0-15 of the MDI address */
					    /*  space */
#define	MDI_CTLREG_MAP		0x20000000  /* Control/Status registers */
#define	MDI_CURSOR_MAP		0x30000000  /* Hardware cursor; MDI address */
					    /*  space */
#define	MDI_SHDW_VRT_MAP	0x40000000  /* Vertical Retrace Counter	*/

/*
 * Mappings to the frame buffer.
 */

#define	MDI_CHUNKY_XBGR_MAP	0x50000000	/* 32 bit chunky XBGR */
#define	MDI_CHUNKY_BGR_MAP	0x60000000	/* 32 bit chunky BGR */
#define	MDI_PLANAR_X16_MAP	0x70000000	/* 8+8 planar X channel */
#define	MDI_PLANAR_C16_MAP	0x80000000	/* 8+8 planar C channel */
#define	MDI_PLANAR_X32_MAP	0x90000000	/* 32 bit planar X */
#define	MDI_PLANAR_B32_MAP	0xa0000000	/* 32 bit planar Blue */
#define	MDI_PLANAR_G32_MAP	0xb0000000	/* 32 bit planar Green */
#define	MDI_PLANAR_R32_MAP	0xc0000000	/* 32 bit planar Red */

#define	MDI_DEFAULT_DEPTH	8		/* used by diags */
#define	MDI_DEFAULT_WIDTH	1152
#define	MDI_DEFAULT_HEIGHT	900

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CG14_IO_H */
