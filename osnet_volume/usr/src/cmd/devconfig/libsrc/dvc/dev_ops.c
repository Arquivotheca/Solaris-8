/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)dev_ops.c 1.3 94/07/13 SMI"

#include <stdio.h>

#include "dvc.h"
#include "conf.h"
#include "dev.h"

/*
 * Category processing structure for devices.  Each category
 * has an internal/external prototype processing routine, and an
 * internal/external device processing routine.  The prototype routines
 * are called once when the prototype is read in and when configuration
 * data is to be written out. The per device routines are called each
 * time a device is added or written out.
 *
 * An internal prototype processing routine may return a new set of
 * prototypes instead of the original one.  In this case the original
 * prototype is replaced with the new set.
 */
typedef struct devops {
	char *cat;					/* device category */
	conf_list_t *(*typi)(attr_list_t *dev);		/* proto in proc */
	void (*typo)(attr_list_t *dev);			/* proto out proc */
	void (*devi)(device_info_t *dev); 		/* device in proc */
	device_info_t *(*devo)(device_info_t *dev);	/* device out proc */
} devops_t;

extern conf_list_t *cvt_kb_typ_in(attr_list_t* typ);
extern void cvt_kb_typ_out(attr_list_t* typ);
extern void cvt_kb_dev_in(device_info_t *dp);
extern device_info_t *cvt_kb_dev_out(device_info_t *dp);

extern conf_list_t *cvt_ds_typ_in(attr_list_t* typ);
extern void cvt_ds_dev_in(device_info_t *dp);
extern device_info_t *cvt_ds_dev_out(device_info_t *dp);

extern conf_list_t *cvt_mn_typ_in(attr_list_t* typ);

extern device_info_t *cvt_ms_dev_out(device_info_t *dp);

static devops_t devopstab[] = {
	"keyboard", cvt_kb_typ_in, cvt_kb_typ_out, cvt_kb_dev_in,cvt_kb_dev_out,
	"display",  cvt_ds_typ_in, NULL,           cvt_ds_dev_in,cvt_ds_dev_out,
	"pointer",  NULL,          NULL,           NULL,         cvt_ms_dev_out,
	"monitor",  cvt_mn_typ_in, NULL,	   NULL,	 NULL,
	NULL,       NULL,          NULL,           NULL,         NULL
};

void
cvt_dev_in(device_info_t *dp)
{
	char *cat;
	devops_t *ops;

	if ((cat = find_attr_str(dp->typ_alist, CAT_ATTR)) == NULL)
		return;

	for (ops = devopstab; ops->cat; ops++)
		if (ops->devi && streq(cat, ops->cat)) {
			(*ops->devi)(dp);
			break;
		}
}

device_info_t *
cvt_dev_out(device_info_t *dp)
{
	char *cat;
	devops_t *ops;

	if ((cat = find_attr_str(dp->typ_alist, CAT_ATTR)) == NULL)
		return (NULL);

	for (ops = devopstab; ops->cat; ops++)
		if (ops->devo && streq(cat, ops->cat))
			return ((*ops->devo)(dp));

	return (NULL);
}

conf_list_t *
cvt_typ_in(attr_list_t *typ)
{
	char *cat;
	devops_t *ops;

	if ((cat = find_attr_str(typ, CAT_ATTR)) == NULL)
		return (NULL);

	for (ops = devopstab; ops->cat; ops++)
		if (ops->typi && streq(cat, ops->cat))
			return ((*ops->typi)(typ));

	return (NULL);
}

void
cvt_typ_out(attr_list_t *typ)
{
	char *cat;
	devops_t *ops;

	if ((cat = find_attr_str(typ, CAT_ATTR)) == NULL)
		return;

	for (ops = devopstab; ops->cat; ops++)
		if (ops->typo && streq(cat, ops->cat)) {
			(*ops->typo)(typ);
			break;
		}
}
