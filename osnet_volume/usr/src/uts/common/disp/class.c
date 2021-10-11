/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)class.c	1.3	99/07/29 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/class.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/modctl.h>
#include <sys/disp.h>

/*
 * Allocate a cid given a class name if one is not already allocated.
 * Returns 0 if the cid was already exists or if the allocation of a new
 * cid was successful. Nonzero return indicates error.
 */
int
alloc_cid(char *clname, id_t *cidp)
{
	sclass_t *clp;

	ASSERT(MUTEX_HELD(&class_lock));

	/*
	 * If the clname doesn't already have a cid, allocate one.
	 */
	if (getcidbyname(clname, cidp) != 0) {
		/*
		 * Allocate a class entry and a lock for it.
		 */
		for (clp = sclass; clp < &sclass[nclass]; clp++)
			if (clp->cl_name[0] == '\0' && clp->cl_lock == NULL)
				break;

		if (clp == &sclass[nclass]) {
			return (ENOSPC);
		}
		*cidp = clp - &sclass[0];
		clp->cl_lock = kmem_alloc(sizeof (krwlock_t), KM_SLEEP);
		clp->cl_name = kmem_alloc(strlen(clname) + 1, KM_SLEEP);
		(void) strcpy(clp->cl_name, clname);
		rw_init(clp->cl_lock, NULL, RW_DEFAULT, NULL);
	}

	/*
	 * At this point, *cidp will contain the index into the class
	 * array for the given class name.
	 */
	return (0);
}

int
scheduler_load(char *clname, sclass_t *clp)
{
	if (LOADABLE_SCHED(clp)) {
		rw_enter(clp->cl_lock, RW_READER);
		while (!SCHED_INSTALLED(clp)) {
			rw_exit(clp->cl_lock);
			if (modload("sched", clname) == -1)
				return (EINVAL);
			rw_enter(clp->cl_lock, RW_READER);
		}
		rw_exit(clp->cl_lock);
	}
	return (0);
}

/*
 * Get class ID given class name.
 */
int
getcid(char *clname, id_t *cidp)
{
	sclass_t *clp;
	int retval;

	mutex_enter(&class_lock);
	if ((retval = alloc_cid(clname, cidp)) == 0) {
		clp = &sclass[*cidp];
		clp->cl_count++;

		/*
		 * If it returns zero, it's loaded & locked
		 * or we found a statically installed scheduler
		 * module.
		 * If it returns EINVAL, modload() failed when
		 * it tried to load the module.
		 */
		mutex_exit(&class_lock);
		retval = scheduler_load(clname, clp);
		mutex_enter(&class_lock);

		clp->cl_count--;
		if (retval != 0 && clp->cl_count == 0) {
			/* last guy out of scheduler_load frees the storage */
			kmem_free(clp->cl_name, strlen(clname) + 1);
			kmem_free(clp->cl_lock, sizeof (krwlock_t));
			clp->cl_name = "";
			clp->cl_lock = (krwlock_t *)NULL;
		}
	}
	mutex_exit(&class_lock);
	return (retval);

}

/*
 * Lookup a module by name.
 */
int
getcidbyname(char *clname, id_t *cidp)
{
	sclass_t *clp;

	if (*clname == NULL)
		return (EINVAL);

	ASSERT(MUTEX_HELD(&class_lock));

	for (clp = &sclass[0]; clp < &sclass[nclass]; clp++) {
		if (strcmp(clp->cl_name, clname) == 0) {
			*cidp = clp - &sclass[0];
			return (0);
		}
	}
	return (EINVAL);
}

/*
 * Get the scheduling parameters of the thread pointed to by
 * tp into the buffer pointed to by parmsp.
 */
void
parmsget(kthread_id_t tp, pcparms_t *parmsp)
{
	parmsp->pc_cid = tp->t_cid;
	CL_PARMSGET(tp, parmsp->pc_clparms);
}


/*
 * Check the validity of the scheduling parameters in the buffer
 * pointed to by parmsp. If our caller passes us non-NULL process
 * pointers we are also being asked to verify that the requesting
 * process (pointed to by reqpp) has the necessary permissions to
 * impose these parameters on the target process (pointed to by
 * targpp).
 * We check validity before permissions because we assume the user
 * is more interested in finding out about invalid parms than a
 * permissions problem.
 * Note that the format of the parameters may be changed by class
 * specific code which we call.
 */
int
parmsin(pcparms_t *parmsp, kthread_id_t reqtp, kthread_id_t targtp)
{
	int		error;
	id_t		reqpcid;
	id_t		targpcid;
	cred_t		*reqpcredp;
	cred_t		*targpcredp;
	caddr_t		targpclpp;
	proc_t		*reqpp = NULL;
	proc_t		*targpp = NULL;

	if (parmsp->pc_cid >= loaded_classes || parmsp->pc_cid < 1)
		return (EINVAL);

	if (reqtp != NULL)
		reqpp = ttoproc(reqtp);
	if (targtp != NULL)
		targpp = ttoproc(targtp);
	if (reqpp != NULL && targpp != NULL) {
		reqpcid = reqtp->t_cid;
		mutex_enter(&reqpp->p_crlock);
		crhold(reqpcredp = reqpp->p_cred);
		mutex_exit(&reqpp->p_crlock);
		targpcid = targtp->t_cid;
		mutex_enter(&targpp->p_crlock);
		crhold(targpcredp = targpp->p_cred);
		mutex_exit(&targpp->p_crlock);
		targpclpp = targtp->t_cldata;
	} else {
		reqpcredp = targpcredp = NULL;
		targpclpp = NULL;
	}

	/*
	 * Call the class specific routine to validate class
	 * specific parameters.  Note that the data pointed to
	 * by targpclpp is only meaningful to the class specific
	 * function if the target process belongs to the class of
	 * the function.
	 */
	error = CL_PARMSIN(&sclass[parmsp->pc_cid], parmsp->pc_clparms,
		reqpcid, reqpcredp, targpcid, targpcredp, targpclpp);
	if (error) {
		if (reqpcredp != NULL) {
			crfree(reqpcredp);
			crfree(targpcredp);
		}
		return (error);
	}

	if (reqpcredp != NULL) {
		/*
		 * Check the basic permissions required for all classes.
		 */
		if (!hasprocperm(targpcredp, reqpcredp)) {
			crfree(reqpcredp);
			crfree(targpcredp);
			return (EPERM);
		}
		crfree(reqpcredp);
		crfree(targpcredp);
	}
	return (0);
}


/*
 * Call the class specific code to do the required processing
 * and permissions checks before the scheduling parameters
 * are copied out to the user.
 * Note that the format of the parameters may be changed by the
 * class specific code.
 */
int
parmsout(pcparms_t *parmsp, kthread_id_t targtp)
{
	int	error;
	id_t	reqtcid;
	id_t	targtcid;
	cred_t	*reqpcredp;
	cred_t	*targpcredp;
	proc_t	*reqpp = ttoproc(curthread);
	proc_t	*targpp = ttoproc(targtp);

	reqtcid = curthread->t_cid;
	targtcid = targtp->t_cid;
	mutex_enter(&reqpp->p_crlock);
	crhold(reqpcredp = reqpp->p_cred);
	mutex_exit(&reqpp->p_crlock);
	mutex_enter(&targpp->p_crlock);
	crhold(targpcredp = targpp->p_cred);
	mutex_exit(&targpp->p_crlock);

	error = CL_PARMSOUT(&sclass[parmsp->pc_cid], parmsp->pc_clparms,
		reqtcid, reqpcredp, targtcid, targpcredp, targpp);

	crfree(reqpcredp);
	crfree(targpcredp);
	return (error);
}


/*
 * Set the scheduling parameters of the thread pointed to by
 * targtp to those specified in the pcparms structure pointed
 * to by parmsp.  If reqtp is non-NULL it points to the thread
 * that initiated the request for the parameter change and indicates
 * that our caller wants us to verify that the requesting thread
 * has the appropriate permissions.
 */
int
parmsset(pcparms_t *parmsp, kthread_id_t targtp)
{
	caddr_t	clprocp;
	int	error;
	cred_t	*reqpcredp;
	proc_t	*reqpp = ttoproc(curthread);
	proc_t	*targpp = ttoproc(targtp);
	id_t	oldcid;

	ASSERT(MUTEX_HELD(&pidlock));
	ASSERT(MUTEX_HELD(&targpp->p_lock));
	if (reqpp != NULL) {
		mutex_enter(&reqpp->p_crlock);
		crhold(reqpcredp = reqpp->p_cred);
		mutex_exit(&reqpp->p_crlock);

		/*
		 * Check basic permissions.
		 */
		mutex_enter(&targpp->p_crlock);
		if (!hasprocperm(targpp->p_cred, reqpcredp)) {
			mutex_exit(&targpp->p_crlock);
			crfree(reqpcredp);
			return (EPERM);
		}
		mutex_exit(&targpp->p_crlock);
	} else {
		reqpcredp = NULL;
	}

	if (parmsp->pc_cid != targtp->t_cid) {
		void	*bufp = NULL;
		/*
		 * Target thread must change to new class.
		 */
		clprocp = (caddr_t)targtp->t_cldata;
		oldcid  = targtp->t_cid;

		/*
		 * Purpose: allow scheduling class to veto moves
		 * to other classes. All the classes, except SRM,
		 * do nothing except returning 0.
		 */
		error = CL_CANEXIT(targtp, reqpcredp);
		if (error) {
			/*
			 * Not allowed to leave the class, so return error.
			 */
			crfree(reqpcredp);
			return (error);
		} else {
			/*
			 * Pre-allocate scheduling class data.
			 */
			if (CL_ALLOC(&bufp, parmsp->pc_cid, KM_NOSLEEP) != 0) {
				error = ENOMEM; /* no memory available */
				crfree(reqpcredp);
				return (error);
			} else {
				error = CL_ENTERCLASS(targtp, parmsp->pc_cid,
				    parmsp->pc_clparms, reqpcredp, bufp);
				crfree(reqpcredp);
				if (error) {
					CL_FREE(parmsp->pc_cid, bufp);
					return (error);
				}
			}
		}
		CL_EXITCLASS(oldcid, clprocp);
	} else {

		/*
		 * Not changing class
		 */
		error = CL_PARMSSET(targtp, parmsp->pc_clparms,
					curthread->t_cid, reqpcredp);
		crfree(reqpcredp);
		if (error)
			return (error);
	}
	return (0);
}
