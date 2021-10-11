/* @(#)event.h 1.34 92/03/06 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef event_h
#define event_h

/*
 * Detailed descriptions of events` meanings appears in "./event.doc"
 */

typedef enum {
    Event_NONE,		/* used as 0th index. keep as first enum */

    Event_EXIT,		/* the four variations on 'wait()' status */
    Event_CONT,
    Event_TERMSIG,
    Event_STOP,

    Event_BPT,		/* generic breakpoint */
    Event_ISTEP,	/* single step */
    Event_SYNC,		/* the initial fork/exec/wait sync */
    Event_SYNC_RTLD,	/* rtld sync */
    Event_ATTACH,	/* attach operation complete */
    Event_STOPSIG,	/* signal interception */

    Event_CALL,		/* function call detected during ISTEPing */

    Event_SSTEP,	/* source step */
    Event_VARDELTA,	/* traced variable value changed */

    Event_USTOP,	/* a stoppage which returns control to the prompt */
    Event_VSTOP,	/* verbose USTOP */

    Event_FSTOP,	/* stop on every function */

    Event_LASTRITES,	/* fired right before death */

    Event_SYS_IN,	/* sys call entry */
    Event_SYS_OUT,	/* sys call exit */

    Event_RTLD_OPEN,	/* dlopen complete */
    Event_RTLD_CLOSE,	/* dlclose complete */

    Event_VCPU_DEATH,	/* A VCPU/LWP exited */

    Event_ALL_METHODS,	/* A breakpoint for all class methods */
    Event_ALL_VARIABLES,/* A trace for all class variables */

    Event_RETFROM,	/* return from some function */
    Event_N		/* sentinel */
} Event_e;

struct Event_t {
    /* List_HDR(Event); */
    Event_e	type;

    VCpu 	vcpu;
    Thread	thread;

    /* parameter values */
    Address	pc;		/* BPT */
    int		sig;		/* TERMSIG, STOPSIG */
    int		sigcode;
    int		exitcode;	/* EXIT */
    Boolean	initially;	/* VARDELTA */
    struct VarDeltaCtx *
		var;
    int 	syscode;	/* SYS_IN, SYS_OUT */
    Level_e	level;		/* VSTOP */
    unsigned int number;	/* ALL_METHODS, ALL_VARIABLES */
};

/*
 * NULL event is useful in some case
 */

extern struct Event_t event_null;


/*
 * Event creation and various specialized variatiosn ...
 */
Event	Event_new(Event_e);

Event	Event_new_vardelta(Event_e, Boolean initially, struct VarDeltaCtx *var);
Event	Event_new_bpt(Event_e, Address pc);
Event	Event_new_sig(Event_e, int sig, int sigcode);
Event	Event_new_all_methods(Event_e type, unsigned int counter);
Event	Event_new_exit(Event_e, int exitcode);
Event	Event_new_sys(Event_e, int syscode);
Event	Event_new_vstop(Event_e, Level_e);


void	Event_delete(Event);

char *	Event_name(Event_e);
Boolean	Event_parametric(Event_e);



/*
 * Event's can be queue'd up
 */

typedef struct struct_List *EventQ;

EventQ	EventQ_new(void);
void	EventQ_flush(EventQ);
void	EventQ_delete(EventQ);

Boolean	EventQ_contains(EventQ, Event_e);
int	EventQ_size(EventQ);

void	EventQ_enqueue(EventQ eq, VCpu vcpu, Thread thread, Event e);
Event	EventQ_dequeue(EventQ eq);
Boolean	EventQ_cancel(EventQ eq, Event_e type);

#endif
