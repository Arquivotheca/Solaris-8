/*
 * Copyright (c) 1996, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)locdef.h 1.9	98/04/18  SMI"

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
/* @(#)$RCSfile: locdef.h,v $ $Revision: 1.4.5.2 $ */
/* (OSF) $Date: 1992/08/10 14:44:37 $ */

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
 * 1.4  com/cmd/nls/locdef.h, , bos320, 9135320l 8/12/91 17:09:41
 *
 */

/* To make available the definition of _LP64 and _ILP32 */
#include <sys/types.h>

extern char	*yyfilenm;
extern int	warn;

extern int	copying_ctype;		/* "" */

/*
 * Ignore and UNDEFINED.
 */
#define	IGNORE    (-1)
#define	UNDEFINED 0
#ifdef _LP64
#define	SUB_STRING (UINT_MAX-1)
#else
#define	SUB_STRING (ULONG_MAX-1)
#endif

#define	INT_METHOD(n)	(n)

#define	COLL_ERROR	0
#define	COLL_OK	1

/* init.c */
void init_symbol_tbl(int);

/* sem_chr.c */

char *copy(const char *);
char *copy_string(const char *);

/* gen.c */
#define	STRING_MAX	255

/* gram.y */
#define	MAX_CODESETS	3

/* sem_ctype.c, gram.y */
struct lcbind_symbol_table {
	char	*symbol_name;
	unsigned int    value;
};

struct lcbind_table {
	_LC_bind_table_t	lcbind;
	unsigned int 		nvalue;
	char			*orig_value;
	int			defined;
};
