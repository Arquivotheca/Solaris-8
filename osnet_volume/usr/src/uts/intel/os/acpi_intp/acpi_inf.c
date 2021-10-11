/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_inf.c	1.1	99/05/21 SMI"


/*
 * interface functions
 */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_elem.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_tab.h"
#include "acpi_io.h"
#include "acpi_inf.h"


#ifdef ACPI_BOOT
extern void wait100ms(void);
#endif
#ifdef ACPI_KERNEL
#include <sys/systm.h>
#endif


/*
 * ACPI init
 * state arg is the desired state (see acpi_prv.h) after finishing
 */
int
acpi_i_init(int state)
{
	if (current_acpi_thread) /* should not happen */
		return (ACPI_EXC);
	acpi_threads = &main_thread;
	current_acpi_thread = &main_thread;
	if ((acpi_state == ACPI_START || acpi_state == ACPI_INIT0) &&
	    table_std_load() == ACPI_EXC)
		goto i_init_exc;
	if (acpi_state != ACPI_INITED && state == ACPI_INITED &&
	    table_all_ddb_load(current_acpi_thread) == ACPI_EXC)
		goto i_init_exc;
	current_acpi_thread = NULL;
	return (ACPI_OK);
i_init_exc:
	current_acpi_thread = NULL;
	return (ACPI_EXC);
}

/* acpi_disable defined elsewhere */
/* acpi_facs_get defined elsewhere */
/* acpi_facp_get defined elsewhere */
/* acpi_apic_get defined elsewhere */
/* acpi_sbst_get defined elsewhere */
/* acpi_uninit_new defined elsewhere */
/* acpi_integer_new defined elsewhere */
/* acpi_string_new defined elsewhere */
/* acpi_buffer_new defined elsewhere */
/* acpi_package_new defined elsewhere */
/* acpi_pkg_setn defined elsewhere */
/* acpi_val_free defined elsewhere */

acpi_nameseg_t
acpi_i_nameseg(acpi_obj obj)
{
	acpi_nameseg_t seg;
	ns_elem_t *nsp = obj;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL) {
		seg.iseg = ACPI_ONES;
		return (seg);	/* really ACPI_EXC */
	}
	seg = nsp->name_seg;
	acpi_thread_release(threadp);
	return (seg);
}

unsigned short
acpi_i_objtype(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	unsigned short type;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return ((unsigned short)ACPI_EXC);
	type = nsp->valp->type;
	acpi_thread_release(threadp);
	return (type);
}

static int
acpi_eval_common(struct acpi_thread *threadp, ns_elem_t *nsp,
    acpi_val_t *args, acpi_val_t **retpp)
{
	value_entry_t ve;
	acpi_val_t *avp = nsp->valp;

	switch (avp->type) {
	case ACPI_UNINIT:
	case ACPI_INTEGER:
	case ACPI_STRING:
	case ACPI_BUFFER:
	case ACPI_PACKAGE:
	case ACPI_DEVICE:
	case ACPI_EVENT:
	case ACPI_MUTEX:
	case ACPI_REGION:
	case ACPI_POWER_RES:
	case ACPI_PROCESSOR:
	case ACPI_THERMAL_ZONE:
	case ACPI_DDB_HANDLE:
	case ACPI_DEBUG_OBJ:
	case ACPI_REF:
		break;
	case ACPI_FIELD:
	case ACPI_BUFFER_FIELD:
		ve.elem = V_ACPI_VALUE;
		ve.data = nsp->valp;
		if (acpi_load(&ve, NULL, &avp, 0) == ACPI_EXC)
			return (ACPI_EXC);
		break;
	case ACPI_METHOD:
		exc_debug(ACPI_DEXE, "ENTER eval_driver");
		if (eval_driver(threadp, nsp, args, &avp, 256) == ACPI_EXC)
			return (ACPI_EXC);
		break;
	default:
		return (ACPI_EXC);
	}
	if (retpp) {
		value_hold(avp);
		*retpp = avp;
	}
	return (ACPI_OK);
}



int
acpi_i_eval(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp)
{
	struct acpi_thread *threadp;
	int ret = ACPI_OK;

	exc_debug(ACPI_DEXE, "ENTER acpi_eval");
	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (ACPI_EXC);
	if (acpi_eval_common(threadp, obj, args, retpp) == ACPI_EXC)
		ret = ACPI_EXC;
	acpi_thread_release(threadp);
	exc_debug(ACPI_DEXE, "EXIT acpi_eval");
	return (ret);
}

int
acpi_i_eval_nameseg(acpi_obj obj, acpi_nameseg_t *segp, acpi_val_t *args,
    acpi_val_t **retpp)
{
	ns_elem_t *nsp;
	struct {
		name_t hdr;
		acpi_nameseg_t seg;
	} oneseg;
	int ret = ACPI_OK;
	struct acpi_thread *threadp;

	exc_debug(ACPI_DEXE, "ENTER acpi_eval_nameseg");
	if (obj == NULL)
		return (ACPI_EXC);
	bzero(&oneseg, sizeof (oneseg));
	oneseg.hdr.segs = 1;
	oneseg.seg.iseg = segp->iseg;
	if ((threadp = acpi_thread_get(0)) == NULL)
		return (ACPI_EXC);
	if (ns_lookup(root_ns, obj, (name_t *)&oneseg, 0, 0, &nsp, NULL) ==
	    ACPI_EXC)
		ret = ACPI_EXC;
	else if (acpi_eval_common(threadp, nsp, args, retpp) == ACPI_EXC)
		ret = ACPI_EXC;
	acpi_thread_release(threadp);
	exc_debug(ACPI_DEXE, "EXIT acpi_eval_nameseg");
	return (ret);
}

acpi_obj
acpi_i_rootobj(void)
{
	acpi_obj ret;
	struct acpi_thread *threadp;

	if ((threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	ret = root_ns;
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_nextobj(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	acpi_obj ret;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	ret = nsp->node.next;
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_childobj(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	acpi_obj ret;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	ret = nsp->node.child;
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_parentobj(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	acpi_obj ret;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	ret = nsp->node.parent;
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_nextdev(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	ns_elem_t *ptr;
	acpi_obj ret = NULL;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	for (ptr = (ns_elem_t *)(nsp->node.next); ptr;
	    ptr = (ns_elem_t *)(ptr->node.next))
		if (ptr->valp->type == ACPI_DEVICE) {
			ret = ptr;
			break;
		}
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_childdev(acpi_obj obj)
{
	ns_elem_t *nsp = obj;
	ns_elem_t *ptr;
	acpi_obj ret = NULL;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	for (ptr = (ns_elem_t *)(nsp->node.child); ptr;
			ptr = (ns_elem_t *)(ptr->node.next))
		if (ptr->valp->type == ACPI_DEVICE) {
			ret = ptr;
			break;
		}
	acpi_thread_release(threadp);
	return (ret);
}

acpi_obj
acpi_i_findobj(acpi_obj obj, char *name, int flags)
{
	name_t *namep;
	ns_elem_t *nsp;
	acpi_obj ret;
	struct acpi_thread *threadp;

	if (obj == NULL || (threadp = acpi_thread_get(0)) == NULL)
		return (NULL);
	if ((namep = name_get(name)) == NULL)
		ret = NULL;
	else {
		ret = (ns_lookup(root_ns, obj, namep, 0, flags, &nsp, NULL) !=
		    ACPI_OK) ? NULL : nsp;
		name_free(namep);
	}
	acpi_thread_release(threadp);
	return (ret);
}

/* XXX should use acpi_gl_acquire, acpi_gl_release in acpi_ml.s */
/*LINTLIBRARY*/
int
acpi_i_gl_acquire(void)
{
	return (ACPI_OK);
}

/*LINTLIBRARY*/
int
acpi_i_gl_release(void)
{
	return (ACPI_OK);
}

acpi_cbid_t
acpi_i_cb_register(acpi_obj obj, acpi_cbfn_t fn, void *cookie)
{
	acpi_val_t *valp;
	acpi_cb_t *cbp, *ptr, *trail;

	if (obj == NULL || fn == NULL)
		return (NULL);
	valp = ((ns_elem_t *)obj)->valp;
	if (valp->type != ACPI_DEVICE && valp->type != ACPI_THERMAL_ZONE)
		return (exc_null(ACPI_ETYPE));
	if ((cbp = kmem_alloc(sizeof (acpi_cb_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	cbp->next = NULL;
	cbp->obj = obj;
	cbp->fn = fn;
	cbp->cookie = cookie;

	if (valp->acpi_valp == NULL) {
		cbp->prev = (acpi_cb_t *)valp;
		cbp->flags = CB_FIRST;
		valp->acpi_valp = cbp;
	} else {
		for (ptr = valp->acpi_valp; ptr; ) {
			trail = ptr;
			ptr = ptr->next;
		}
		cbp->prev = trail;
		cbp->flags = 0;
		trail->next = cbp;
	}

	return (cbp);
}

int
acpi_i_cb_cancel(acpi_cbid_t id)
{
	acpi_cb_t *cbp = (acpi_cb_t *)id;
	acpi_val_t *valp;

	if (cbp == NULL)
		return (ACPI_EXC);
	if (cbp->flags & CB_FIRST) {
		valp = (acpi_val_t *)cbp->prev;
		valp->acpi_valp = cbp->next;
		cbp->next->flags |= CB_FIRST;
	} else
		cbp->prev->next = cbp->next;
	cbp->next->prev = cbp->prev;
	kmem_free(cbp, sizeof (acpi_cb_t));
	return (ACPI_OK);
}

/* acpi_ld_register defined elsewhere */
/* acpi_ld_cancel defined elsewhere */


/* utility routines */
void
acpi_delay_sig(unsigned int msec)
{
#ifdef ACPI_BOOT
	int i;
	int loops = (msec + 99) / 100;

	for (i = 0; i < loops; i++)
		wait100ms();
#endif
#ifdef ACPI_KERNEL
	int ticks = (msec * hz + 999) / 1000;

	if (ticks > 0)
		delay_sig((clock_t)ticks);
#endif
}


/* eof */
