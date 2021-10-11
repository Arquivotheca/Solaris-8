/*
 * Copyright (c) 1992-2000, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_mod.c	1.67	99/10/19 SMI"

/*
 * System call to checkpoint and resume the currently running kernel
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/cred.h>
#include <sys/uadmin.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/cpr.h>
#include <sys/swap.h>
#include <sys/vfs.h>

extern int i_cpr_is_supported(void);
extern int cpr_is_ufs(struct vfs *);
extern int cpr_check_spec_statefile(void);
extern int cpr_reusable_mount_check(void);
extern void cpr_forget_cprconfig(void);
extern int i_cpr_reusable_supported(void);
extern int i_cpr_reusefini(void);

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "checkpoint resume"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

char _depends_on[] = "misc/bootdev";	/* i_devname_to_promname() */

int cpr_reusable_mode;

kmutex_t	cpr_slock;	/* cpr serial lock */
cpr_t		cpr_state;
int		cpr_debug;
int		cpr_test_mode; /* true if called via uadmin testmode */

/*
 * All the loadable module related code follows
 */
int
_init(void)
{
	register int e;

	if ((e = mod_install(&modlinkage)) == 0) {
		mutex_init(&cpr_slock, NULL, MUTEX_DEFAULT, NULL);
	}
	return (e);
}

int
_fini(void)
{
	register int e;

	if ((e = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&cpr_slock);
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
cpr(int fcn)
{
	static const char noswapstr[] = "reusable statefile requires "
	    "that no swap area be configured.\n";
	static const char blockstr[] = "cpr: statefile must be "
	    "on block special device for reusable statefile.\n"
	    "See power.conf(4) and pmconfig(1M).\n";
	static const char normalfmt[] = "cpr: cannot do normal "
	    "checkpoint/resume when in reusable statefile mode.\n"
	    "use uadmin A_FREEZE AD_REUSEFINI (uadmin %d %d) "
	    "to exit reusable statefile mode.\n";
	register int rc = 0;
	extern int cpr_init(int);
	extern void cpr_done(void);

	/*
	 * Need to know if we're in reusable mode, but we will likely have
	 * rebooted since REUSEINIT, so we have to get the info from the
	 * file system
	 */
	if (!cpr_reusable_mode)
		cpr_reusable_mode = cpr_get_reusable_mode();

	cpr_forget_cprconfig();
	switch (fcn) {

	case AD_CPR_REUSEINIT:
		if (!i_cpr_reusable_supported())
			return (ENOTSUP);
		if (!cpr_statefile_is_spec()) {
			cmn_err(CE_CONT, blockstr);
			return (EINVAL);
		}
		if ((rc = cpr_check_spec_statefile()) != 0)
			return (rc);
		if (swapinfo) {
			cmn_err(CE_CONT, noswapstr);
			return (EINVAL);
		}
		cpr_test_mode = 0;
		break;

	case AD_CPR_NOCOMPRESS:
	case AD_CPR_COMPRESS:
	case AD_CPR_FORCE:
		if (cpr_reusable_mode) {
			cmn_err(CE_CONT, normalfmt, A_FREEZE, AD_REUSEFINI);
			return (ENOTSUP);
		}
		cpr_test_mode = 0;
		break;

	case AD_CPR_REUSABLE:
		if (!i_cpr_reusable_supported())
			return (ENOTSUP);
		if (!cpr_statefile_is_spec()) {
			cmn_err(CE_CONT, blockstr);
			return (EINVAL);
		}
		if ((rc = cpr_check_spec_statefile()) != 0)
			return (rc);
		if (swapinfo) {
			cmn_err(CE_CONT, noswapstr);
			return (EINVAL);
		}
		if ((rc = cpr_reusable_mount_check()) != 0)
			return (rc);
		cpr_test_mode = 0;
		break;

	case AD_CPR_REUSEFINI:
		if (!i_cpr_reusable_supported())
			return (ENOTSUP);
		cpr_test_mode = 0;
		break;

	case AD_CPR_TESTZ:
	case AD_CPR_TESTNOZ:
	case AD_CPR_TESTHALT:
		if (cpr_reusable_mode) {
			cmn_err(CE_CONT, normalfmt, A_FREEZE, AD_REUSEFINI);
			return (ENOTSUP);
		}
		cpr_test_mode = 1;
		break;

	case AD_CPR_CHECK:
		if (!i_cpr_is_supported() || cpr_reusable_mode)
			return (ENOTSUP);
		return (0);

	case AD_CPR_PRINT:
		CPR_STAT_EVENT_END("POST CPR DELAY");
		cpr_stat_event_print();
		return (0);

	case AD_CPR_DEBUG0:
		cpr_debug = 0;
		return (0);

	case AD_CPR_DEBUG1:
	case AD_CPR_DEBUG2:
	case AD_CPR_DEBUG3:
	case AD_CPR_DEBUG4:
	case AD_CPR_DEBUG5:
	case AD_CPR_DEBUG7:
	case AD_CPR_DEBUG8:
		cpr_debug |= CPR_DEBUG_BIT(fcn);
		return (0);

	case AD_CPR_DEBUG9:
		cpr_debug |= LEVEL6;
		return (0);

	default:
		return (ENOTSUP);
	}

	if (!i_cpr_is_supported() || !cpr_is_ufs(rootvfs))
		return (ENOTSUP);

	if (fcn == AD_CPR_REUSEINIT) {
		if (mutex_tryenter(&cpr_slock) == 0)
			return (EBUSY);
		if (cpr_reusable_mode) {
			cmn_err(CE_CONT, "cpr: already in reusable statefile "
			    "mode.\n");
			mutex_exit(&cpr_slock);
			return (EBUSY);
		}
		rc = i_cpr_reuseinit();
		mutex_exit(&cpr_slock);
		return (rc);
	}

	if (fcn == AD_CPR_REUSEFINI) {
		if (mutex_tryenter(&cpr_slock) == 0)
			return (EBUSY);
		if (!cpr_reusable_mode) {
			cmn_err(CE_CONT, "cpr: not in reusable statefile "
			    "mode.\n");
			mutex_exit(&cpr_slock);
			return (EINVAL);
		}
		rc = i_cpr_reusefini();
		mutex_exit(&cpr_slock);
		return (rc);
	}


	/*
	 * acquire cpr serial lock and init cpr state structure.
	 */
	if (rc = cpr_init(fcn))
		return (rc);

	if (fcn == AD_CPR_REUSABLE) {
		if ((rc = i_cpr_check_cprinfo()) != 0)  {
			mutex_exit(&cpr_slock);
			return (rc);
		}
	}

	/*
	 * Call the main cpr routine. If we are successful, we will be coming
	 * down from the resume side, otherwise we are still in suspend.
	 */
	cmn_err(CE_CONT, "cpr: System is being suspended.\n");
	if (rc = cpr_main()) {
		CPR->c_flags |= C_ERROR;
		cmn_err(CE_NOTE, "cpr: Suspend operation failed.");
	} else if (CPR->c_flags & C_SUSPENDING) {
		extern void cpr_power_down();
		/*
		 * Back from a successful checkpoint
		 */
		if (fcn == AD_CPR_TESTZ || fcn == AD_CPR_TESTNOZ) {
			mdboot(0, AD_BOOT, "");
			/* NOTREACHED */
		}

		/*
		 * If cpr_power_down() succeeds, it'll not return.
		 */
		if (fcn != AD_CPR_TESTHALT)
			cpr_power_down();

		errp("(Done. Please Switch Off)\n");
		halt(NULL);
		/* NOTREACHED */
	}
	/*
	 * For resuming: release resources and the serial lock.
	 */
	cpr_done();
	return (rc);
}
