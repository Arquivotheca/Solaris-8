/*
 * Copyright (c) 1994-1995, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)func.h	1.9	98/06/30 SMI"

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
 * @(#)$RCSfile: func.h,v $ $Revision: 1.2.2.3 $ (OSF)
 * $Date: 1992/03/24 15:28:58 $
 */
/*
 * func.h - function type and argument declarations
 *
 * DESCRIPTION
 *
 *	This file contains function delcarations in both ANSI style
 *	(function prototypes) and traditional style.
 *
 * AUTHOR
 *
 *     Mark H. Colburn, NAPS International (mark@jhereg.mn.org)
 *
 * Sponsored by The USENIX Association for public distribution.
 *
 * Copyright (c) 1989 Mark H. Colburn.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Mark H. Colburn and sponsored by The USENIX Association.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _PAX_FUNC_H
#define	_PAX_FUNC_H

/* Headers */

#include "pax.h"

/* Function Prototypes */

extern void		append_archive(void);
extern int		ar_read(void);
extern void		buf_allocate(OFFSET);
extern int		buf_read(char *, uint_t);
extern int		buf_skip(OFFSET);
extern int		c_utf8(char *target, const char *source);
extern int		charmap_convert(char *);
extern void		close_archive(void);
extern void		create_archive(void);
extern int		dirmake(char *, Stat *);
extern int		dirneed(char *);
extern void		fatal(char *);
extern gid_t		findgid(char *);
extern char		*findgname(gid_t);
extern uid_t		finduid(char *);
extern char		*finduname(uid_t);
extern int		get_disposition(int, char *);
extern int		get_header(char *, Stat *);
extern int		get_newname(char *, int);
extern int		get_xdata(void);
extern struct group	*getgrgid();
extern struct group	*getgrnam();
extern struct passwd	*getpwuid();
extern int		hash_lookup(char *, struct timeval *);
extern void 		hash_name(char *, Stat *);
extern int		inentry(char *, char *, Stat *);
extern Link		*islink(char *, Stat *);
extern int		isyesno(const char *respMb, size_t testLen);
extern int		lineget(FILE *, char *);
extern Link		*linkfrom(char *, Stat *);
extern Link		*linkto(char *, Stat *);
extern char		*mem_get(uint_t);
extern char		*mem_rpl_name(char *);
extern char		*mem_str(char *);
extern void		name_gather(void);
extern int		name_match(char *, int);
extern int		name_next(char *, Stat *);
extern int		nameopt(char *);
extern void		names_notfound(void);
extern void		next(int);
extern int		nextask(char *, char *, int);
extern int		open_archive(int);
extern int		open_tty(void);
extern int		openin(char *, Stat *);
extern int		openout(char *, char *, Stat *, Link *, int);
extern void		outdata(int, char *, Stat *);
extern void		outwrite(char *, OFFSET);
extern void		pass(char *);
extern void		passdata(char *, int, char *, int);
extern void		print_entry(char *, Stat *);
extern void		read_archive(void);
extern int		read_header(char *, Stat *, int);
extern void		rpl_name(char *);
extern void		warn(char *, char *);
extern void		warnarch(char *, OFFSET);
extern void		write_eot(void);

#endif /* _PAX_FUNC_H */
