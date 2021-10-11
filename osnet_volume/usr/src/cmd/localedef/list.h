/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)list.h 1.4	97/07/25  SMI"

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
/* @(#)$RCSfile: list.h,v $ $Revision: 1.3.2.2 $ */
/* (OSF) $Date: 1992/02/18 20:25:23 $ */
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
 * 1.1  com/cmd/nls/list.h, bos320 5/2/91 16:02:18
 */

#ifndef _H_LIST
#define	_H_LIST
typedef struct _LISTEL_T listel_t;

struct _LISTEL_T {
	void	*key;
	void	*data;

	listel_t *next;
};

typedef struct {
	int	(*comparator)(void *, void *);

	listel_t head;
} list_t;

list_t	*create_list(int (*comparator)(void *, void *));
int	add_list_element(list_t *, listel_t *);
listel_t	*loc_list_element(list_t *, void *);
listel_t	*create_list_element(void *key, void *data);

#endif
