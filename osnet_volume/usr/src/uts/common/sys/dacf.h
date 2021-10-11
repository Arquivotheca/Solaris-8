/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DACF_H
#define	_DACF_H

#pragma ident	"@(#)dacf.h	1.1	99/01/26 SMI"

/*
 * Device autoconfiguration framework (dacf)
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define	DACF_MODREV_1		1	/* interface version # */

typedef void* dacf_arghdl_t;
typedef void* dacf_infohdl_t;

typedef enum {
	DACF_OPID_ERROR = -1,
	DACF_OPID_END = 0,		/* mark end of array of dacf_op's */
	DACF_OPID_POSTATTACH = 1,	/* operate after a driver attaches */
	DACF_OPID_PREDETACH = 2		/* operate before a driver detaches */
} dacf_opid_t;

#define	DACF_NUM_OPIDS 2

typedef struct dacf_op {
	dacf_opid_t op_id;		/* operation id */
	int (*op_func)(dacf_infohdl_t, dacf_arghdl_t, int);
} dacf_op_t;

typedef struct dacf_opset {
	char *opset_name;		/* name of this op-set */
	dacf_op_t *opset_ops;		/* null-terminated array of ops */
} dacf_opset_t;

struct dacfsw {
	int		dacf_rev;	/* dacf interface revision #	*/
	dacf_opset_t	*dacf_opsets;	/* op-sets in this module	*/
};

extern struct dacfsw kmod_dacfsw;	/* kernel provided module */

/*
 * DACF client interface
 */

const char *dacf_minor_name(dacf_infohdl_t);
minor_t dacf_minor_number(dacf_infohdl_t);
const char *dacf_driver_name(dacf_infohdl_t);
dev_info_t *dacf_devinfo_node(dacf_infohdl_t);
const char *dacf_get_arg(dacf_arghdl_t, char *);

void dacf_store_info(dacf_infohdl_t, void *);
void *dacf_retrieve_info(dacf_infohdl_t);

/*
 * Error codes for configuration operations
 */
#define	DACF_SUCCESS		0
#define	DACF_FAILURE		-1

#ifdef __cplusplus
}
#endif

#endif /* _DACF_H */
