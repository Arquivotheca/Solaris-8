/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)alerts.c	1.10	98/08/05 SMI"	/* SVr4.0 1.11.1.4	*/

#include "lpsched.h"
#include "stdarg.h"

static char		*Fa_msg[] =
{
    "Subject: Mount form %s\n\nThe form %s needs to be mounted\non the printer(s):\n",
    "	%-14s (%d requests)\n",
    "Total print requests queued for this form: %d\n",
    "Use the %s ribbon.\n",
    "Use any ribbon.\n",
    "Use the %s print wheel, if appropriate.\n",
    "Use any print wheel, if appropriate.\n",
};

static char		*Fa_New_msg[] =
{
    "The form `%s' needs to be mounted\non the printer(s):\n",
    "The form `%s' (paper size: `%s') needs\nto be mounted on the printer(s):\n",
};

static char		*Pa_msg[] =
{
    "Subject: Mount print-wheel %s\n\nThe print-wheel %s needs to be mounted\non the printer(s):\n",
    "	%-14s (%d request(s))\n",
    "Total print requests queued for this print-wheel: %d\n",
};

static char		*Pf_msg[] =
{
    "Subject: Problem with printer %s\n\nThe printer %s has stopped printing for the reason given below.\n",
    "Fix the problem and bring the printer back on line\nto resume printing.\n",
    "Fix the problem and bring the printer back on line, and issue\nan enable command when you want to resume or restart printing.\n",
    "Fix the problem and bring the printer back on line.\nPrinting has stopped, but will be restarted in a few minutes;\nissue an enable command if you want to restart sooner.\nUnless someone issues a change request\n\n\tlp -i %s -P ...\n\nto change the page list to print, the current request will be reprinted from\nthe beginning.\n",
    "\nThe reason(s) it stopped (multiple reasons indicate repeated attempts):\n\n"
};

static void		pformat(),
			pwformat(),
			fformat();

static int		f_count(),
			p_count();

/*VARARGS1*/
void
alert (int type, ...)
{
    PSTATUS	*pr;
    RSTATUS	*rp;
    FSTATUS	*fp;
    PWSTATUS	*pp;
    char	*text;
    va_list	args;

    va_start (args, type);

    switch (type) {
	case A_PRINTER:
	    pr = va_arg(args, PSTATUS *);
	    rp = va_arg(args, RSTATUS *);
	    text = va_arg(args, char *);
	    pformat(pr->alert->msgfile, text, pr, rp);
	    if (!pr->alert->active)
	    {
		if (exec(EX_ALERT, pr) == 0)
			pr->alert->active = 1;
		else
		{
		    if (errno == EBUSY)
			pr->alert->exec->flags |= EXF_RESTART;
		    else
		        Unlink(pr->alert->msgfile);
		}
	    }
	    break;

	case A_PWHEEL:
	    pp = va_arg(args, PWSTATUS *);
	    pwformat(pp->alert->msgfile, pp);
	    if (!pp->alert->active) {
		if (exec(EX_PALERT, pp) == 0)
			pp->alert->active = 1;
		else {
		    if (errno == EBUSY)
			pp->alert->exec->flags |= EXF_RESTART;
		    else
			Unlink(pp->alert->msgfile);
		}
	    }
	    break;

	case A_FORM: {
		int isFormMessage;
		char *formPath;

		fp = va_arg(args, FSTATUS *);
		isFormMessage = (STREQU(fp->form->alert.shcmd, "showfault"));
		if (isFormMessage)
			formPath = makepath(Lp_A_Forms, fp->form->name,
				FORMMESSAGEFILE, (char * )NULL);
		else
			formPath = fp->alert->msgfile;
			
		fformat(formPath, fp,isFormMessage);
		 
		if (isFormMessage) {
			  Free(formPath);
			  schedule (EV_FORM_MESSAGE, fp);
		} else if (!fp->alert->active) {
			if (exec(EX_FALERT, fp) == 0)
				fp->alert->active = 1;
			else {
				if (errno == EBUSY)
					fp->alert->exec->flags |= EXF_RESTART;
				else
					Unlink(fp->alert->msgfile);
			}
		}
		break;
		}
    }
    va_end(args);
}

static void
pformat(char *file, char *text, PSTATUS *pr, RSTATUS *rp)
{
    int fd;

    if (Access(pr->alert->msgfile, 0) == 0) {
	if ((fd = open_locked(file, "a", MODE_READ)) < 0)
		return;
	if (text)
	    fdprintf(fd, text);
	close(fd);
    } else {
	if ((fd = open_locked(file, "w", MODE_READ)) < 0)
		return;
	fdprintf(fd, Pf_msg[0], NB(pr->printer->name), NB(pr->printer->name));
	if (STREQU(pr->printer->fault_rec, NAME_WAIT))
	    fdprintf(fd, Pf_msg[2]);
	else {
	    if (pr->exec->pid > 0)
		fdprintf(fd, Pf_msg[1]);
	    else if (rp)
		fdprintf(fd, Pf_msg[3], rp->secure->req_id);
	}
	fdprintf(fd, Pf_msg[4]);
	if (text)
	{
		while (*text == '\n' || *text == '\r')
		    text++;
		fdprintf(fd, "%s", text);
	}
	close(fd);
    }
}

static void
pwformat(char *file, PWSTATUS *pp)
{
	int fd;
	PSTATUS	*p;

	if ((fd = open_locked(file, "w", MODE_READ)) < 0)
	    return;
	fdprintf(fd, Pa_msg[0], NB(pp->pwheel->name), NB(pp->pwheel->name));
	for (p = walk_ptable(1); p; p = walk_ptable(0))
	    if (
		p->printer->daisy
	     && !SAME(p->pwheel_name, pp->pwheel->name)
	     && searchlist(pp->pwheel->name, p->printer->char_sets)
	    )
	    {
		register int		n = p_count(pp, p->printer->name);

		if (n)
		  fdprintf(fd, Pa_msg[1], p->printer->name, n);
	    }
	fdprintf(fd, Pa_msg[2], pp->requests);
	close(fd);
	pp->requests_last = pp->requests;
}

static void
fformat(char *file, FSTATUS *fp, int isFormMessage)
{
    int fd;
    PSTATUS	*p;
    int		numLines=0;

	if ((fd = open_locked(file, "w", MODE_READ)) < 0)
	    return;

	if (isFormMessage)
		if (fp->form->paper)
			fdprintf(fd, Fa_New_msg[1], NB(fp->form->name),
				fp->form->paper);
		else
			fdprintf(fd, Fa_New_msg[0], NB(fp->form->name));
	else
		fdprintf(fd, Fa_msg[0], NB(fp->form->name), NB(fp->form->name));

	for (p = walk_ptable(1); p; p = walk_ptable(0))
		if ((! isFormMountedOnPrinter(p,fp)) &&
		    allowed(fp->form->name, p->forms_allowed,
		    p->forms_denied)) {

			register int n = f_count(fp, p->printer->name);

			if (n) {
				fdprintf(fd, Fa_msg[1], p->printer->name, n);
				numLines++;
			}
		}

	if (numLines != 1) fdprintf(fd, Fa_msg[2], fp->requests);
	if (!isFormMessage) {
		if (fp->form->rcolor && !STREQU(fp->form->rcolor, NAME_ANY))
			 fdprintf(fd, Fa_msg[3], NB(fp->form->rcolor));
		else
			 fdprintf(fd, Fa_msg[4]);

		if (fp->form->chset && !STREQU(fp->form->chset, NAME_ANY))
			 fdprintf(fd, Fa_msg[5], NB(fp->form->chset));
		else
			 fdprintf(fd, Fa_msg[6]);
	}

	close(fd);
	fp->requests_last = fp->requests;
}


/* VARARGS1 */
void
cancel_alert(int type, ...)
{
    ALERT	*ap;
    va_list	args;

    va_start (args, type);

    switch (type)
    {
	case A_PRINTER:
	    ap = va_arg(args, PSTATUS *)->alert;
	    break;

	case A_PWHEEL:
	    ap = va_arg(args, PWSTATUS *)->alert;
	    break;

	case A_FORM:
	    ap = va_arg(args, FSTATUS *)->alert;
	    break;

	default:
	    return;
    }
    va_end(args);

    ap->active = 0;
    terminate(ap->exec);
    Unlink(ap->msgfile);
    return;
}

static int
dest_equivalent_printer(char *dest, char *printer)
{
	CSTATUS *		pc;

	return (
		STREQU(dest, printer)
	     || STREQU(dest, NAME_ANY)
	     || (
			((pc = search_ctable(dest)) != NULL)
		     && searchlist(printer, pc->class->members)
		)
	);
}

static int
f_count(fp, name)
register FSTATUS 	*fp;
register char		*name;
{
    register int		count = 0;
    register RSTATUS		*rp;

    BEGIN_WALK_BY_FORM_LOOP(rp, fp)
	if (dest_equivalent_printer(rp->request->destination, name))
	    count++;
    END_WALK_LOOP
    if (
	NewRequest
     && NewRequest->form == fp
     && dest_equivalent_printer(NewRequest->request->destination, name)
    )
	count++;
    return(count);
}

static int
p_count(pp, name)
register PWSTATUS 	*pp;
register char		*name;
{
    register int		count = 0;
    register RSTATUS		*rp;

    BEGIN_WALK_LOOP(rp, rp->pwheel == pp)
	if (dest_equivalent_printer(rp->request->destination, name))
	    count++;
    END_WALK_LOOP
    if (
	NewRequest
     && NewRequest->pwheel == pp
     && dest_equivalent_printer(NewRequest->request->destination, name)
    )
	count++;
    return(count);
}
