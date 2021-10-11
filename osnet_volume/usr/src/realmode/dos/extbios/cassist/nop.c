/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 *
 *  No-op pseudo-driver:
 *
 *	This is a dummy realmode driver used primarily to test bootconf in
 *	a UNIX environment.
 *
 */

#ident "<@(#)nop.c	1.3	95/06/28 SMI>"
int(dispatch)() { return (0); }
