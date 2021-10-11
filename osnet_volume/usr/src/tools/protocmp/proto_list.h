/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proto_list.h	1.1	99/01/11 SMI"

#define	SYM_F		1
#define	PERM_F		2
#define	REF_F		4
#define	TYPE_F		8
#define	NAME_F		16
#define	OWNER_F		32
#define	GROUP_F		64
#define	MAJMIN_F	128

#define	CODE	0
#define	NAME	1
#define	SYM	2
#define	SRC	2
#define	PERM	3
#define	OWNR	4
#define	GRP	5
#define	INO	6
#define	LCNT	7
#define	MAJOR	8
#define	MINOR	9
#define	PROTOS	10
#define	FIELDS	11

extern int read_in_protolist(const char *, elem_list *, int);

extern int check_sym;
extern int check_link;
extern int check_user;
extern int check_group;
extern int check_perm;
extern int check_majmin;
