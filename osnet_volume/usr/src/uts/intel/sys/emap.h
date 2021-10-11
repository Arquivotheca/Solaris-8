/*
 * Copyright (c) 1993,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_EMAP_H
#define	_SYS_EMAP_H

#pragma ident	"@(#)emap.h	1.12	99/05/04 SMI"

/*
 *	Copyright (C) The Santa Cruz Operation, 1986, 1987, 1988, 1989.
 *	This Module contains Proprietary Information of
 *	The Santa Cruz Operation and should be treated as Confidential.
 */

#include <sys/termios.h>

#ifdef	__cplusplus
extern "C" {
#endif

							/* BEGIN SCO_INTL */

/*
 * Emapping tables structures
 */

struct emind {
	unsigned char	e_key;
	unsigned char	e_ind;
};
typedef	struct emind	*emip_t;

struct emout {
	unsigned char	e_key;
	unsigned char	e_out;
};
typedef	struct emout	*emop_t;

struct emtab {
	unsigned char	e_imap[256];	/* 8-bit  input map */
	unsigned char	e_omap[256];	/* 8-bit output map */
	unsigned char	e_comp;		/* compose key */
	/*
	 * The following field used to be e_beep which indicated
	 * beep on error flag.
	 */
	unsigned char	e_toggle;	/* character to dis/enable mapping */
	short		e_cind;		/* offset of compose indexes */
	short		e_dctab;	/* offset of dead/compose table */
	short		e_sind;		/* offset of string indexes */
	short		e_stab;		/* offset of string table */
	struct emind	e_dind[1];	/* start of dead key indexes */
};
typedef	struct emtab	*emp_t;

/*
 * Emap control structure
 */
struct emap {
	emp_t	e_map;			/* pointer to emapping table */
	short	e_ndind;		/* number of dead indexes */
	short	e_ncind;		/* number of compose indexes */
	short	e_nsind;		/* number of string indexes */
};

#if _KERNEL

struct emap_state {
	char		es_state;	/* state during emapping only */
	char		es_saved_state;	/* old state before K_RAW mode */
	char		es_flags;	/* other states than es_state */
	char		es_error;	/* error during emapping only */
	unsigned char	es_saved;	/* saved emapping char */
	struct emap	*es_emap;	/* emapping control info */
};
typedef struct emap_state emap_state_t;

#define	EMAP_STATE(sp)		((sp)->es_state)
#define	EMAP_SAVED_STATE(sp)	((sp)->es_saved_state)
#define	EMAP_FLAGS(sp)		((sp)->es_flags)
#define	EMAP_ERROR(sp)		((sp)->es_error)
#define	EMAP_SAVED(sp)		((sp)->es_saved)
#define	EMAP_EMAP(sp)		((sp)->es_emap)

#define	EMAP_MAP(sp)		(EMAP_EMAP(sp)->e_map)
#define	EMAP_NDIND(sp)		(EMAP_EMAP(sp)->e_ndind)
#define	EMAP_NCIND(sp)		(EMAP_EMAP(sp)->e_ncind)
#define	EMAP_NSIND(sp)		(EMAP_EMAP(sp)->e_nsind)

/*
 * Emapping state (es_state)
 */
#define	ES_NULL		0x0		/* emapping not enabled */
#define	ES_OFF		0x1		/* emapping temporarily disabled */
#define	ES_ON		0x2		/* emapping enabled */
#define	ES_DEAD	   (0x4  | ES_ON)	/* dead key received */
#define	ES_COMP1   (0x8  | ES_ON)	/* compose key received */
#define	ES_COMP2   (0x10 | ES_ON)	/* compose + 1st key received */
#define	ES_DEC	   (0x20 | ES_ON)	/* compose key and 2 digits received */

/*
 * es_flags values
 */
#define	EF_NULL			0
#define	EF_GOT_LDSMAP_DATA	1
#define	EF_SCANCODE_RAW		2	/* stream is in the K_RAW scan mode */

#define	EMAPPSZ		INFPSZ	/* max packet size allowed on the emap stream */

#endif /* _KERNEL */

#define	EMBSHIFT	10	/* log2 BSIZE (EMAP_SIZE 1K) */
#define	EMBMASK		01777	/* EMAP_SIZE - 1 */

#define	ESTRUCTOFF(structure, field)	(int)&(((struct structure *)0)->field)
#define	E_DIND		(ESTRUCTOFF(emtab, e_dind[0]))
#define	EMAPED		'\0'		/* key maps to dead/compose/string */
#define	EMAP_SIZE	1024		/* size of an emtab */
							/* END SCO_INTL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EMAP_H */
