
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Downloadable code definitions
 */

#ifndef	_P_ROM
#define	_P_ROM

#pragma ident	"@(#)rom.h	1.1	94/12/02 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * The PLUTO controller has 4 proms (0-3).  Prom 1-3 are writeable and are
 * soldered to the board while prom 0 is not writeable but socketed. The
 * following items are placed in the PLUTO prom set:
 *	- POST0		-Power On Self Test code.  This code goes in
 *			 pluto_prom0 and may not be modified in the field.
 *			 It contains serial port downloading code.
 *	- FUNC		-Pluto Functional code (SPARC)
 *	- SOC		-SOC microcode
 *	- ISP		-ISP microcode
 *	- OBP		-Open Boot Prom code
 *	- Date Code	-date/time of prom creation.
 *	- WWN		- World Wide Name
 *
 *
 * This utility creates the writeable prom images for PLUTO.  Three prom images
 * are created: pluto_prom1, pluto_prom2, pluto_prom3.
 *
 * The following defines the layout of the 4 proms on the PLUTO controller:
 *
 * prom		offset		image
 * -----------------------------------
 * prom_0:
 *		0		POST
 * prom_1:
 *		0		FUNC
 * prom_2:
 *		0		FUNC cont'd
 * prom_3:
 *		PROM_MAGIC_OFF  PROM_MAGIC
 *		DATE_OFF	DATE_CODE
 *		WWN_OFF		WWN
 *		SOC_OFF 	SOC
 *		ISP_OFF		ISP
 *		OBP_OFF		OBP
 */
#define	PROM_MAGIC	0x7b7b7b7b
#define	PROMSIZE	0x00040000	/* 256K bytes each prom */

#define	FUNC_PROM	1
#define	CHECKSUM_PROM	3
#define	PROM_MAGIC_PROM	3
#define	WWN_PROM	3
#define	SOC_PROM	3
#define	ISP_PROM	3
#define	OBP_PROM	3
#define	DATE_PROM	3

struct PLUTO_DATE {
	long	real_time;
	char	date_str[26];	/* Note: terminated with /0a/00 */
};

#define	WWN_SIZE	8
#define	SOC_SIZE	0x8000	 /* 32K */
#define	ISP_SIZE	0x8000	 /* 32K */
#define	OBP_SIZE	0x8000	 /* 32K */

/* offsets in prom1 */
#define	FUNC_OFF	0

/* offsets in prom3 */
#define	PROM_MAGIC_OFF	0
#define	CHECKSUM_OFF	4
#define	WWN_OFF		8
#define	DATE_OFF	(WWN_OFF + WWN_SIZE)
#define	SOC_OFF		(PROMSIZE - ISP_SIZE - SOC_SIZE - OBP_SIZE)
#define	ISP_OFF		(PROMSIZE - ISP_SIZE - OBP_SIZE)
#define	OBP_OFF		(PROMSIZE - OBP_SIZE)


#ifdef	__cplusplus
}
#endif

#endif	/* _P_ROM */
