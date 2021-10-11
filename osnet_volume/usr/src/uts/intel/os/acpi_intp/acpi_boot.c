/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_boot.c	1.1	99/05/21 SMI"


/* boot interface to ACPI interpreter */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <strings.h>
#ifdef MEM_PROFILE
#include <stdlib.h>
#endif
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_tab.h"
#include "acpi_inf.h"


#undef	ACPI_EXTRA_DEBUG
#ifdef ACPI_EXTRA_DEBUG
unsigned int acpi_debug_init = 0x8903;
#endif

/* memory prop dealt with in probe.c */
unsigned int acpi_status_prop;
unsigned int acpi_debug_prop;
unsigned int acpi_options_prop;

int acpi_mutex;			/* boot is single threaded */
unsigned int acpi_ld_cnt;
int acpi_state;

/* global table info */
struct ns_elem *root_ns;
int rsdt_entries;
struct acpi_header *rsdt_p;
struct acpi_facp *facp_p;
struct acpi_facs *facs_p;
struct acpi_apic *apic_p;
struct acpi_sbst *sbst_p;
struct ddb_desc *ddb_descs;


int spec_thread;
struct acpi_thread main_thread, special_thread;

acpi_fatal_t fatal_info;

/* in case address translation is needed, return NULL on error */
void *
acpi_trans_addr(unsigned int addr)
{
	return ((void *)addr);
}

int
region_map(struct acpi_region *regionp)
{
	regionp->mapping = regionp->offset;
	return (ACPI_OK);
}

static void
mutex_cleanup(struct acpi_thread *threadp)
{
	acpi_val_t *ptr;
	acpi_mutex_t *mutexp;

	if (threadp->mutex_list == NULL)
		return;
	for (ptr = threadp->mutex_list; ptr; ) {
		mutexp = ptr->acpi_valp;
		ptr = mutexp->next;
		mutexp->owner = NULL;
		mutexp->next = NULL;
	}
	threadp->mutex_list = NULL;
	threadp->sync = 0;
}

/*ARGSUSED*/
struct acpi_thread *
acpi_thread_get(int need_excl)
{
	if (current_acpi_thread)
		return (NULL);
	bzero(&main_thread, sizeof (main_thread));
	current_acpi_thread = &main_thread;
	exc_clear();		/* clear previous errors */
	return (&main_thread);
}

/*ARGSUSED*/
void
acpi_thread_release(struct acpi_thread *threadp)
{
	main_thread.ddp = NULL;
	main_thread.edp = NULL;
	mutex_cleanup(&main_thread);
	current_acpi_thread = NULL;
}

struct acpi_thread *
acpi_special_thread_get(void)
{
	if (spec_thread)
		return (NULL);
	bzero(&special_thread, sizeof (special_thread));
	spec_thread++;
	return (&special_thread);
}

void
acpi_special_thread_release(void)
{
	mutex_cleanup(&special_thread);
	spec_thread = 0;
}

/* no acpi_ld_cnt stuff done in boot version of acpi_enter/exit */
static int
acpi_enter(void)
{
	if (acpi_mutex || acpi_state != ACPI_INITED)
		return (ACPI_EXC);
	acpi_mutex = 1;
	return (ACPI_OK);
}

static void
acpi_exit(void)
{
	acpi_mutex = 0;
}

/* ACPI init */
int
acpi_init(void)
{
	if (acpi_mutex)
		return (ACPI_EXC);
	acpi_mutex = 1;
	if (acpi_state == ACPI_INITED)
		goto init_ok;
	if (acpi_state == ACPI_DISABLED)
		goto init_exc;

#ifdef ACPI_EXTRA_DEBUG
	acpi_debug_prop = acpi_debug_init;
#endif

	if ((acpi_options_prop & ACPI_OUSER_MASK) == ACPI_OUSER_OFF)
		goto init_exc;
	if (acpi_i_init(ACPI_INITED) == ACPI_EXC)
		goto init_exc;

	acpi_state = ACPI_INITED;
	acpi_status_prop |= (ACPI_BOOT_INIT | ACPI_BOOT_ENABLE);
init_ok:
	acpi_exit();
	return (ACPI_OK);
init_exc:
	acpi_disable();
	acpi_exit();
	return (ACPI_EXC);
}

void
acpi_disable(void)
{
	/* do not put a mutex here! */
	acpi_status_prop &= ~ACPI_BOOT_ENABLE;
	acpi_state = ACPI_DISABLED;
}

/*LINTLIBRARY*/
acpi_fatal_t *
acpi_fatal_get(void)
{
	(void) acpi_enter();
	acpi_exit();
	return (&fatal_info);
}

/* table functions */
/*LINTLIBRARY*/
acpi_facs_t *
acpi_facs_get(void)
{
	acpi_facs_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = facs_p;
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_facp_t *
acpi_facp_get(void)
{
	acpi_facp_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = facp_p;
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_apic_t *
acpi_apic_get(void)
{
	acpi_apic_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = apic_p;
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_sbst_t *
acpi_sbst_get(void)
{
	acpi_sbst_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = sbst_p;
	acpi_exit();
	return (ret);
}


/* acpi_val_t fns */
/*LINTLIBRARY*/
acpi_val_t *
acpi_uninit_new(void)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = uninit_new();
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_val_t *
acpi_integer_new(unsigned int value)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = integer_new(value);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_val_t *
acpi_string_new(char *string)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = string_new(string);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_val_t *
acpi_buffer_new(char *buffer, int length)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = buffer_new(buffer, length);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_val_t *
acpi_package_new(int size)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = package_new(size);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_val_t *
acpi_pkg_setn(acpi_val_t *pkg, int index, acpi_val_t *value)
{
	acpi_val_t *ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = package_setn(pkg, index, value);
	acpi_exit();
	return (ret);
}

void
acpi_val_free(acpi_val_t *valp)
{
	if (acpi_enter() == ACPI_OK)
		value_free(valp);
	acpi_exit();
}

/* object attributes */
acpi_nameseg_t
acpi_nameseg(acpi_obj obj)
{
	acpi_nameseg_t ret;

	ret.iseg = ACPI_ONES;	/* really ACPI_EXC */
	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_nameseg(obj);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
unsigned short
acpi_objtype(acpi_obj obj)
{
	unsigned short ret = 0xFFFF; /* really ACPI_EXC */

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_objtype(obj);
	acpi_exit();
	return (ret);
}


/* eval */
int
acpi_eval(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_eval(obj, args, retpp);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
int
acpi_eval_nameseg(acpi_obj obj, acpi_nameseg_t *segp, acpi_val_t *args,
    acpi_val_t **retpp)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_eval_nameseg(obj, segp, args, retpp);
	acpi_exit();
	return (ret);
}


/* navigation */
acpi_obj
acpi_rootobj(void)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_rootobj();
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_obj
acpi_nextobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_nextobj(obj);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_obj
acpi_childobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_childobj(obj);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
acpi_obj
acpi_parentobj(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_parentobj(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_nextdev(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_nextdev(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_childdev(acpi_obj obj)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_childdev(obj);
	acpi_exit();
	return (ret);
}

acpi_obj
acpi_findobj(acpi_obj obj, char *name, int flags)
{
	acpi_obj ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_findobj(obj, name, flags);
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
		ret = acpi_i_gl_acquire();
	acpi_exit();
	return (ret);
}


int
acpi_gl_release(void)
{
	int ret = ACPI_EXC;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_gl_release();
	acpi_exit();
	return (ret);
}
#endif


/* callbacks */
/*LINTLIBRARY*/
acpi_cbid_t
acpi_cb_register(acpi_obj obj, acpi_cbfn_t fn, void *cookie)
{
	acpi_cbid_t ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_cb_register(obj, fn, cookie);
	acpi_exit();
	return (ret);
}

/*LINTLIBRARY*/
int
acpi_cb_cancel(acpi_cbid_t id)
{
	int ret = NULL;

	if (acpi_enter() == ACPI_OK)
		ret = acpi_i_cb_cancel(id);
	acpi_exit();
	return (ret);
}


/*
 * lockdown does not actually control unloading in boot.bin
 * but keep track of requests to flag incorrect behavior
 */
int
acpi_ld_register(void)
{
	if (acpi_enter() == ACPI_EXC)
		return (ACPI_EXC);
	acpi_ld_cnt++;
	acpi_exit();
	return (ACPI_OK);
}

void
acpi_ld_cancel(void)
{
	if (acpi_enter() == ACPI_EXC)
		return;
	if (acpi_ld_cnt > 0)
		acpi_ld_cnt--;
	else
		(void) exc_warn("ACPI lockdown count already zero");
	acpi_exit();
}


void
acpi_client_status(unsigned int client, int status)
{
	unsigned int mask;

	if (acpi_mutex)
		return;
	mask = client & ACPI_BOOT_CMASK;
	if (status == ACPI_CLIENT_ON)
		acpi_status_prop |= mask;
	else if (status == ACPI_CLIENT_OFF)
		acpi_status_prop &= ~mask;
	acpi_exit();
}


#if defined(ACPI_USER) && defined(MEM_PROFILE)
int heap_max;
int heap_size;

void *
my_malloc(size_t size)
{
	heap_size += size;
	if (heap_size > heap_max)
		heap_max = heap_size;
	return (malloc(size));
}

void
my_free(void *ptr, size_t size)
{
	heap_size -= size;
	free(ptr);
}
#endif


/* eof */
