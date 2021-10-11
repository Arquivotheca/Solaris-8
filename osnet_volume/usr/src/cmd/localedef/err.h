/*
 * Copyright (c) 1996, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)err.h 1.6	98/11/19  SMI"

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
/*
 * @(#)$RCSfile: err.h,v $ $Revision: 1.4.2.4 $ (OSF)
 * $Date: 1992/02/18 20:24:55 $
 */
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
 * 1.2  com/cmd/nls/err.h, cmdnls, bos320, 9125320 6/1/91 14:41:50
 */

#include "localedef_msg.h"
#include <sys/types.h>

#define	MALLOC(t, n)   ((t *)safe_malloc(sizeof (t) * (n), __FILE__, __LINE__))
#define	STRDUP(s)	(safe_strdup(s, __FILE__, __LINE__))

extern void *safe_malloc(size_t, const char *, int);
extern void	*safe_strdup(const char *, const char *, int);

extern void error(int, ...);
extern void diag_error(int, ...);
extern void diag_error2(int, ...);
extern char *msgstr(int);
extern void usage(int);
extern void yyerror(const char *);
#define	INTERNAL_ERROR   error(ERR_INTERNAL, __FILE__, __LINE__)
