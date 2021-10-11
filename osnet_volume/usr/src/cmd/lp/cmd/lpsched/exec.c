/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <dial.h>

#include "limits.h"
#include "stdarg.h"
#include "wait.h"
#include "dial.h"
#include "lpsched.h"
#include <syslog.h>
#include <pwd.h>

#define Done(EC,ERRNO)	done(((EC) << 8),ERRNO)


static MESG *		ChildMd;

static int		ChildPid;
static int		WaitedChildPid;
static int		slot;
static int		do_undial;

static char		argbuf[ARG_MAX];

static long		key;

static void		sigtrap ( int );
static void		done ( int , int );
static void		cool_heels ( void );
static void		addenv ( char * , char * );
static void		trap_fault_signals ( void );
static void		ignore_fault_signals ( void );
static void		child_mallocfail ( void );
static void		Fork2 ( void );

static int		Fork1 ( EXEC * );

static void
relock(void)
{
	struct flock		l;

	l.l_type = F_WRLCK;
	l.l_whence = 1;
	l.l_start = 0;
	l.l_len = 0;
	(void)Fcntl (lock_fd, F_SETLK, &l);
	return;
}

static char *_exec_name(int type)
{
	static char *_names[] = {
	"", "EX_INTERF", "EX_SLOWF", "EX_ALERT", "EX_FALERT", "EX_PALERT",
	"EX_NOTIFY", "EX_FAULT_MESSAGE", "EX_FORM_MESSAGE", NULL };

	if ((type < 0) || (type > EX_FORM_MESSAGE))
		return ("BAD_EXEC_TYPE");
	else
		return (_names[type]);
}

/*
 * This function replaces characters in a string that might be used
 * to exploit a security hole.  Replace command seperators (`, &, ;, |, ^),
 * output redirection (>, |), variable expansion ($), and character
 * escape (\).
 *
 * Bugid 4141687
 * Add ( ) < * ? [
 * Bugid 4139071
 * Remove \
 */
void clean_string(char *ptr)
{
	char *cp;

	for (cp = ptr ; *cp != NULL ; cp++)
		if ((*cp == '`') || (*cp == '&') || (*cp == ';') ||
		    (*cp == '|') || (*cp == '>') || (*cp == '^') ||
		    (*cp == '$') || (*cp == '(') || (*cp == ')') ||
		    (*cp == '<') || (*cp == '*') || (*cp == '?') ||
		    (*cp == '['))
			*cp = '_';
}


static char time_buf[50];
/**
 ** exec() - FORK AND EXEC CHILD PROCESS
 **/

/*VARARGS1*/
int
exec(int type, ...)
{
	va_list			args;

	int			i;
	int			procuid;
	int			procgid;
	int			ret;
	int			fr_flg;

	char			*cp;
	char			*infile;
	char			*outfile;
	char			*errfile;
	char			*sep;

	char			**listp;
	char			**file_list;
	char			*printerName;
	char			*printerNameToShow;
	static char		nameBuf[100];
	char			*clean_title;

	PSTATUS			*printer;

	RSTATUS			*request;

	FSTATUS			*form;

	EXEC			*ep;

	PWSTATUS		*pwheel;
	time_t			now;
	struct passwd		*pwp;

	syslog(LOG_DEBUG, "exec(%s)", _exec_name(type));

	va_start (args, type);

	switch (type) {

	case EX_INTERF:
		printer = va_arg(args, PSTATUS *);
		if (printer->status & PS_REMOTE) {
			errno = EINVAL;
			return (-1);
		}
		request = printer->request;
		ep = printer->exec;
		break;
		
	case EX_FAULT_MESSAGE:
		printer = va_arg(args, PSTATUS *);
		request = va_arg(args, RSTATUS *);
		if (! ( printer->status & (PS_FORM_FAULT | PS_SHOW_FAULT))) {
			return(0);
		}
		ep = printer->fault_exec;
		printerName = (printer->printer && printer->printer->name 
				  ? printer->printer->name : "??");
			sprintf(nameBuf, "%s (on %s)\n", printerName,
				Local_System);

		printerNameToShow = nameBuf;

		(void) time(&now);
		cftime(time_buf, NULL,&now);
		break;

	case EX_SLOWF:
		request = va_arg(args, RSTATUS *);
		ep = request->exec;
		break;

	case EX_NOTIFY:
		request = va_arg(args, RSTATUS *);
		if (request->request->actions & ACT_NOTIFY) {
			errno = EINVAL;
			return (-1);
		}
		ep = request->exec;
		break;

	case EX_ALERT:
		printer = va_arg(args, PSTATUS *);
		if (!(printer->printer->fault_alert.shcmd)) {
			errno = EINVAL;
			return(-1);
		}
		ep = printer->alert->exec;
		break;

	case EX_PALERT:
		pwheel = va_arg(args, PWSTATUS *);
		ep = pwheel->alert->exec;
		break;

	case EX_FORM_MESSAGE:
		(void) time(&now);
		cftime(time_buf, NULL,&now);

		/*FALLTHRU*/
	case EX_FALERT:
		form = va_arg(args, FSTATUS *);
		ep = form->alert->exec;
		break;

	default:
		errno = EINVAL;
		return(-1);

	}
	va_end (args);

	if (!ep || (ep->pid > 0)) {
		errno = EBUSY;
		return(-1);
	}

	ep->flags = 0;

	key = ep->key = getkey();
	slot = ep - Exec_Table;

	switch ((ep->pid = Fork1(ep))) {

	case -1:
		relock ();
		return(-1);

	case 0:
		/*
		 * We want to be able to tell our parent how we died.
		 */
		lp_alloc_fail_handler = child_mallocfail;
		break;

	default:
		switch(type) {

		case EX_INTERF:
			request->request->outcome |= RS_PRINTING;
			break;

		case EX_NOTIFY:
			request->request->outcome |= RS_NOTIFYING;
			break;

		case EX_SLOWF:
			request->request->outcome |= RS_FILTERING;
			request->request->outcome &= ~RS_REFILTER;
			break;

		}
		return(0);

	}

	for (i = 0; i < NSIG; i++)
		(void)signal (i, SIG_DFL);
	(void)signal (SIGALRM, SIG_IGN);
	(void)signal (SIGTERM, sigtrap);
	
	for (i = 0; i < OpenMax; i++)
		if (i != ChildMd->writefd)
			Close (i);

	setpgrp();

	sprintf ((cp = BIGGEST_NUMBER_S), "%ld", key);
	addenv ("SPOOLER_KEY", cp);

#if	defined(DEBUG)
	addenv ("LPDEBUG", (debug? "1" : "0"));
#endif

	/*
	 * Open the standard input, standard output, and standard error.
	 */
	switch (type) {
		
	case EX_SLOWF:
	case EX_INTERF:
		/*
		 * stdin:  /dev/null
		 * stdout: /dev/null (EX_SLOWF), printer port (EX_INTERF)
		 * stderr: req#
		 */
		infile = 0;
		outfile = 0;
		errfile = makereqerr(request);
		break;

	case EX_NOTIFY:
		/*
		 * stdin:  req#
		 * stdout: /dev/null
		 * stderr: /dev/null
		 */
		infile = makereqerr(request);
		outfile = 0;
		errfile = 0;

		break;

	case EX_ALERT:
	case EX_FALERT:
	case EX_PALERT:
	case EX_FAULT_MESSAGE:
	case EX_FORM_MESSAGE:
		/*
		 * stdin:  /dev/null
		 * stdout: /dev/null
		 * stderr: /dev/null
		 */
		infile = 0;
		outfile = 0;
		errfile = 0;
		break;

	}

	if (infile) {
		if (Open(infile, O_RDONLY) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	} else {
		if (Open("/dev/null", O_RDONLY) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	}

	if (outfile) {
		if (Open(outfile, O_CREAT|O_TRUNC|O_WRONLY, 0600) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	} else {
		/*
		 * If EX_INTERF, this is still needed to cause the
		 * standard error channel to be #2.
		 */
		if (Open("/dev/null", O_WRONLY) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	}

	if (errfile) {
		if (Open(errfile, O_CREAT|O_TRUNC|O_WRONLY, 0600) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	} else {
		if (Open("/dev/null", O_WRONLY) == -1)
			Done (EXEC_EXIT_NOPEN, errno);
	}

	switch (type) {

	case EX_INTERF:
		/*
		 * Opening a ``port'' can be dangerous to our health:
		 *
		 *	- Hangups can occur if the line is dropped.
		 *	- The printer may send an interrupt.
		 *	- A FIFO may be closed, generating SIGPIPE.
		 *
		 * We catch these so we can complain nicely.
		 */
		trap_fault_signals ();

		(void)Close (1);

		if (strchr (request->secure->user, '!'))
		{
			procuid = Lp_Uid;
			procgid = Lp_Gid;
		}
		else
		{
			procuid = request->secure->uid;
			procgid = request->secure->gid;
		}
		if (printer->printer->dial_info)
		{
			ret = open_dialup(request->printer_type,
				printer->printer);
			if (ret == 0)
				do_undial = 1;
		}
		else
		{
			ret = open_direct(request->printer_type,
				printer->printer);
			do_undial = 0;
		}
		if (ret != 0)
			Done (ret, errno);
			
		if (!(request->request->outcome & RS_FILTERED))
			file_list = request->request->file_list;

		else {
			register int		count	= 0;
			register char *		num	= BIGGEST_REQID_S;
			register char *		prefix;

			prefix = makestr(
				Lp_Tmp,
				"/",
				(request->secure && request->secure->system ?
					request->secure->system : Local_System),
				"/F",
				getreqno(request->secure->req_id),
				"-",
				(char *)0
			);

			file_list = (char **)Malloc(
				(lenlist(request->request->file_list) + 1)
			      * sizeof(char *)
			);

			for (
				listp = request->request->file_list;
				*listp;
				listp++
			) {
				sprintf (num, "%d", count + 1);
				file_list[count] = makestr(
					prefix,
					num,
					(char *)0
				);
				count++;
			}
			file_list[count] = 0;
		}

		if (request->printer_type)
			addenv("TERM", request->printer_type);

		if (!(printer->printer->daisy)) {
			register char *	chset = 0;
			register char *	csp;

			if (
				request->form
			     && request->form->form->chset
			     && request->form->form->mandatory
			     && !STREQU(NAME_ANY, request->form->form->chset)
			)
				chset = request->form->form->chset;

			else if (
				request->request->charset
			     && !STREQU(NAME_ANY, request->request->charset)
			)
				chset = request->request->charset;

			if (chset) {
				csp = search_cslist(
					chset,
					printer->printer->char_sets
				);

				/*
				 * The "strtok()" below wrecks the string
				 * for future use, but this is a child
				 * process where it won't be needed again.
				 */
				addenv (
					"CHARSET",
					(csp? strtok(csp, "=") : chset)
				);
			}
		}

		if (request->fast)
			addenv("FILTER", request->fast);

		/*
		*/
		if (strcmp (request->secure->user, request->request->user))
		{
			addenv ("ALIAS_USERNAME", request->request->user);
		}
		/*
		 * Add the system name to the user name (ala system!user)
		 * unless it is already there. RFS users may have trouble
		 * here, sorry!
		 */
		cp = strchr(request->secure->user, '@');

		allTraysWithForm(printer, request->form); 

		/*
		 * Fix for 4137389
		 * Remove double quotes from title string.
		 */
		fr_flg = 1;
		clean_title = strdup(NB(request->request->title));
		if (clean_title == NULL) {
			/*
			 * strdup failed. We're probably hosed
			 * but try setting clean_title
			 * to original title and continuing.
			 */
			clean_title = NB(request->request->title);
			fr_flg = 0;
		} else if (strcmp(clean_title, "") != 0) {
			char *ct_p;

			for (ct_p = clean_title; *ct_p != NULL; ct_p++) {
				if (*ct_p == '"')
					*ct_p = ' ';
			}
		}

		sprintf (
			argbuf,
			"%s/%s %s %s%s%s \"%s\" %d  \"",
			Lp_A_Interfaces,
			printer->printer->name,
			request->secure->req_id,
			request->secure->user,
			(cp? "" : "@"),
			(cp? "" : request->secure->system),
			clean_title,
			request->copies
		);

		if (fr_flg)
			free (clean_title);

		sep = "";

		/*
		 * Do the administrator defined key=value pair options
		 */

		
		if (printer->printer->options) {
			char **tmp = printer->printer->options;
			while(*tmp != NULL) {
				strcat(argbuf, sep); sep = " ";
				strcat(argbuf, *tmp++);
			} 
		}

		/*
		 * Do the administrator defined ``stty'' stuff before
		 * the user's -o options, to allow the user to override.
		 */
		if (printer->printer->stty) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "stty='");
			strcat (argbuf, printer->printer->stty);
			strcat (argbuf, "'");
		}

		/*
		 * Do all of the user's options except the cpi/lpi/etc.
		 * stuff, which is done separately.
		 */
		if (request->request->options) {
			listp = dashos(request->request->options);
			while (*listp) {
				if (
					!STRNEQU(*listp, "cpi=", 4)
				     && !STRNEQU(*listp, "lpi=", 4)
				     && !STRNEQU(*listp, "width=", 6)
				     && !STRNEQU(*listp, "length=", 7)
				) {
					strcat (argbuf, sep);	sep = " ";
					strcat (argbuf, *listp);
				}
				listp++;
			}
		}

		/*
		 * The "pickfilter()" routine (from "validate()")
		 * stored the cpi/lpi/etc. stuff that should be
		 * used for this request. It chose form over user,
		 * and user over printer.
		 */
		if (request->cpi) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "cpi=");
			strcat (argbuf, request->cpi);
		}
		if (request->lpi) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "lpi=");
			strcat (argbuf, request->lpi);
		}
		if (request->pwid) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "width=");
			strcat (argbuf, request->pwid);
		}
		if (request->plen) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "length=");
			strcat (argbuf, request->plen);
		}

		/*
		 * Do the ``raw'' bit last, to ensure it gets
		 * done. If the user doesn't want this, then he or
		 * she can do the correct thing using -o stty=
		 * and leaving out the -r option.
		 */
		if (request->request->actions & ACT_RAW) {
			strcat (argbuf, sep);	sep = " ";
			strcat (argbuf, "stty=-opost");
		}

		strcat (argbuf, "\"");

		for (listp = file_list; *listp; listp++) {
			strcat (argbuf, " ");
			strcat (argbuf, *listp);
		}

		(void)chfiles (file_list, procuid, procgid);

		break;


	case EX_SLOWF:
		if (request->slow)
			addenv("FILTER", request->slow);

		if (strchr (request->secure->user, '!'))
		{
			procuid = Lp_Uid;
			procgid = Lp_Gid;
		}
		else
		{
			procuid = request->secure->uid;
			procgid = request->secure->gid;
		}
		cp = _alloc_files(
			lenlist(request->request->file_list),
			getreqno(request->secure->req_id),
			procuid,
			procgid,
			(request->secure && request->secure->system ?
				request->secure->system : NULL )
		);

		sprintf (
			argbuf,
			"%s %s/%s/%s %s",
			Lp_Slow_Filter,
			Lp_Tmp,
			(request->secure && request->secure->system ?
				request->secure->system : Local_System),
			cp,
			sprintlist(request->request->file_list)
		);

		(void)chfiles (request->request->file_list, procuid, procgid);
		break;

	case EX_ALERT:
		procuid = Lp_Uid;
		procgid = Lp_Gid;
		(void)Chown (printer->alert->msgfile, procuid, procgid);

		sprintf (
			argbuf,
			"%s/%s/%s %s",
			Lp_A_Printers,
			printer->printer->name,
			ALERTSHFILE,
			printer->alert->msgfile
			);
		break;

	case EX_PALERT:
		procuid = Lp_Uid;
		procgid = Lp_Gid;
		(void)Chown (pwheel->alert->msgfile, procuid, procgid);

		sprintf (
			argbuf,
			"%s/%s/%s %s",
			Lp_A_PrintWheels,
			pwheel->pwheel->name,
			ALERTSHFILE,
			pwheel->alert->msgfile
		);
		break;

	case EX_FALERT:
		procuid = Lp_Uid;
		procgid = Lp_Gid;
		(void)Chown (form->alert->msgfile, procuid, procgid);

		sprintf (
			argbuf,
			"%s/%s/%s %s",
			Lp_A_Forms,
			form->form->name,
			ALERTSHFILE,
			form->alert->msgfile
		);
		break;

	case EX_FORM_MESSAGE:
		procuid = Lp_Uid;
		procgid = Lp_Gid;

		sprintf (
			argbuf,
			"%s/form  '%s' '%s' '%s/%s/%s'",
			Lp_A_Faults,
			form->form->name,
			time_buf,
			Lp_A_Forms,form->form->name,FORMMESSAGEFILE);
		break;

	case EX_FAULT_MESSAGE:
		procuid = Lp_Uid;
		procgid = Lp_Gid;

		sprintf (
			argbuf,
			"%s/printer  '%s' '%s' '%s/%s/%s'",
			Lp_A_Faults,
			printerNameToShow,
			time_buf,
			Lp_A_Printers,printerName,FAULTMESSAGEFILE);
		break;

	case EX_NOTIFY:
		if (request->request->alert) {
			if (strchr(request->secure->user, '!')) {
				procuid = Lp_Uid;
				procgid = Lp_Gid;
			} else {
				procuid = request->secure->uid;
				procgid = request->secure->gid;
			}
			strcpy (argbuf, request->request->alert);

		} else {
			char *user = strdup(request->secure->user);
			procuid = Lp_Uid;
			procgid = Lp_Gid;
			clean_string(user);

			if ((request->request->actions & ACT_WRITE) &&
			    (!request->secure->system ||
			    STREQU(request->secure->system, Local_System)))
				sprintf(argbuf, "%s %s || %s %s",
					BINWRITE, user,
					BINMAIL, user
				);
			else
				sprintf( argbuf, "%s %s",
					BINMAIL, user
				);

			free(user);
		}
		break;
	}

	syslog(LOG_DEBUG, "requested command(sh -c %s)",
		(argbuf ? argbuf : "NULL"));

	Fork2 ();
	/* only the child returns */

	/*
	 * Correctly set up the supplemental group list
	 * for proper file access (before execl the interface program)
	 */

	pwp = getpwuid(procuid);
	if (pwp == NULL) {
		note("getpwuid(%d) call failed\n", procuid);
	} else if (initgroups(pwp->pw_name, procgid) < 0) {
		note("initgroups() call failed %d\n", errno);
	}
	
	setgid (procgid);
	setuid (procuid);

	/*
	 * The shell doesn't allow the "trap" builtin to set a trap
	 * for a signal ignored when the shell is started. Thus, don't
	 * turn off signals in the last child!
	 */

	/*
 	 * Strip characters tha could be interpreted by the shell, causing
	 * it or a subsequent script to run a command inadvertently.
	 * This is only necessary where user input could be used in the
	 * command.
	 */
	if (type == EX_INTERF)
		clean_string(argbuf);

	syslog(LOG_DEBUG, "execl(sh -c %s)", (argbuf ? argbuf : "NULL"));
	execl (SHELL, SHELL, "-c", argbuf, 0);
	Done (EXEC_EXIT_NEXEC, errno);
	/*NOTREACHED*/
}

/**
 ** addenv() - ADD A VARIABLE TO THE ENVIRONMENT
 **/

static void
addenv(char *name, char *value)
{
	register char *		cp;

	if ((cp = makestr(name, "=", value, (char *)0)))
		putenv (cp);
	return;
}

/**
 ** Fork1() - FORK FIRST CHILD, SET UP CONNECTION TO IT
 **/

static int
Fork1(EXEC *ep)
{
	int			pid;
	int			fds[2];

	if (pipe(fds) == -1) {
		note("Failed to create pipe for child process (%s).\n", PERROR);
		errno = EAGAIN ;
		return(-1);
	}

	ep->md = mconnect((char *)0, fds[0], fds[1]);

	switch (pid = fork()) {

	case -1:
		mdisconnect(ep->md);
		close(fds[0]);
		close(fds[1]);
		ep->md = 0;
		return (-1);
	
	case 0:
		ChildMd = mconnect(NULL, fds[1], fds[1]);
		return (0);

	default:
		mlistenadd(ep->md, POLLIN);
		return (pid);
	}
}

/**
 ** Fork2() - FORK SECOND CHILD AND WAIT FOR IT
 **/

static void
Fork2(void)
{
	switch ((ChildPid = fork())) {

	case -1:
		Done (EXEC_EXIT_NFORK, errno);
		/*NOTREACHED*/
					
	case 0:
		return;
					
	default:
		/*
		 * Delay calling "ignore_fault_signals()" as long
		 * as possible, to give the child a chance to exec
		 * the interface program and turn on traps.
		 */

		cool_heels ();
		/*NOTREACHED*/

	}
}


/**
 ** cool_heels() - WAIT FOR CHILD TO "DIE"
 **/

static void
cool_heels(void)
{
	int			status;

	/*
	 * At this point our only job is to wait for the child process.
	 * If we hang out for a bit longer, that's okay.
	 * By delaying before turning off the fault signals,
	 * we increase the chance that the child process has completed
	 * its exec and has turned on the fault traps. Nonetheless,
	 * we can't guarantee a zero chance of missing a fault.
	 * (We don't want to keep trapping the signals because the
	 * interface program is likely to have a better way to handle
	 * them; this process provides only rudimentary handling.)
	 *
	 * Note that on a very busy system, or with a very fast interface
	 * program, the tables could be turned: Our sleep below (coupled
	 * with a delay in the kernel scheduling us) may cause us to
	 * detect the fault instead of the interface program.
	 *
	 * What we need is a way to synchronize with the child process.
	 */
	sleep (1);
	ignore_fault_signals ();

	WaitedChildPid = 0;
	while ((WaitedChildPid = wait(&status)) != ChildPid)
		;
			
	if (
		EXITED(status) > EXEC_EXIT_USER
	     && EXITED(status) != EXEC_EXIT_FAULT
	)
		Done (EXEC_EXIT_EXIT, EXITED(status));

	done (status, 0);	/* Don't use Done() */
	/*NOTREACHED*/
}


/**
 ** trap_fault_signals() - TRAP SIGNALS THAT CAN OCCUR ON PRINTER FAULT
 ** ignore_fault_signals() - IGNORE SAME
 **/

static void
trap_fault_signals(void)
{
	signal (SIGHUP, sigtrap);
	signal (SIGINT, sigtrap);
	signal (SIGQUIT, sigtrap);
	signal (SIGPIPE, sigtrap);
	return;
}

static void
ignore_fault_signals(void)
{
	signal (SIGHUP, SIG_IGN);
	signal (SIGINT, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	signal (SIGPIPE, SIG_IGN);
	return;
}

/**
 ** sigtrap() - TRAP VARIOUS SIGNALS
 **/

static void
sigtrap(int sig)
{
	signal (sig, SIG_IGN);
	switch (sig) {

	case SIGHUP:
		Done (EXEC_EXIT_HUP, 0);
		/*NOTREACHED*/

	case SIGQUIT:
	case SIGINT:
		Done (EXEC_EXIT_INTR, 0);
		/*NOTREACHED*/

	case SIGPIPE:
		Done (EXEC_EXIT_PIPE, 0);
		/*NOTREACHED*/

	case SIGTERM:
		/*
		 * If we were killed with SIGTERM, it should have been
		 * via the Spooler who should have killed the entire
		 * process group. We have to wait for the children,
		 * since we're their parent, but WE MAY HAVE WAITED
		 * FOR THEM ALREADY (in cool_heels()).
		 */
		if (ChildPid != WaitedChildPid) {
			register int		cpid;

			while (
				(cpid = wait((int *)0)) != ChildPid
			     && (cpid != -1 || errno != ECHILD)
			)
				;
		}

		/*
		 * We can't rely on getting SIGTERM back in the wait()
		 * above, because, for instance, some shells trap SIGTERM
		 * and exit instead. Thus we force it.
		 */
		done (SIGTERM, 0);	/* Don't use Done() */
		/*NOTREACHED*/
	}
}

/**
 ** done() - TELL SPOOLER THIS CHILD IS DONE
 **/

static void
done(int status, int err)
{
	if (do_undial)
		undial (1);

	mputm (ChildMd, S_CHILD_DONE, key, slot, status, err);
	mdisconnect (ChildMd);

	exit (0);
	/*NOTREACHED*/
}

/**
 ** child_mallocfail()
 **/

static void
child_mallocfail(void)
{
	Done (EXEC_EXIT_NOMEM, ENOMEM);
}
