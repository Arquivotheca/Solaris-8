/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_OBJMGR_H
#define	_SYS_DKTP_OBJMGR_H

#pragma ident	"@(#)objmgr.h	1.4	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int	objmgr_load_obj(char *);
extern void	objmgr_unload_obj(char *);
extern opaque_t objmgr_create_obj(char *);
extern int	objmgr_destroy_obj(char *);
extern int	objmgr_ins_entry(char *, opaque_t, char *);
extern int	objmgr_del_entry(char *);

#define	OBJNAMELEN	64
struct obj_entry {
	struct obj_entry *o_forw;
	struct obj_entry *o_back;
	char		*o_keyp;
	opaque_t	(*o_cfunc)();
	int		o_refcnt;
	int		o_modid;
	char		*o_modgrp;
};

#define	OBJ_MODGRP_SNGL	NULL

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_OBJMGR_H */
