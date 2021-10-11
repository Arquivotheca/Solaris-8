/* @(#)handler.h 1.33 91/07/10 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef handler_h
#define handler_h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The "holes" in the bit assignmnet represent private flags defined
 * in "handler_priv.h"
 */

#define Handler_SELFDESTRUCT	(1<<0)	/* ... upon trigerring */
#define Handler_INTERNAL	(1<<1)	/* won't show up on STATUS */
#define Handler_COUNT		(1<<2)	/* count and fire only when expired */
#define Handler_HELPER		(1<<3)	/* not a "primary" handler */
#define Handler_INSTR	       	(1<<5)	/* instruction level handler */


#define ANYADDR ((Address) -2)		/* see Handler_new_bpt() */
#define ANYSIG 0			/* analogous to ANYADDR */

#define Handler_MAX_HELPER 5		/* max number of helpers per handler */


/*
 * Handler creation and various specialized attribute setting routines:
 */
Handler	Handler_new(Event_e, unsigned flags, ...);
Handler	Handler_new_bpt(Event_e, unsigned flags, Address pc, Boolean user);
Handler Handler_new_all_methods(Event_e type,
				unsigned flags, unsigned int counter);

Handler Handler_new_fret(Address);
Handler Handler_new_step(Event_e, unsigned flags, Boolean next_like);
Handler	Handler_new_vardelta(Event_e, unsigned flags, Node expr);
Handler	Handler_new_lastrites(Event_e, unsigned flags);

void	Handler_add_action(Handler, Action);
void	Handler_add_helper(Handler, Handler helper);

void	Handler_set_origin_str(Handler, char *);
void	Handler_set_origin_tree(Handler, Node);
void	Handler_set_origin_tree_internal(Handler, Node);
void	Handler_set_count(Handler, int);
void	Handler_set_cond(Handler, Node cond);
void	Handler_set_vcpu(Handler, VCpu);
void	Handler_set_thread(Handler, Thread);


/*
 * Listing of Handlers for internal debugging purposes. Listing levels:
 * 0: user
 * 1: internal but signals (signals are very volumnious)
 * 2: all
 */
void	Handler_db_print(int id);
void	Handler_db_print_all(int level);

/*
 * Listing of Handlers for purpose of STATUS/CATCH/IGNORE
 */
void	Handler_print(Handler);
void	Handler_print_all(void);
void	Handler_print_sigs(Boolean v);


void	Handler_delete(Handler);
void 	Handler_delete_byid(int id);
void 	Handler_delete_bypc(Address pc);
void	Handler_delete_by_vcpu(VCpu);
void	Handler_delete_by_LO(LoadObj lo);
void 	Handler_delete_all(void);

void	Handler_toggle(Handler, Boolean activate);
void	Handler_toggle_all(Boolean activate);
void	Handler_sigvec(int sig, Boolean v);

void	Handler_sync(void);	/* upon Event_SYNC */
void	Handler_delay_all(void);

Boolean	Handler_need_istep(void);
Boolean Handler_anybps(void);

Boolean	Handler_dispatch(Proc);


void	Handler_defaults_once(Boolean verbose);	/* per dbx invokation */
void	Handler_defaults_perrun(Proc);		/* per RUN cmd */

void    tool_breakpoint_all(Boolean set);

#ifdef __cplusplus
}
#endif

#endif	// handler_h
