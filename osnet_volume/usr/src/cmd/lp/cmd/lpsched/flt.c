/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)flt.c	1.4	96/04/10 SMI"	/* SVr4.0 1.4.1.4	*/

# include	<stdarg.h>
# include	"lpsched.h"

typedef struct fault	FLT;

struct fault
{
    FLT *	next;
    int		type;
    int		i1;
    char *	s1;
    RSTATUS *	r1;
    MESG *	ident;
};

static void free_flt ( FLT * );
static void do_flt_acts ( MESG * );

static FLT	Fault_Head = { NULL, 0, 0, NULL, NULL, NULL };
static FLT *	Fault_List = &Fault_Head;

void
add_flt_act(MESG * md, ...)
{
    va_list	arg;
    FLT		*f;

    va_start (arg, md);

    f = (FLT *)Malloc(sizeof(FLT));
    
    (void) memset((char *)f, 0, sizeof(FLT));
    
    f->type = (int)va_arg(arg, int);
    f->ident = md;
    
    if (md->on_discon == NULL)
	if (mon_discon(md, do_flt_acts))
	    mallocfail();

    switch(f->type)
    {
	case FLT_FILES:
	f->s1 = Strdup((char *)va_arg(arg, char *));
	f->i1 = (int)va_arg(arg, int);
	break;
	
	case FLT_CHANGE:
	f->r1 = (RSTATUS *)va_arg(arg, RSTATUS *);
	break;
    }

    va_end(arg);

    f->next = Fault_List->next;
    Fault_List->next = f;
}


void
del_flt_act(MESG *md, ...)
{
    va_list	arg;
    int		type;
    FLT		*fp;
    FLT		*f;

    va_start(arg, md);

    type = (int)va_arg(arg, int);
    
    for (f = Fault_List; f->next; f = f->next)
	if (f->next->type == type && f->next->ident == md)
	{
	    fp = f->next;
	    f->next = f->next->next;
	    free_flt(fp);
	    break;
	}

    va_end(arg);
}

static void
do_flt_acts(MESG *md)
{
    FLT		*f;
    FLT		*fp;
    char	*file;
    char	id[15];
    
    for (f = Fault_List; f && f->next; f = f->next)
	if (f->next->ident == md)
	{
	    fp = f->next;
	    f->next = f->next->next;

	    switch (fp->type)
	    {
		case FLT_FILES:
		/* remove files created with alloc_files */
		while(fp->i1--)
		{
		    (void) sprintf(id, "%s-%d", fp->s1, fp->i1);
		    file = makepath(Lp_Temp, id, (char *)0);
		    (void) Unlink(file);
		    Free(file);
		}
		break;
		

		case FLT_CHANGE:
		/* clear RS_CHANGE bit, write request file, and schedule */
		fp->r1->request->outcome &= ~RS_CHANGING;
		putrequest(fp->r1->req_file, fp->r1->request);
		if (NEEDS_FILTERING(fp->r1))
		    schedule(/* LP_FILTER */ EV_SLOWF, fp->r1);
		else
		    schedule(/* LP_PRINTER */ EV_INTERF, fp->r1->printer);
		break;
	    }
	    free_flt(fp);
	}
}

static void
free_flt(FLT *f)
{
    if (f->s1)
	Free(f->s1);
    Free((char *)f);
}
