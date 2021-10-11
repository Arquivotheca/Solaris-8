/*
 * Copyright (c) by 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DDI_IMPLFUNCS_H
#define	_SYS_DDI_IMPLFUNCS_H

#pragma ident	"@(#)ddi_implfuncs.h	1.31	99/06/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * Declare implementation functions that sunddi functions can
 * call in order to perform their required task. Each kernel
 * architecture must provide them.
 */

int
i_ddi_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp);

int
i_ddi_apply_range(dev_info_t *dip, dev_info_t *rdip, struct regspec *rp);

struct regspec *
i_ddi_rnumber_to_regspec(dev_info_t *dip, int rnumber);


int
i_ddi_map_fault(dev_info_t *dip, dev_info_t *rdip,
	struct hat *hat, struct seg *seg, caddr_t addr,
	struct devpage *dp, uint_t pfn, uint_t prot, uint_t lock);

ddi_regspec_t
i_ddi_get_regspec(dev_info_t *dip, dev_info_t *rdip, uint_t rnumber,
	off_t offset, off_t len);

ddi_intrspec_t
i_ddi_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber);

int
i_ddi_add_intrspec(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind);

void
i_ddi_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie);

int
i_ddi_add_softintr(dev_info_t *dip, int preference, ddi_softintr_t *idp,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg);

void
i_ddi_trigger_softintr(ddi_softintr_t id);

void
i_ddi_remove_softintr(ddi_softintr_t id);

void
i_ddi_remove_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t iblock_cookie);

int
i_ddi_get_nintrs(dev_info_t *dip);

int
i_ddi_intr_hilevel(dev_info_t *dip, uint_t inumber);

int
i_ddi_add_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg);

int
i_ddi_add_fastintr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*hi_int_handler)(void));

int
i_ddi_dev_nintrs(dev_info_t *dev, int *result);

int
i_ddi_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t op,
    void *arg, void *val);

void
i_ddi_set_parent_private(dev_info_t *dip, caddr_t data);

caddr_t
i_ddi_get_parent_private(dev_info_t *dip);

/*
 * device tree configuration functions
 */

dev_info_t *
i_ddi_add_child(dev_info_t *, char *, uint_t, uint_t);

int
i_ddi_remove_child(dev_info_t *, int);

int
i_ddi_initchild(dev_info_t *, dev_info_t *);

void
i_ddi_set_binding_name(dev_info_t *dip, char *name);

major_t
i_ddi_bind_node_to_driver(dev_info_t *dip);

struct devnames;	/* some files doesn't include autoconf.h */
void
i_ddi_remove_from_dn_list(struct devnames *, dev_info_t **, dev_info_t *,
    uint_t, uint_t);

/*
 * Implementation specific memory allocation and de-allocation routines.
 */
int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attributes,
	size_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_hdl_t *handlep);

int
i_ddi_mem_alloc_lim(dev_info_t *dip, ddi_dma_lim_t *limits,
	uint_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	uint_t *real_length, ddi_acc_hdl_t *handlep);

void
i_ddi_mem_free(caddr_t kaddr, int streaming);

dev_info_t *
i_ddi_path_to_devi(char *pathname);

/*
 * Access and DMA handle fault set/clear routines
 */
void
i_ddi_acc_set_fault(ddi_acc_handle_t handle);
void
i_ddi_acc_clr_fault(ddi_acc_handle_t handle);
void
i_ddi_dma_set_fault(ddi_dma_handle_t handle);
void
i_ddi_dma_clr_fault(ddi_dma_handle_t handle);

/*
 *      Event-handling functions for rootnex
 *      These provide the standard implementation of fault handling
 */
void
i_ddi_rootnex_init_events(dev_info_t *dip);

int
i_ddi_rootnex_get_eventcookie(dev_info_t *dip, dev_info_t *rdip,
	char *eventname, ddi_eventcookie_t *cookiep, ddi_plevel_t *plevelp,
	ddi_iblock_cookie_t *iblock_cookiep);
int
i_ddi_rootnex_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid, int (*handler)(dev_info_t *dip,
	ddi_eventcookie_t event, void *arg, void *impl_data), void *arg);
int
i_ddi_rootnex_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid);
int
i_ddi_rootnex_post_event(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid, void *impl_data);

/*
 * Clustering: Return the global devices path base, or
 * the entire global devices path prefix.
 */
const char *
i_ddi_get_dpath_base();

const char *
i_ddi_get_dpath_prefix();

/*
 * Search and return properties from the PROM
 */
int
impl_ddi_bus_prop_op(dev_t dev, dev_info_t *dip,
	dev_info_t *ch_dip, ddi_prop_op_t prop_op, int mod_flags,
	char *name, caddr_t valuep, int *lengthp);

/*
 * Copy an integer from PROM to native machine representation
 */
int
impl_ddi_prop_int_from_prom(uchar_t *intp, int n);


extern int impl_ddi_sunbus_initchild(dev_info_t *);
extern void impl_ddi_sunbus_removechild(dev_info_t *);

extern int impl_ddi_sbus_initchild(dev_info_t *);

/*
 * Implementation specific access handle allocator and init. routines
 */
extern ddi_acc_handle_t impl_acc_hdl_alloc(int (*waitfp)(caddr_t),
	caddr_t arg);
extern void impl_acc_hdl_free(ddi_acc_handle_t handle);

extern ddi_acc_hdl_t *impl_acc_hdl_get(ddi_acc_handle_t handle);
extern void impl_acc_hdl_init(ddi_acc_hdl_t *hp);

/*
 * misc/bootdev entry points - these are private routines and subject
 * to change.
 */
extern int
i_devname_to_promname(char *dev_name, char *ret_buf);

extern int
i_promname_to_devname(char *prom_name, char *ret_buf);

extern int
i_ddi_peek(dev_info_t *, size_t, void *, void *);

extern int
i_ddi_poke(dev_info_t *, size_t, void *, void *);

/*
 * Nodeid management ...
 */
void impl_ddi_init_nodeid(void);
int impl_ddi_alloc_nodeid(int *nodeid);
int impl_ddi_take_nodeid(int nodeid, int kmflag);
void impl_ddi_free_nodeid(int nodeid);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDI_IMPLFUNCS_H */
