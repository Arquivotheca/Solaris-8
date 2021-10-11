/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_mod.c	1.1	99/05/21 SMI"


/* kernel module packaging for ACPI interpreter */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#include <sys/mutex.h>
#include <sys/modctl.h>

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


extern kmutex_t acpi_mutex;
extern unsigned int acpi_ld_cnt;

int spec_thread;
struct acpi_thread main_thread, special_thread;

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

/*
 * ACPI loadable module wrapper
 */

static struct modlmisc acpi_intp_modmisc = {
	&mod_miscops,
	"ACPI Interpreter",
};

static struct modlinkage acpi_intp_modlink = {
	MODREV_1,
	(void *)&acpi_intp_modmisc,
	NULL,
};

int
_init(void)
{
	/* fill in ops vector */
	acpi_ops_vector.fn_init = acpi_i_init;
	acpi_ops_vector.fn_uninit_new = uninit_new;
	acpi_ops_vector.fn_integer_new = integer_new;
	acpi_ops_vector.fn_string_new = string_new;
	acpi_ops_vector.fn_buffer_new = buffer_new;
	acpi_ops_vector.fn_package_new = package_new;
	acpi_ops_vector.fn_pkg_setn = package_setn;
	acpi_ops_vector.fn_val_free = value_free;
	acpi_ops_vector.fn_nameseg = acpi_i_nameseg;
	acpi_ops_vector.fn_objtype = acpi_i_objtype;
	acpi_ops_vector.fn_eval = acpi_i_eval;
	acpi_ops_vector.fn_eval_nameseg = acpi_i_eval_nameseg;
	acpi_ops_vector.fn_rootobj = acpi_i_rootobj;
	acpi_ops_vector.fn_nextobj = acpi_i_nextobj;
	acpi_ops_vector.fn_childobj = acpi_i_childobj;
	acpi_ops_vector.fn_parentobj = acpi_i_parentobj;
	acpi_ops_vector.fn_nextdev = acpi_i_nextdev;
	acpi_ops_vector.fn_childdev = acpi_i_childdev;
	acpi_ops_vector.fn_findobj = acpi_i_findobj;
	acpi_ops_vector.fn_gl_acquire = acpi_gl_acquire;
	acpi_ops_vector.fn_gl_release = acpi_gl_release;
	acpi_ops_vector.fn_cb_register = acpi_i_cb_register;
	acpi_ops_vector.fn_cb_cancel = acpi_i_cb_cancel;

	return (mod_install(&acpi_intp_modlink));
}

int
_fini(void)
{
	if (acpi_ld_cnt > 0 || MUTEX_HELD(&acpi_mutex))
		return (EBUSY);
	return (mod_remove(&acpi_intp_modlink));
}

int
_info(struct modinfo *mip)
{
	return (mod_info(&acpi_intp_modlink, mip));
}


/* eof */
