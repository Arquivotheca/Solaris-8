/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ESUNDDI_H
#define	_SYS_ESUNDDI_H

#pragma ident	"@(#)esunddi.h	1.16	99/07/08 SMI"	/* SVr4.0 */

#include <sys/sunddi.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * esunddi.h:		Function prototypes for kernel ddi functions.
 *	Note that drivers calling these functions are not
 *	portable.
 */

int
e_ddi_prop_create(dev_t dev, dev_info_t *dip, int flag,
	char *name, caddr_t value, int length);

int
e_ddi_prop_modify(dev_t dev, dev_info_t *dip, int flag,
	char *name, caddr_t value, int length);

int
e_ddi_prop_update_int(dev_t match_dev, dev_info_t *dip,
	char *name, int data);

int
e_ddi_prop_update_int_array(dev_t match_dev, dev_info_t *dip,
    char *name, int *data, uint_t nelements);

int
e_ddi_prop_update_string(dev_t match_dev, dev_info_t *dip,
	char *name, char *data);

int
e_ddi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, uint_t nelements);

int
e_ddi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, uchar_t *data, uint_t nelements);

int
e_ddi_prop_remove(dev_t dev, dev_info_t *dip, char *name);

void
e_ddi_prop_remove_all(dev_info_t *dip);

int
e_ddi_prop_undefine(dev_t dev, dev_info_t *dip, int flag, char *name);

int
e_ddi_getprop(dev_t dev, vtype_t type, char *name, int flags, int defaultval);

int64_t
e_ddi_getprop_int64(dev_t dev, vtype_t type, char *name,
	int flags, int64_t defvalue);

int
e_ddi_getproplen(dev_t dev, vtype_t type, char *name, int flags, int *lengthp);

int
e_ddi_getlongprop(dev_t dev, vtype_t type, char *name, int flags,
	caddr_t valuep, int *lengthp);

int
e_ddi_getlongprop_buf(dev_t dev, vtype_t type, char *name, int flags,
	caddr_t valuep, int *lengthp);

dev_info_t *
e_ddi_get_dev_info(dev_t dev, vtype_t type);

int
e_ddi_deferred_attach(major_t maj, dev_t dev);

int
e_ddi_parental_suspend_resume(dev_info_t *dip);

int
e_ddi_resume(dev_info_t *dip, ddi_attach_cmd_t);

int
e_ddi_suspend(dev_info_t *dip, ddi_detach_cmd_t cmd);

int
e_ddi_prepower(dev_info_t *dip, int pm_cmpt, int pm_newlevel, int pm_oldlevel);

int
e_ddi_postpower(dev_info_t *dip, int pm_cmpt, int pm_newlevel, int pm_oldlevel,
	int result);

void
pm_init(void);

void
e_ddi_enter_driver_list(struct devnames *dnp, int *listcnt);

int
e_ddi_tryenter_driver_list(struct devnames *dnp, int *listcnt);

void
e_ddi_exit_driver_list(struct devnames *dnp, int listcnt);

void
e_ddi_prop_list_delete(ddi_prop_t *proplist);

/*
 * return codes for devi_stillreferenced()
 *
 * DEVI_REFERENCED	- specfs has open minor device(s) for the devinfo
 * DEVI_NOT_REFERENCED	- specfs has no open minor device for the devinfo
 * DEVI_REF_UNKNOWN	- open state of the devinfo node is not known
 *			  by specfs
 */
#define	DEVI_REFERENCED		1
#define	DEVI_NOT_REFERENCED	0
#define	DEVI_REF_UNKNOWN	-1

int
devi_stillreferenced(dev_info_t *dip);

extern int (*pm_platform_power)(power_req_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ESUNDDI_H */
