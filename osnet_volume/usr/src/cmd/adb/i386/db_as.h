/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.  All Rights Reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "@(#)db_as.h	1.1     92/06/18 SMI"

/*
 * Address space codes for the kernel debugger.
 */

#ifndef _DB_AS_H
#define _DB_AS_H

#define AS_KVIRT	1	/* Kernel virtual */
#define AS_PHYS		2	/* Physical */
#define AS_IO		3	/* I/O ports */
#define AS_UVIRT	4	/* User process virtual */

typedef unsigned int	db_as_t;
typedef caddr_t as_addr_t;

paddr_t db_uvtop(), db_kvtop();

#endif /* !_DB_AS_H */
