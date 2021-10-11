/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_stub.c	1.6	99/11/12 SMI"


/* Solaris ACPI stub */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/mutex.h>
#include <sys/modctl.h>
#include <sys/psm.h>

#include <sys/promif.h>

#include <sys/acpi.h>
#include <sys/acpi_prv.h>



/* this stub does not require the interpreter files to compile or link */


/* properties */
acpi_memory_t acpi_memory_prop;
unsigned int acpi_status_prop;
unsigned int acpi_debug_prop;
unsigned int acpi_options_prop;

static char *status_prop_name = "acpi-status";

kmutex_t acpi_mutex;
unsigned int acpi_ld_cnt;
int acpi_state;
acpi_ops_t acpi_ops_vector;
int init_msg_allowed = 1;		/* mention init only once */

/* memory */
caddr_t map_reclaim;
caddr_t map_nvs;
int acpi_tables_exist = 0;
int nvs_exists;

/* global table info */
struct ns_elem *root_ns;
int rsdt_entries;
struct acpi_header *rsdt_p;
struct acpi_facp *facp_p;
struct acpi_facs *facs_p;
struct acpi_apic *apic_p;
struct acpi_sbst *sbst_p;
struct ddb_desc *ddb_descs;

acpi_fatal_t fatal_info;

#define	acpi_dbg_msg(FAC) \
((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_DEBUG && \
acpi_debug_prop & (FAC))


/* address translation, return NULL on error */
void *
acpi_trans_addr(unsigned int addr)
{
	unsigned int offset;

	/* in reclaim range? */
	if (addr >= acpi_memory_prop.reclaim_paddr &&
	    addr < acpi_memory_prop.reclaim_paddr +
	    acpi_memory_prop.reclaim_len) {
		offset = addr - acpi_memory_prop.reclaim_paddr;
		if (acpi_dbg_msg(ACPI_DMEM))
			cmn_err(CE_NOTE,
			    "acpi_trans_addr: reclaim 0x%x to 0x%x\n",
			    addr, (unsigned int)(map_reclaim + offset));
		return ((void *)(map_reclaim + offset));
	}

	/* in nvs range? */
	if (nvs_exists == 0)
		return (NULL);

	if (addr >= acpi_memory_prop.nvs_paddr &&
	    addr < acpi_memory_prop.nvs_paddr +
	    acpi_memory_prop.nvs_len) {
		offset = addr - acpi_memory_prop.nvs_paddr;
		if (acpi_dbg_msg(ACPI_DMEM))
			cmn_err(CE_NOTE, "acpi_trans_addr: nvs 0x%x to 0x%x\n",
			    addr, (unsigned int)(map_nvs + offset));
		return ((void *)(map_nvs + offset));
	}
	if (acpi_dbg_msg(ACPI_DMEM))
		cmn_err(CE_NOTE, "acpi_trans_addr: TRANSLATION FAILED\n");
	return (NULL);
}

int
region_map(struct acpi_region *regionp)
{
	caddr_t map_base;

	if (regionp->space != ACPI_MEMORY) {
		regionp->mapping = regionp->offset;
		return (ACPI_OK);
	}
	map_base = psm_map(regionp->offset, regionp->length, PSM_PROT_WRITE);
	if (map_base == NULL)
		return (ACPI_EXC);
	regionp->mapping = (unsigned int)map_base;
	return (ACPI_OK);
}

/* get ACPI properties, these are all prom properties */
static int
acpi_props(void)
{
	dnode_t root_node, options_node;
	int ret = ACPI_OK;

	/* get properties */
	root_node = prom_rootnode();
	if (prom_bounded_getprop(root_node, "acpi-memory",
	    (caddr_t)&acpi_memory_prop, sizeof (acpi_memory_prop)) < 0)
		ret = ACPI_EXC;
	else
		acpi_tables_exist = 1;

	/* status really should exist, but if not assume zero */
	(void) prom_bounded_getprop(root_node, status_prop_name,
	    (caddr_t)&acpi_status_prop, sizeof (acpi_status_prop));

	/* if these two option props are not found, assume zero */
	options_node = prom_optionsnode();
	(void) prom_bounded_getprop(options_node, "acpi-debug",
	    (caddr_t)&acpi_debug_prop, sizeof (acpi_debug_prop));

	(void) prom_bounded_getprop(options_node, "acpi-user-options",
	    (caddr_t)&acpi_options_prop, sizeof (acpi_options_prop));
	if ((acpi_options_prop & ACPI_OUSER_MASK) == ACPI_OUSER_OFF)
		ret = ACPI_EXC;

	/* propagate disable from boot */
	if ((acpi_status_prop & ACPI_BOOT_ENABLE) == 0)
		ret = ACPI_EXC;

	/* do this after the debug variable is loaded */
	nvs_exists = (acpi_memory_prop.nvs_paddr != 0) ? 1 : 0;
	if (acpi_dbg_msg(ACPI_DMEM))
		cmn_err(CE_NOTE, "nvs exists: %d\n", nvs_exists);

	return (ret);
}

/* map the ACPI memory segments */
static int
acpi_segmap(void)
{
	caddr_t map_base;
	paddr_t phys_base;
	ulong_t phys_len;

	/* map each segment separately */
	phys_base = acpi_memory_prop.reclaim_paddr;
	phys_len = acpi_memory_prop.reclaim_len;
	map_base = psm_map(phys_base, phys_len, PSM_PROT_WRITE);
	if (map_base == NULL)
		return (ACPI_EXC);
	map_reclaim = map_base;

	if (nvs_exists) { /* do nvs, if it exists */
		phys_base = acpi_memory_prop.nvs_paddr;
		phys_len = acpi_memory_prop.nvs_len;
		map_base = psm_map(phys_base, phys_len,
		    PSM_PROT_WRITE);
		if (map_base == NULL)
			return (ACPI_EXC);
		map_nvs = map_base;
	}

	if (acpi_dbg_msg(ACPI_DMEM)) {
		cmn_err(CE_NOTE, "map_reclaim: 0x%x\n",
		    (unsigned int)map_reclaim);
		cmn_err(CE_NOTE, "map_nvs: 0x%x\n", (unsigned int)map_nvs);
	}
	return (ACPI_OK);
}

static int
acpi_modload(void)
{
	if (modload("misc", "acpi_intp") < 0) {
		cmn_err(CE_WARN, "ACPI modload failed");
		return (ACPI_EXC);
	}
	return (ACPI_OK);
}

static void
acpi_detect_msg_print(void)
{
	cmn_err(CE_CONT, "?ACPI detected: %d %x %x %x\n", acpi_state,
	    acpi_status_prop, acpi_options_prop, acpi_debug_prop);
}

static void
acpi_disable_common(void)
{
	static int disable_msg_allowed = 1; /* mention disable only once  */

	acpi_status_prop &= ~ACPI_OS_ENABLE;
	acpi_state = ACPI_DISABLED;

	if (disable_msg_allowed && acpi_tables_exist &&
	    (acpi_options_prop & ACPI_OUSER_MASK) != ACPI_OUSER_OFF) {
		acpi_detect_msg_print();
		disable_msg_allowed = 0;
	}
}


/* guard functions */
static int
acpi_enter(void)
{
	mutex_enter(&acpi_mutex);
	if (acpi_state != ACPI_INITED)
		return (ACPI_EXC);
	acpi_ld_cnt++;
	if (acpi_ld_cnt == 1 && acpi_modload() == ACPI_EXC) {
		acpi_ld_cnt = 0;
		return (ACPI_EXC);
	}
	return (ACPI_OK);
}

/* a little looser allowing entry on INIT1 state */
static int
acpi_tbl_enter(void)
{
	mutex_enter(&acpi_mutex);
	if (acpi_state != ACPI_INITED && acpi_state != ACPI_INIT1)
		return (ACPI_EXC);
	acpi_ld_cnt++;
	if (acpi_ld_cnt == 1 && acpi_modload() == ACPI_EXC) {
		acpi_ld_cnt = 0;
		return (ACPI_EXC);
	}
	return (ACPI_OK);
}

static void
acpi_exit(void)
{
	if (acpi_ld_cnt > 0)
		acpi_ld_cnt--;
	else
		cmn_err(CE_WARN, "ACPI lockdown count already zero");
	mutex_exit(&acpi_mutex);
}

/*
 * pre-initialization init
 * This is run very early when the system is not completely setup so
 * that pcplusmp can use it too.  Because we run so early, things
 * sometimes have to be done in a rather awkward way.
 *
 * this fn should only be called once!
 * ld_cnt is incremented to lockdown for pcplusmp
 * acpi_enter can't be used since we are not fully inited
 * as a result, an ld_cancel is necessary after pcplusmp
 */
void
acpi_init0(void)
{
	mutex_init(&acpi_mutex, NULL, MUTEX_ADAPTIVE, NULL);

	acpi_ld_cnt++;		/* needs a matching acpi_ld_cancel */
	if (acpi_props() == ACPI_EXC || acpi_modload() == ACPI_EXC) {
		acpi_disable_common();
		return;
	}

	acpi_state = ACPI_INIT0;
}


/*
 * half init to prevent building namespace
 * pcplusmp can use this to see if we need the rest
 */
int
acpi_init1(void)
{
	mutex_enter(&acpi_mutex);
	if (acpi_state != ACPI_INIT0 || acpi_ld_cnt == 0)
		goto init1_exc;

	if (acpi_segmap() == ACPI_EXC)
		goto init1_exc;
	if ((*acpi_ops_vector.fn_init)(ACPI_INIT1) == ACPI_EXC)
		goto init1_exc;
	acpi_state = ACPI_INIT1;
	mutex_exit(&acpi_mutex);
	if (init_msg_allowed) {
		acpi_detect_msg_print();
		init_msg_allowed = 0;
	}
	return (ACPI_OK);
init1_exc:
	acpi_disable_common();
	mutex_exit(&acpi_mutex);
	return (ACPI_EXC);
}

/* ACPI init, all clients call this (possibly more than once) */
int
acpi_init(void)
{
	mutex_enter(&acpi_mutex);
	switch (acpi_state) {
	case ACPI_INITED:
		goto init_ok;
	case ACPI_INIT0:
	case ACPI_INIT1:
		break;
	case ACPI_START:
		cmn_err(CE_WARN, "ACPI not pre-initialized");
	default:
		goto init_exc1;
	case ACPI_DISABLED:
		goto init_exc2;
	}

	if (acpi_ld_cnt == 0) {	/* in case it has been unloaded already */
		acpi_ld_cnt++;
		if (acpi_modload() == ACPI_EXC)
			goto init_exc1;
	}
	if (acpi_state == ACPI_INIT0 && acpi_segmap() == ACPI_EXC)
		goto init_exc1;
	if ((*acpi_ops_vector.fn_init)(ACPI_INITED) == ACPI_EXC)
		goto init_exc1;

	acpi_state = ACPI_INITED;

	acpi_status_prop |= (ACPI_OS_INIT | ACPI_OS_ENABLE);

init_ok:
	mutex_exit(&acpi_mutex);
	if (init_msg_allowed) {
		acpi_detect_msg_print();
		init_msg_allowed = 0;
	}
	return (ACPI_OK);
init_exc1:
	acpi_disable_common();
init_exc2:
	mutex_exit(&acpi_mutex);
	return (ACPI_EXC);
}

void
acpi_disable(void)
{
	mutex_enter(&acpi_mutex);
	acpi_disable_common();
	mutex_exit(&acpi_mutex);
}

acpi_fatal_t *
acpi_fatal_get(void)
{
	(void) acpi_enter();
	acpi_exit();
	return (&fatal_info);
}

/* table functions */
acpi_facs_t *
acpi_facs_get(void)
{
	acpi_facs_t *ret = NULL;

	if (acpi_tbl_enter() == ACPI_OK)
		ret = facs_p;
	acpi_exit();
	return (ret);
}

acpi_facp_t *
acpi_facp_get(void)
{
	acpi_facp_t *ret = NULL;

	if (acpi_tbl_enter() == ACPI_OK)
		ret = facp_p;
	acpi_exit();
	return (ret);
}

acpi_apic_t *
acpi_apic_get(void)
{
	acpi_apic_t *ret = NULL;

	if (acpi_tbl_enter() == ACPI_OK)
		ret = apic_p;
	acpi_exit();
	return (ret);
}

acpi_sbst_t *
acpi_sbst_get(void)
{
	acpi_sbst_t *ret = NULL;

	if (acpi_tbl_enter() == ACPI_OK)
		ret = sbst_p;
	acpi_exit();
	return (ret);
}


/* acpi_val_t fns */
acpi_val_t *
acpi_uninit_new(void)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_uninit_new)();
	acpi_exit();
	return (ret);
}

acpi_val_t *
acpi_integer_new(unsigned int value)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_integer_new)(value);
	acpi_exit();
	return (ret);
}

acpi_val_t *
acpi_string_new(char *string)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_string_new)(string);
	acpi_exit();
	return (ret);
}

acpi_val_t *
acpi_buffer_new(char *buffer, int length)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_buffer_new)(buffer, length);
	acpi_exit();
	return (ret);
}

acpi_val_t *
acpi_package_new(int size)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_package_new)(size);
	acpi_exit();
	return (ret);
}


acpi_val_t *
acpi_pkg_setn(acpi_val_t *pkg, int index, acpi_val_t *value)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_pkg_setn)(pkg, index, value);
	acpi_exit();
	return (ret);
}

void
acpi_val_free(acpi_val_t *valp)
{
	if (acpi_enter() == ACPI_OK)
		(*acpi_ops_vector.fn_val_free)(valp);
	acpi_exit();
}


/* object attributes */
acpi_nameseg_t
acpi_nameseg(acpi_obj obj)
{
	acpi_nameseg_t ret;

	ret.iseg = ACPI_ONES; /* really ACPI_EXC */
	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_nameseg)(obj);
	acpi_exit();
	return (ret);
}

unsigned short
acpi_objtype(acpi_obj obj)
{
	unsigned short ret = 0xFFFF; /* really ACPI_EXC */

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_objtype)(obj);
	acpi_exit();
	return (ret);
}


/* eval */
int
acpi_eval(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_eval)(obj, args, retpp);
	acpi_exit();
	return (ret);
}

int
acpi_eval_nameseg(acpi_obj obj, acpi_nameseg_t *segp, acpi_val_t *args,
    acpi_val_t **retpp)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_eval_nameseg)(obj, segp, args,
		    retpp);
	acpi_exit();
	return (ret);
}


/* navigation */
acpi_obj
acpi_rootobj(void)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_rootobj)();
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_nextobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_nextobj)(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_childobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_childobj)(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_parentobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_parentobj)(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_nextdev(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_nextdev)(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_childdev(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_childdev)(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_findobj(acpi_obj obj, char *name, int flags)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_findobj)(obj, name, flags);
	acpi_exit();
	return (ret);
}


#if 0
/* global lock */
int
acpi_gl_acquire(void)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_gl_acquire)();
	acpi_exit();
	return (ret);
}


int
acpi_gl_release(void)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_gl_release)();
	acpi_exit();
	return (ret);
}
#endif


/* callbacks */
acpi_cbid_t
acpi_cb_register(acpi_obj obj, acpi_cbfn_t fn, void *cookie)
{
	acpi_cbid_t ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_cb_register)(obj, fn, cookie);
	acpi_exit();
	return (ret);
}

int
acpi_cb_cancel(acpi_cbid_t id)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = (*acpi_ops_vector.fn_cb_cancel)(id);
	acpi_exit();
	return (ret);
}


/* lockdown */
int
acpi_ld_register(void)
{
	int ret = acpi_enter();

	mutex_exit(&acpi_mutex);
	return (ret);
}

void
acpi_ld_cancel(void)
{
	mutex_enter(&acpi_mutex);
	acpi_exit();
}

void
acpi_client_status(unsigned int client, int status)
{
	unsigned int mask;

	mutex_enter(&acpi_mutex);
	mask = client & ACPI_OS_CMASK;
	if (status == ACPI_CLIENT_ON)
		acpi_status_prop |= mask;
	else if (status == ACPI_CLIENT_OFF)
		acpi_status_prop &= ~mask;
	mutex_exit(&acpi_mutex);
}
