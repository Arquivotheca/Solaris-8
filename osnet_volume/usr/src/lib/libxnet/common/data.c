/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)data.c	1.3	96/11/08 SMI"

/*
 * Define any exported data items.  These have to be described within an object
 * rather than a mapfile so that they are assigned to a valid section (and thus
 * do not result in absolute symbols). 
 *
 * Both of these symbols originate from libnsl.  h_error is an uninitialized
 * data item where as t_errno is initialized - the value provided here is
 * irrelevant but necessary to generate an appropriate copy relocation should
 * an application reference this symbol.
 */
int	h_errno;
int	t_errno = 0;
