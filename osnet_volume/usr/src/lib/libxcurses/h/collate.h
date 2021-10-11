/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)collate.h 1.1	96/01/17 SMI"

/*
 * collate.h 
 *
 *   MKS extension to ANSI/POSIX to retrieve additional locale information.
 *   Specifically collation information.
 *
 * Copyright 1989, 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 * 
 * $Header: /rd/h/rcs/collate.h 1.16 1993/12/02 21:54:23 mark Exp $
 */
#ifndef	__COLLATE_H__
#define	__COLLATE_H__

#include <mks.h>
#include <limits.h>

/*
 * include prototypes of the MKS i18n routines
 */
#include <m_collat.h>

/*
 * constants and structure definitions used in MKS internal implementation
 *
 *  -- used by strxfrm(), localedef.
 */

/* string length limitations */
#define _M_COLLATE_MAX	32	/* max length of a collation string */
#define _M_COTOM_MAX	4	/* max length of one to many map */

/* forward, backward... -- or'ed into cmode */
#define _M_FORWARD_MODE	 1 	/* compare based on string forward */
#define _M_BACKWARD_MODE 2 	/* compare based on string backward */
#define _M_POSITION_MODE 4 	/* consider relative position of non-IGNOREd */

/* flags used in cord.cflag */
#define _M_NOCOLLSTR	0
#define _M_MANYTOONEF	1	/* Potential many-to-one mapping in forward */
#define _M_MANYTOONEB	2	/* Potential many-to-one mapping in backward */

/* typedef's */
typedef short	_m_weight_t;

/* collate database file header */
struct _m_cinfo {
	int		cnstr;			/* number of collate strings */
	int		cnotom;			/* number of one to many maps */
	int		cnweight;		/* Number of weights in cmode */
	unsigned char	cmode[COLL_WEIGHTS_MAX];/* order mode: forward... */
	struct _m_cstr	*cstr;			/* Pointer to collating strs */
	struct _m_cord	*cord;			/* weight tables */
	struct _m_cotom	*cotom;			/* one-to-many mapping tables */
	m_collel_t	*cindex,		/* index to range table map */
			*range;			/* Range table */
	m_collel_t	*equivo,		/* equiv class offsets */
			*cequiv;		/* equiv class table */
};

/* Per character and per collating-sequence structure */
struct _m_cord {				/* order */
	unsigned char	cflag;	/* =1, beginning of a collation string */
	_m_weight_t	cweight[COLL_WEIGHTS_MAX]; /* relative weight */
};

/*l
 * The _m_cstr structure is per many-to-one mapping (collating element from...)
 * Pointed at by _m_cinfo.cstr, indexed by (m_collel_t - M_CSETSIZE).
 * Stored both forwards and backwards to make it easy for strxfrm parsing
 * for order_start backward.
 */
struct _m_cstr {				/* collating-element */
	char		csf[_M_COLLATE_MAX];	/* collating string forward */
	char		csb[_M_COLLATE_MAX];	/* collating string backward */
};

/*l
 * Per one-to-many mapping entry.  A cweight entry in the _cord structure
 * which is negative, will index into this table.
 * Pointed at by _m_cinfo.cotom.
 */
struct _m_cotom {				/* one to many map */
	_m_weight_t	weight[_M_COTOM_MAX];
};

#ifdef I18N

extern struct _m_cinfo	_m_cinfo;

#endif	/* I18N */

#endif	/* __COLLATE_H_ */
