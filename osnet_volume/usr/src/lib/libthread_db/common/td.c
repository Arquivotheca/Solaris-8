/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)td.c	1.57	99/08/10 SMI"

/*
*  Description:
*	Main module for td.  Globals are defined in this file.
*/

/*
 * Define TD_INITIALIZER.  It is used in the td.h file to determine
 * how global variables are declared.  By defining TD_INITIALIZER
 * here, the definition of globals variables is done in this
 * file and other files will declare them as extern.
 */
#define		TD_INITIALIZER

#pragma weak td_ta_new = __td_ta_new
#pragma weak td_ta_delete = __td_ta_delete
#pragma weak td_init = __td_init
#pragma weak td_log = __td_log

/*
 * These three proc_service entry points need to be weak for compatibility
 * with older libthread_db clients.
 */
#pragma weak ps_lrolltoaddr
#pragma weak ps_kill
#pragma weak ps_pdmodel

#include "td_impl.h"

/*
 * The #define TEST_PS_CALLS and the global td_noop (also only defined
 * when TEST_PS_CALLS is defined, can be used to test proc_service
 * calls made by a function.  The proc_service functions used for
 * testing will mark its position in an array.  Calling the function
 * with and without td_noop can be used to check the proc_service
 * functions that are called (the proc_service functions called
 * under the guard of the td_noop variable should equal those
 * called by the function (directly and indirectly).  This can
 * be used to check the documentation on which functions proc_service
 * are called.
 */


/*
 * td_log
 *
 * This function does nothing, and never did.  But we have exported
 * the symbol, so we can't delete it.
 */
void
__td_log()

{}


/*
* Allocate a thread agent and return a pointer to it.
*
* Input:
*    *ph_p - process handle defined by debugger
*
* Output:
*    *ta_pp - thread agent for input process handle.
*    td_ta_new - return value
*
* Side effects:
*    Storage for thread agent is allocated.
*    Imported functions called: ps_pglobal_lookup().
*/
td_err_e
__td_ta_new(struct ps_prochandle * ph_p,
    td_thragent_t **ta_pp)
{
	td_err_e	return_val = TD_OK;
	td_thragent_t	*ta_p;
	psaddr_t	tdb_invar_addr;
	tdb_agt_stat_t	start_agent = TDB_START_AGENT;
	ps_err_e	db_return;
	int		is_core_file = 0;
	ps_err_e	(*ps_kill_p)(struct ps_prochandle *, int);
	ps_err_e	ps_ret = PS_OK;
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * ps_kill is weak, and might not be defined if this is
	 * an older client; so don't call ps_kill if it is NULL.
	 */
	ps_kill_p = ps_kill;

	/*
	 * Check that the pointers ph_p and  ta_pp are okay.
	 */
	if (ph_p == NULL) {
		__td_report_po_err(TD_BADPH,
		    "Input parameter error - td_ta_new");
		return (TD_BADPH);
	}

	if (ta_pp == NULL) {
		__td_report_po_err(TD_ERR,
		    "Input parameter error - td_ta_new");
		return (TD_ERR);
	}
	*ta_pp = NULL;

	ta_p = (td_thragent_t *) calloc(1, sizeof (*ta_p));
	if (ta_p == NULL) {
		__td_report_po_err(TD_ERR,
		    "Malloc failed - td_ta_new");
		return (TD_MALLOC);
	}

	ta_p->ph_p = ph_p;

	if (__td_pdmodel(ph_p, &model) != PS_OK) {
		free(ta_p);
		return (TD_ERR);
	}

	/*
	 * Initialize the rwlock for the thread agent.
	 */
	rwlock_init(&ta_p->rwlock, USYNC_THREAD, NULL);

	if (ps_pstop(ph_p) != PS_OK) {
		free(ta_p);
		return (TD_DBERR);
	}

	/*
	 * Pick up various symbol values from the target process.
	 */
	if ((db_return = ps_pglobal_lookup(ph_p, TD_LIBTHREAD_NAME,
	    TD_INVAR_DATA_NAME, &tdb_invar_addr)) == PS_OK) {
		if (model == PR_MODEL_NATIVE) {
			db_return = ps_pdread(ph_p, tdb_invar_addr,
			    (char *) &ta_p->tdb_invar,
			    sizeof (ta_p->tdb_invar));
		}
#ifdef _SYSCALL32_IMPL
		else {
			db_return = ps_pdread(ph_p, tdb_invar_addr,
			    (char *) &ta_p->tdb_invar32,
			    sizeof (ta_p->tdb_invar32));
		}
#endif /* _SYSCALL32_IMPL */
	}

	if (db_return == PS_NOSYM) {
		/* Assume libthread is not linked. */
		return_val = TD_NOLIBTHREAD;
	} else if (db_return != PS_OK) {
		return_val = TD_ERR;
	}

	/*
	 * Start the tdb agent thread in the target process, unless
	 * it's an older client that didn't define ps_kill; in that
	 * case, the client should not be calling any routines that
	 * require the agent, anyway.  If the ps_pdwrite call fails,
	 * assume we're attached to a core file, not a live process,
	 * and don't try to start the agent thread.
	 */
	if (ps_kill_p != NULL) {
		if (model == PR_MODEL_NATIVE) {
			ps_ret = ps_pdwrite(ph_p,
			    (psaddr_t) ta_p->tdb_invar.tdb_agent_stat_addr,
			    (char *) &start_agent, sizeof (start_agent));
		}
#ifdef _SYSCALL32_IMPL
		else {
			ps_ret = ps_pdwrite(ph_p,
			    (psaddr_t) ta_p->tdb_invar32.tdb_agent_stat_addr,
			    (char *) &start_agent, sizeof (start_agent));
		}
#endif /* _SYSCALL32_IMPL */
		if (return_val == TD_OK && ps_ret != PS_OK)
			is_core_file = 1;
		if (return_val == TD_OK && !is_core_file &&
		    (*ps_kill_p)(ph_p, SIGLWP) != PS_OK)
			return_val = TD_DBERR;
	}

	if (ps_pcontinue(ph_p) != PS_OK)
		return_val = TD_DBERR;

	if (return_val == TD_OK)
		*ta_pp = ta_p;
	else
		free(ta_p);

	return (return_val);
}


/*
* Description:
*    Deallocate the thread agent.
*
* Input:
*    ta_p - thread agent pointer
*
* Output:
*    ta_delete - return value
*
* Side effects:
*    Storage for thread agent is not deallocated.  The prochandle
* in the thread agent is set to NULL so that future uses of
* the thread agent can be detected and an error value returned.
* All functions in the external user interface that make
* use of the thread agent are expected
* to check for a NULL prochandle in the therad agent.
* All such functions are also expect to use obtain a
* reader lock on the thread agent while it is using it.
*    Imported functions called: none
*/
td_err_e
__td_ta_delete(td_thragent_t *ta_p)
{
	struct ps_prochandle *ph_p;
	int		model;
	tdb_agt_stat_t	stop_agent = TDB_NOT_ATTACHED;

	if (ta_p == NULL) {
		__td_report_po_err(return_val,
		    "NULL thread agent ptr - td_ta_delete");
		return (TD_BADTA);
	}

	/*
	 * Grab a writer lock while thread agent is being deleted.
	 */
	if (rw_wrlock(&ta_p->rwlock))
		return (TD_ERR);

	if ((ph_p = ta_p->ph_p) == NULL) {
		rw_unlock(&ta_p->rwlock);
		return (TD_BADTA);
	}
	ta_p->ph_p = NULL;

	/*
	 * Reset tdb_agent_stat so the target process no longer
	 * thinks it has libthread_db attached.
	 */
	if (__td_pdmodel(ph_p, &model) == 0) {
		if (model == PR_MODEL_NATIVE) {
			(void) ps_pdwrite(ph_p,
			    (psaddr_t)ta_p->tdb_invar.tdb_agent_stat_addr,
			    &stop_agent, sizeof (stop_agent));
		}
#ifdef _SYSCALL32_IMPL
		else {
			(void) ps_pdwrite(ph_p,
			    (psaddr_t)ta_p->tdb_invar32.tdb_agent_stat_addr,
			    &stop_agent, sizeof (stop_agent));
		}
#endif /* _SYSCALL32_IMPL */
	}

	rw_unlock(&ta_p->rwlock);
	return (TD_OK);
}


/*
* Description:
*    Perform initialization for libthread_db
* interface to debugger.
*
* Input:
*    none
*
* Output:
*    td_init - return value
*
* Side effects:
*    none.
*    Imported functions called: none
*    Lock for protecting global data is initialized.
*/
td_err_e
__td_init()
{
	td_err_e	return_val = TD_OK;

	/*
	 * Initialize global data lock(s).  Initialize only once
	 * based on td_initialized flag.
	 */
	if (!td_initialized) {
		if (mutex_init(&__gd_lock, USYNC_THREAD, 0) != 0) {
			return_val = TD_ERR;
		}
	}

	td_initialized = 1;
	return (return_val);
}


/*
 * Send a request to the thread_db agent in the target process, and
 * collect the response.  Return an error if something goes wrong.
 */
td_err_e
__td_agent_send(td_thragent_t *ta_p, tdb_ctl_t *agent_msg)

{
#if defined(AGENT_TEST)
	tdb_ctl_t tmp_msg;
#endif	/* AGENT_TEST */
	int		model = PR_MODEL_NATIVE;

	ps_err_e (*lroll_p)(struct ps_prochandle *,
	    lwpid_t, psaddr_t, psaddr_t);

	/*
	 * Older clients may not have ps_lrolltoaddr; they should not
	 * be calling any routine that calls this.
	 */
	lroll_p = ps_lrolltoaddr;
	if (lroll_p == NULL)
		return (TD_NOCAPAB);

	if (__td_pdmodel(ta_p->ph_p, &model) != PS_OK) {
		return (TD_ERR);
	}
	/*
	 * Make sure the agent thread is running, and that we have
	 * enough information to interact with it.
	 */
	if (ta_p->tdb_agent_data.agent_lwpid == 0) {
		if (model == PR_MODEL_NATIVE) {
			/* Try (re-)reading the agent_data structure */
			if (ps_pdread(ta_p->ph_p,
			    (psaddr_t) ta_p->tdb_invar.tdb_agent_data_addr,
			    (char *) &ta_p->tdb_agent_data,
			    sizeof (ta_p->tdb_agent_data)) != TD_OK)
				return (TD_DBERR);
			if (ta_p->tdb_agent_data.agent_lwpid == 0) {
				/*
				 * XXX Agent not running.  If it's because
				 * we're debugging a core file, that's OK.
				 * If it's because the agent has not completed
				 * initialization yet, this is a (documented)
				 * misfeature that should be fixed in a future
				 * release:  td_ta_new should block until the
				 * agent thread is up and running.
				 */
				return (TD_NOCAPAB);
			}
		}
#ifdef  _SYSCALL32_IMPL
		else {
			/* Try (re-)reading the agent_data structure */
			if (ps_pdread(ta_p->ph_p,
			    (psaddr_t) ta_p->tdb_invar32.tdb_agent_data_addr,
			    (char *) &ta_p->tdb_agent_data32,
			    sizeof (ta_p->tdb_agent_data32)) != TD_OK)
				return (TD_DBERR);
			if (ta_p->tdb_agent_data32.agent_lwpid == 0) {
				/*
				 * XXX Agent not running.  If it's because
				 * we're debugging a core file, that's OK.
				 * If it's because the agent has not completed
				 * initialization yet, this is a (documented)
				 * misfeature that should be fixed in a future
				 * release:  td_ta_new should block until the
				 * agent thread is up and running.
				 */
				return (TD_NOCAPAB);
			}
		}
#endif /* _SYSCALL32_IMPL */
	}

#if defined(AGENT_TEST)
	/*
	 * If the opcode in the agent control structure isn't
	 * NONE_PENDING, something has gone badly wrong.
	 */
	if (model == PR_MODEL_NATIVE) {
		if (ps_pdread(ta_p->ph_p,
		    ta_p->tdb_invar.tdb_agent_ctl_addr,
		    (char *) &tmp_msg, sizeof (tmp_msg)) != TD_OK)
			return (TD_DBERR);
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (ps_pdread(ta_p->ph_p,
		    ta_p->tdb_invar32.tdb_agent_ctl_addr,
		    (char *) &tmp_msg, sizeof (tmp_msg)) != TD_OK)
			return (TD_DBERR);
	}
#endif /* _SYSCALL32_IMPL */
	if (tmp_msg.opcode != NONE_PENDING)
		return (TD_ERR);
#endif	/* defined(AGENT_TEST) */

	/*
	 * Write the agent control structure, roll the agent, then
	 * read back the result.
	 */
	if (model == PR_MODEL_NATIVE) {
		if (ps_pdwrite(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != PS_OK)
			return (TD_DBERR);
		if (ps_lrolltoaddr(ta_p->ph_p,
		    ta_p->tdb_agent_data.agent_lwpid,
		    (psaddr_t) ta_p->tdb_agent_data.agent_go_addr,
		    (psaddr_t) ta_p->tdb_agent_data.agent_stop_addr) != PS_OK)
			return (TD_DBERR);
		if (ps_pdread(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != PS_OK)
			return (TD_DBERR);
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (ps_pdwrite(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar32.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != PS_OK)
			return (TD_DBERR);
		if (ps_lrolltoaddr(ta_p->ph_p,
		    ta_p->tdb_agent_data32.agent_lwpid,
		    (psaddr_t) ta_p->tdb_agent_data32.agent_go_addr,
		    (psaddr_t) ta_p->tdb_agent_data32.agent_stop_addr)
		    != PS_OK)
			return (TD_DBERR);
		if (ps_pdread(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar32.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != PS_OK)
			return (TD_DBERR);
	}
#endif /* _SYSCALL32_IMPL */
#if defined(AGENT_TEST)
	/*
	 * Write back an "opcode" of NONE_PENDING, to show that we've
	 * read the result status.
	 */
	agent_msg->opcode = NONE_PENDING;
	if (model == PR_MODEL_NATIVE) {
		if (ps_pdwrite(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != TD_OK)
			return (TD_DBERR);
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (ps_pdwrite(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar32.tdb_agent_ctl_addr,
		    (char *) agent_msg, sizeof (*agent_msg)) != TD_OK)
			return (TD_DBERR);
	}
#endif /* _SYSCALL32_IMPL */
#endif	/* defined(AGENT_TEST) */
	return (TD_OK);
}

/*
* Description:
*    Initialize model for libthread_db, taking into account
* whether the client has knowledge of the model function.
*
* Input:
*    ph_p - pointer to the proc handle
*    model - pointer to the model
*
* Output:
*    return value
*    The model gets filled in if successful.
*
* Side effects:
*    none.
*    Imported functions called: none
*/
td_err_e
__td_pdmodel(struct ps_prochandle *ph_p, int *model)
{
	ps_err_e	(*ps_pdmodel_p)(struct ps_prochandle *, int *);

	/*
	 * ps_pdmodel is weak, and might not be defined if this is
	 * an older client; so don't call ps_pdmodel if it is NULL.
	 */
	ps_pdmodel_p = ps_pdmodel;

	/*
	 * Older clients of libthread_db won't know of the existence
	 * of ps_pdmodel because its declared weak at the top
	 * of this file, so we assume their model is native.
	 */
	if (ps_pdmodel_p != NULL) {
		if ((*ps_pdmodel_p)(ph_p, model) != PS_OK) {
			return (TD_ERR);
		}
	} else
		*model = PR_MODEL_NATIVE;
	return (TD_OK);
}
