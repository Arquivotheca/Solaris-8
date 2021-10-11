/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef _SYS_AUXIO_H
#define	_SYS_AUXIO_H

#pragma ident	"@(#)auxio.h	1.8	92/07/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions and structures for the Auxiliary Input/Output register
 * on sun4m machines.
 */
#define	AUXIO_REG	0xFEFF0000	/* virtual address of the register */

/* Bits of the auxio register -- input bits must always be written with ones */
#define	AUX_MBO		0xF0		/* Must be written with ones */
/* NOTE: whenever writing to the auxio register, or in AUX_MBO!!! */
#define	AUX_DENSITY	0x20		/* Floppy density (Input) */
					/* 1 = high, 0 = low */
#define	AUX_DISKCHG	0x10		/* Floppy diskette change (input) */
					/* 1 = new diskette inserted */
#define	AUX_DRVSELECT	0x08		/* Floppy drive select (output) */
					/* 1 = selected, 0 = deselected */
#define	AUX_TC		0x04		/* Floppy terminal count (output) */
					/* 1 = transfer over */
#define	AUX_EJECT	0x02		/* Floppy eject (output, NONinverted) */
					/* 0 = eject the diskette */
#define	AUX_LED		0x01		/* LED (output); 1 = on, 0 = off */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AUXIO_H */
