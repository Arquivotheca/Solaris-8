/*
 * Copyright (c) 1990,1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident  "@(#)klmmod.c 1.17     98/05/14 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <nfs/lm.h>

static struct modlmisc modlmisc = {
	&mod_miscops, "lock mgr common module"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

char _depends_on[] = "strmod/rpcmod fs/nfs";

_init()
{
	int retval;

	mutex_init(&lm_lck, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&lm_status_cv, NULL, CV_DEFAULT, NULL);
	rw_init(&lm_sysids_lock, NULL, RW_DEFAULT, NULL);

	retval = mod_install(&modlinkage);
	if (retval != 0) {
		/*
		 * Clean up previous initialization work.
		 */
		mutex_destroy(&lm_lck);
		cv_destroy(&lm_status_cv);
		rw_destroy(&lm_sysids_lock);
	}

	return (retval);
}

_fini()
{
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{

	return (mod_info(&modlinkage, modinfop));
}

#ifdef __lock_lint

/*
 * Stub function for warlock only - this is never compiled or called.
 */
void
klmmod_null()
{
}

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/lm_server.h>

/*
 * Function for warlock only - this is never compiled or called.
 *
 * It obtains locks which must be held while calling a few functions which
 * are normally only called from klmops.  This allows warlock to analyze
 * these functions.
 */
void
klmmod_lock_held_roots()
{
	lm_block_t lmb, *lmbp;
	struct flock64 flk64;
	struct lm_vnode lv;
	netobj netobj;

	mutex_enter(&lm_lck);
	(void) lm_add_block(&lmb);
	(void) lm_cancel_granted_rxmit(&flk64, &lv);
	(void) lm_dump_block(&lv);
	(void) lm_find_block(&flk64, &lv, &netobj, &lmbp);
	(void) lm_init_block(&lmb, &flk64, &lv, &netobj);
	(void) lm_remove_block(&lmb);
	mutex_exit(&lm_lck);
}
#endif __lock_lint
