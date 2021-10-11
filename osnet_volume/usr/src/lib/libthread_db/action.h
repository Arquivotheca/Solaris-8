/* @(#)action.h 4.5 92/03/23 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef action_h
#define action_h

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {			/* matches 'action_map' */
    Action_NONE,

    Action_STOP,		/* stop looping in Proc_go() */
    Action_VSTOP,		/* Verbose Action_STOP */

    Action_PRINTSIG,
    Action_PRINTEXIT,
    Action_PRINTWHERE,
    Action_PRINTIN,
    Action_PRINTAT,

    Action_PRINT_TRACE,
    Action_PRINT_EXPR,
    Action_PRINT_CALL,
    Action_PRINT_RET,
    Action_PRINT_VARDELTA,

    Action_PRINT_TID,
    Action_PRINT_LID,

    Action_LINEDELTA,	/* if sline changed generate Event_SSTEP */
    Action_VARDELTA,	/* if variable value changed generate Event_? */

    Action_SKIPFUNC,	/* a'la the NEXT command */
    Action_SKIPSIG,

    Action_RESUME_STEP,	/* resume ISTEP or SSTEP specified by Action.h_step */
    Action_RAISE,	/* ... an event */

    Action_CALLBACK,	/* call back a function */
    Action_USERCMDS,	/* execute user cmds. For dbx WHEN command */

    Action_N		/* sentinel */
} Action_e;

/*
 * Type of function called as a result of Action_CALLBACK:
 */
typedef void Action_f(Proc, Handler, Event, void *client_data);

/*
 * Action creation
 */
Action	Action_new(Action_e);
Action	Action_new_expr(Action_e, Node);
Action	Action_new_usercmds(Action_e, UserActionCtx);
Action	Action_new_skipfunc(Action_e, Boolean next_like);
Action	Action_new_raise(Action_e type, Event_e event);
Action	Action_new_callback(Action_e, Action_f *, void *client_data);
Action	Action_new_resume_step(Action_e, Handler h_istep);
Action	Action_new_print_ret(Action_e, Symbol from);


/*
 * Hackish routines to force printing of status
 */
void    Action_printstatus(Proc p);
void    Action_printwhere(Proc p);

#ifdef __cplusplus
}
#endif

#endif	// action_h

