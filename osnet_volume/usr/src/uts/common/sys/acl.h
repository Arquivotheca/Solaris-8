/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#ifndef _SYS_ACL_H
#define	_SYS_ACL_H

#pragma ident	"@(#)acl.h	1.12	96/11/04 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAX_ACL_ENTRIES		(1024)	/* max entries of each type */
typedef struct acl {
	int		a_type;		/* the type of ACL entry */
	uid_t		a_id;		/* the entry in -uid or gid */
	o_mode_t	a_perm;		/* the permission field */
} aclent_t;

#define	USER_OBJ	(0x01)		/* object owner */
#define	USER		(0x02)		/* additional users */
#define	GROUP_OBJ	(0x04)		/* owning group of the object */
#define	GROUP		(0x08)		/* additional groups */
#define	CLASS_OBJ	(0x10)		/* file group class and mask entry */
#define	OTHER_OBJ	(0x20)		/* other entry for the object */
#define	ACL_DEFAULT	(0x1000)	/* default flag */
/* default object owner */
#define	DEF_USER_OBJ	(ACL_DEFAULT | USER_OBJ)
/* defalut additional users */
#define	DEF_USER	(ACL_DEFAULT | USER)
/* default owning group */
#define	DEF_GROUP_OBJ	(ACL_DEFAULT | GROUP_OBJ)
/* default additional groups */
#define	DEF_GROUP	(ACL_DEFAULT | GROUP)
/* default mask entry */
#define	DEF_CLASS_OBJ	(ACL_DEFAULT | CLASS_OBJ)
/* default other entry */
#define	DEF_OTHER_OBJ	(ACL_DEFAULT | OTHER_OBJ)

/* cmd arg to acl(2) */
#define	GETACL			1
#define	SETACL			2
#define	GETACLCNT		3

/* minimal acl entries from GETACLCNT */
#define	MIN_ACL_ENTRIES		4

#if !defined(_KERNEL)

/* acl check errors */
#define	GRP_ERROR		1
#define	USER_ERROR		2
#define	OTHER_ERROR		3
#define	CLASS_ERROR		4
#define	DUPLICATE_ERROR		5
#define	MISS_ERROR		6
#define	MEM_ERROR		7
#define	ENTRY_ERROR		8

/*
 * similar to ufs_acl.h: changed to char type for user commands (tar, cpio)
 * Attribute types
 */
#define	UFSD_FREE	('0')	/* Free entry */
#define	UFSD_ACL	('1')	/* Access Control Lists */
#define	UFSD_DFACL	('2')	/* reserved for future use */

extern int aclcheck(aclent_t *, int, int *);
extern int acltomode(aclent_t *, int, mode_t *);
extern int aclfrommode(aclent_t *, int, mode_t *);
extern int aclsort(int, int, aclent_t *);
extern char *acltotext(aclent_t *, int);
extern aclent_t *aclfromtext(char *, int *);

#else	/* !defined(_KERNEL) */

extern void ksort(caddr_t, int, int, int (*)(aclent_t *, aclent_t *));
extern int cmp2acls(aclent_t *, aclent_t *);

#endif	/* !defined(_KERNEL) */

#if defined(__STDC__)
extern int acl(const char *path, int cmd, int cnt, aclent_t *buf);
extern int facl(int fd, int cmd, int cnt, aclent_t *buf);
#else	/* !__STDC__ */
extern int acl();
extern int facl();
#endif	/* defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ACL_H */
