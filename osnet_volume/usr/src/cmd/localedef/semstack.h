/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)semstack.h 1.8	98/04/18  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
/* @(#)$RCSfile: semstack.h,v $ $Revision: 1.3.2.5 $ */
/* (OSF) $Date: 1992/03/05 17:55:14 $ */
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/cmd/nls/semstack.h, cmdnls, bos320 6/20/91 01:01:06
 */

#ifndef _SEMSTACK_H_
#define	_SEMSTACK_H_

#include "symtab.h"

typedef struct {
	wchar_t min;
	wchar_t max;
} range_t;


/* valid types for item_type */
typedef enum {
	SK_NONE,
	SK_INT,
	SK_STR,
	SK_RNG,
	SK_CHR,
	SK_SUBS,
	SK_SYM,
	SK_UNDEF
} item_type_t;

typedef struct {
	item_type_t	type;

	union {			/* type =  */
		int	int_no;		/*   SK_INT */
		char	*str;	/*   SK_STR, SK_UNDEF */
		range_t	*range;	/*   SK_RNG */
		chr_sym_t	*chr;	/*   SK_CHR */
		_LC_subs_t	*subs;	/*   SK_SUBS */
		symbol_t	*sym;	/*   SK_SYM */
	} value;

} item_t;


typedef struct _symbol_list {
	struct _symbol_list	*next;
	symbol_t	*symbol;
	int			order;
	symbol_t	*target;
} symbol_list;


/* semstack errors */
#define	SK_OK	0
#define	SK_OVERFLOW 1

int sem_push(item_t *);
item_t *sem_pop(void);
void destroy_item(item_t *);
item_t *create_item(item_type_t, ...);
#endif /* _SEMSTACK_H_ */
