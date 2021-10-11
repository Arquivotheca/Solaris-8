/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_HWCONF_H
#define	_SYS_HWCONF_H

#pragma ident	"@(#)hwconf.h	1.11	98/10/11 SMI"

#include <sys/dditypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAX_HWC_LINESIZE 1024

typedef struct proto_dev_info {
	char		*proto_devi_name;
	ddi_prop_t	*proto_devi_sys_prop_ptr;
} proto_dev_info_t;

struct hwc_spec {
	struct hwc_spec *hwc_next;
	char		*hwc_parent_name;
	char		*hwc_class_name;
	proto_dev_info_t *hwc_proto;
};

/*
 * used to create sorted linked lists of hwc_spec structs for loading parents
 */
struct par_list {
	struct par_list	*par_next;
	struct hwc_spec	*par_specs;		/* List of prototype nodes */
	major_t		par_major;		/* Simple name of parent */
};

struct bind {
	struct bind 	*b_next;
	char		*b_name;
	char		*b_bind_name;
	int		b_num;
};

struct mperm {
	struct mperm	*mp_next;
	char		*mp_drvname;
	char		*mp_minorname;
	int		mp_perm;
	char		*mp_owner;
	char		*mp_group;
	uid_t		mp_uid;
	gid_t		mp_gid;
};

#ifdef _KERNEL

extern struct bind *mb_hashtab[];
extern struct bind *sb_hashtab[];

extern struct hwc_spec *hwc_parse(char *);
extern void hwc_free(struct hwc_spec *);
extern struct par_list *sort_hwc(struct hwc_spec *);
extern struct par_list *impl_make_parlist(major_t);

extern struct par_list *impl_read_driver_conf(major_t);
extern void impl_free_driver_conf(struct par_list *);

extern void impl_delete_par_list(struct par_list *);
extern struct par_list *impl_replicate_class_spec(struct par_list *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HWCONF_H */
