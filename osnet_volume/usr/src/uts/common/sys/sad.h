/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SAD_H
#define	_SYS_SAD_H

#pragma ident	"@(#)sad.h	1.20	99/03/11 SMI"	/* SVr4.0 1.5 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Streams Administrative Driver
 */

/*
 * As time has passed, it has become necessary to add members to some
 * of the structures passed downstream with these ioctls.  Currently,
 * only the SAD_GAP/SAD_SAP ioctls are versioned, (which use the
 * strapush structure), but the versioning mechanism is general enough
 * to be applied to any SAD ioctls.  This is done by repartitioning
 * the SAD ioctl namespace to include a version number in addition to
 * the command (see below).
 *
 * In the case of the SAD_GAP/SAD_SAP ioctls, an application can
 * choose which "version" of the ioctl to use by #defining AP_VERSION
 * before including this file. Old code implicitly has AP_VERSION set
 * to 0, and even newly compiled code defaults to an AP_VERSION of 0,
 * since it may not be aware of the new structure members and
 * therefore not know to set them to reasonable values.  In order for
 * programs to make use of a newer version, they must explicitly
 * #define AP_VERSION to the appropriate value. Note that the kernel
 * always defaults to the latest version, since it is internally
 * self-consistent.
 */
#ifndef	AP_VERSION
#ifdef	_KERNEL
#define	AP_VERSION		1		/* latest version */
#else
#define	AP_VERSION		0		/* SVR4 version */
#endif
#endif

/*
 * ioctl defines
 *
 * The layout for the low 16 bits is 01000101VVVVCCCC, where the
 * first bitpattern is `D' in binary, followed by a 4 bit version
 * field (limiting the number of versions to 16), followed by a
 * 4 bit command field (limiting the number of commands to 16).
 */
#define	SADIOC		('D' << 8)
#define	SAD_SAP		(SADIOC|AP_VERSION << 4|01)
#define	SAD_GAP		(SADIOC|AP_VERSION << 4|02)
#define	SAD_VML		(SADIOC|03)		/* validate module list */

/*
 * Device naming and numbering conventions.
 */
#define	USERDEV		"/dev/sad/user"
#define	ADMINDEV	"/dev/sad/admin"

#define	USRMIN		0
#define	ADMMIN		1

/*
 * The maximum modules you can push on a stream using the autopush
 * feature.  This should be less than NSTRPUSH.
 */
#define	MAXAPUSH	8

/*
 * autopush info common to user and kernel
 */
struct apcommon {
	uint_t	apc_cmd;		/* command (see below) */
	major_t	apc_major;		/* major # of device */
	minor_t	apc_minor;		/* minor # of device */
	minor_t	apc_lastminor;		/* last minor for range */
	uint_t	apc_npush;		/* number of modules to push */
};

/*
 * New autopush information structure.  This wouldn't be necessary
 * except `struct apcommon' wasn't defined last in the `strapush'
 * structure, making it difficult to grow the structure without
 * breaking binary compatibility.  Note that new members can be added
 * to this structure in the future, at which point AP_VERSION should
 * be incremented (of course, a new STRAPUSH_Vx_LEN macro should be
 * added and sad.c should be changed to handle the new member).
 */
struct apdata {
	uint_t		apd_anchor;	/* position of anchor in stream */
};

/*
 * ap_cmd: various flavors of autopush
 */
#define	SAP_CLEAR	0		/* remove configuration list */
#define	SAP_ONE		1		/* configure one minor device */
#define	SAP_RANGE	2		/* configure range of minor devices */
#define	SAP_ALL		3		/* configure all minor devices */

/*
 * format for autopush ioctls
 */
struct strapush {
	struct apcommon	sap_common;			  /* see above */
	char		sap_list[MAXAPUSH][FMNAMESZ + 1]; /* module list */
#if AP_VERSION > 0
	struct apdata	sap_data;			  /* see above */
#endif
};

#define	sap_cmd		sap_common.apc_cmd
#define	sap_major	sap_common.apc_major
#define	sap_minor	sap_common.apc_minor
#define	sap_lastminor	sap_common.apc_lastminor
#define	sap_npush	sap_common.apc_npush
#define	sap_anchor	sap_data.apd_anchor

#ifdef _KERNEL

/*
 * state values for ioctls
 */
#define	GETSTRUCT	1
#define	GETRESULT	2
#define	GETLIST		3

#define	SAD_VER(ioccmd)	(((ioccmd) >> 4) & 0x0f)
#define	SAD_CMD(ioccmd)	((ioccmd) & ~0xf0)

#define	STRAPUSH_V0_LEN	(size_t)(&((struct strapush *)0)->sap_data)
#define	STRAPUSH_V1_LEN	(size_t)(STRAPUSH_V0_LEN + sizeof (uint_t))

struct saddev {
	queue_t	*sa_qp;		/* pointer to read queue */
	caddr_t	 sa_addr;	/* saved address for copyout */
	int	 sa_flags;	/* see below */
};

/*
 * values for saddev flags field.
 */
#define	SADPRIV		0x01

/*
 * Module Autopush Cache
 */
struct autopush {
	struct autopush	*ap_nextp;		/* next on list */
	int		 ap_flags;		/* see below */
	struct apcommon  ap_common;		/* see above */
	ushort_t	 ap_list[MAXAPUSH];	/* list of modules to push */
						/* (indices into fmodsw) */
	int		 ap_cnt;		/* in use count */
	struct apdata	 ap_data;		/* see above */
};

/*
 * The command issued by the user ultimately becomes
 * the type of the autopush entry.  Therefore, occurrences of
 * "type" in the code refer to an existing autopush entry.
 * Occurrences of "cmd" in the code refer to the command the
 * user is currently trying to complete.  types and cmds take
 * on the same values.
 */
#define	ap_type		ap_common.apc_cmd
#define	ap_major	ap_common.apc_major
#define	ap_minor	ap_common.apc_minor
#define	ap_lastminor	ap_common.apc_lastminor
#define	ap_npush	ap_common.apc_npush
#define	ap_anchor	ap_data.apd_anchor

/*
 * autopush flag values
 */
#define	APFREE	0x00	/* free */
#define	APUSED	0x01	/* used */
#define	APHASH	0x02	/* on hash list */

/*
 * hash function for cache
 */
#define	strphash(maj)	strpcache[(((int)maj)&strpmask)]

extern struct saddev	*saddev;	/* sad device array */
extern int		sadcnt;		/* number of elements in saddev */
extern struct autopush	*autopush;	/* autopush data array */
extern int		nautopush;	/* number of elements in autopush */
extern struct autopush **strpcache;	/* autopush hash list */
extern int		strpmask;	/* used in hash function */

/*
 * function prototypes
 */
#ifdef C2_AUDIT
void audit_stropen(struct vnode *, dev_t *, int, cred_t *);
void audit_strclose(struct vnode *, int, cred_t *);
void audit_strioctl(struct vnode *, int, intptr_t, int, int, cred_t *, int *);
struct strbuf;
void audit_strputmsg(struct vnode *, struct strbuf *, struct strbuf *,
						unsigned char, int, int);
void audit_fdsend(int, struct file *, int);
void audit_fdrecv(int, struct file *);
#endif

extern void ap_free(struct autopush *);
extern void sad_initspace(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SAD_H */
