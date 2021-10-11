/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)disp1.c	1.44	98/08/05 SMI"	/* SVr4.0 1.11.1.11	*/

#include "dispatch.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

static char	*reqpath ( char *, char ** );


RSTATUS			*NewRequest;

/**
 ** s_alloc_files()
 **/

void s_alloc_files ( char * m, MESG * md )	/* funcdef */
{
    char			*file_prefix;
    ushort			count;
    mode_t			old_msk;


    /*
     * Bugid 4140311
     * Set umask to 0 before creating files.
     */
    old_msk = umask((mode_t) 0);

    getmessage (m, S_ALLOC_FILES, &count);
    syslog(LOG_DEBUG, "s_alloc_files(%d)", count);

    if ((file_prefix = _alloc_files(count, (char *)0, md->uid, md->gid, NULL)))
    {
	mputm (md, R_ALLOC_FILES, MOK, file_prefix);
	add_flt_act(md, FLT_FILES, file_prefix, count);
    }
    else if (errno == EEXIST)
	mputm (md, R_ALLOC_FILES, MERRDEST, "");
    else
	mputm (md, R_ALLOC_FILES, MNOMEM, "");

    (void) umask(old_msk);

    return;
}

/**
 ** s_print_request()
 **/

void s_print_request ( char * m, MESG * md )
{
    extern char			*Local_System;
    char			*file;
    char			*idno;
    char			*path;
    char			*req_file;
    char			*req_id	= 0;
    RSTATUS			*rp;
    REQUEST			*r;
    SECURE			*s;
    struct passwd		*pw;
    short			err;
    short			status;
    off_t			size;
    uid_t			org_uid;
    gid_t			org_gid;


    (void) getmessage (m, S_PRINT_REQUEST, &file);
    syslog(LOG_DEBUG, "s_print_request(%s)", (file ? file : "NULL"));

    /*
     * "NewRequest" points to a request that's not yet in the
     * request list but is to be considered with the rest of the
     * requests (e.g. calculating # of requests awaiting a form).
     */
    if ((rp = NewRequest = allocr()) == NULL)
	status = MNOMEM;

    else
    {
	req_file = reqpath(file, &idno);
	path = makepath(Lp_Tmp, req_file, (char *)0);
	(void) Chmod(path, 0600);
	(void) Chown(path, Lp_Uid, Lp_Gid);
	Free (path);
    
	if (!(r = Getrequest(req_file)))
	    status = MNOOPEN;

	else
	{
	    *(rp->request) = *r;
	    rp->req_file = Strdup(req_file);

	    /*
	    **	Test for the presence of a secure file.
	    **	If found skip sanity checks.
	    **  The secure file will only exist if the request
	    **  originated on a different system.  Since the
	    **  request has not been validated on this side yet
	    **  we remove the secure file until it is.
	    **
	    */
	    if ((s = Getsecure(req_file)))
	    {
		(void)  rmsecure (req_file);
		rp->request->outcome = 0;
		*(rp->secure) = *s;
		rp->secure->req_id = Strdup(s->req_id);
		rp->secure->user = Strdup(s->user);
		rp->secure->system = Strdup(s->system);
		freesecure(s);
		/*
		**  There are some anomallies associated w/
		**  '-1', '-2', etc. files received from other systems
		**  so even though the uid and gid will be 'lp'
		**  the mode may be incorrect.  'chfiles()' will
		**  fix this for us.
		*/
		(void)	chfiles (rp->request->file_list, Lp_Uid, Lp_Gid);
	    }
	    else
	    {
		rp->request->outcome = 0;
		rp->secure->uid = md->uid;
		rp->secure->gid = md->gid;
    
		pw = getpwuid(md->uid);
		endpwent();
		if (pw && pw->pw_name && *pw->pw_name)
		    rp->secure->user = Strdup(pw->pw_name);
		else
		{
		    rp->secure->user = Strdup(BIGGEST_NUMBER_S);
		    (void) sprintf (rp->secure->user, "%ld", md->uid);
		}
	    
		if ((rp->request->actions & ACT_SPECIAL) == ACT_HOLD)
		    rp->request->outcome |= RS_HELD;
		if ((rp->request->actions & ACT_SPECIAL) == ACT_RESUME)
		    rp->request->outcome &= ~RS_HELD;
		if((rp->request->actions & ACT_SPECIAL) == ACT_IMMEDIATE)
		{
		    if (!md->admin)
		    {
			status = MNOPERM;
			goto Return;
		    }
		    rp->request->outcome |= RS_IMMEDIATE;
		}

		size = chfiles(rp->request->file_list, Lp_Uid, Lp_Gid);

		if (size < 0)
		{

		/* at this point, chfiles() may have failed because the
		 * the file may live on an NFS mounted filesystem, under
		 * a directory of mode 700. such a directory isn't 
		 * accessible even by root, according to the NFS protocol
		 * (i.e. the Stat() in chfiles() failed). this most commonly 
		 * happens via the automounter, and rlogin.
		 *
		 * thus we change our euid/egid to that of the user, and
		 * try again. if *this* fails, then the file must really
		 * be inaccessible.
		 */
		    org_uid = geteuid();
		    org_gid = getegid();

		    if (setegid(md->gid) != 0) {
			    status = MUNKNOWN;
			    goto Return;
		    }

		    if (seteuid(md->uid) != 0) {
			    setgid(org_gid);
			    status = MUNKNOWN;
			    goto Return;
		    }

		    size = chfiles(rp->request->file_list, Lp_Uid, Lp_Gid);

		    if (seteuid(org_uid) != 0) {
			/* should never happen */
			note("s_print_request(): ");
			note("seteuid back to uid=%d failed!!\n", org_uid);
			size = -1;
		    } 

		    if (setegid(org_gid) != 0) {
			/* should never happen */
			note("s_print_request(): ");
			note("setegid back to uid=%d failed!!\n", org_uid);
			size = -1;
		    } 

		    if (size < 0) {
			    status = MUNKNOWN;
			    goto Return;
		    }
		}
		if (!(rp->request->outcome & RS_HELD) && size == 0)
		{
		    status = MNOPERM;
		    goto Return;
		}
		rp->secure->size = size;

		(void) time(&rp->secure->date);
		rp->secure->req_id = NULL;
		rp->secure->system = Strdup(Local_System);
	    }

	   if (!rp->request->title) {
		if (strlen(*rp->request->file_list) < (size_t)24)
			rp->request->title = Strdup(*rp->request->file_list);
		else {
			char *r;
		
			if (r = strrchr(*rp->request->file_list, '/'))
				r++;
			else
				r = *rp->request->file_list ;

			rp->request->title = malloc(25);
			sprintf(rp->request->title, "%-.24s", r);	
		}
	   }

	    if((err = validate_request(rp, &req_id, 0)) != MOK)
		status = err;
	    else {
		/*
		 * "req_id" will be supplied if this is from a
		 * remote system.
		 */
		if (rp->secure->req_id == NULL)
		{
		    req_id = makestr(req_id, "-", idno, (char *)0);
		    rp->secure->req_id = req_id;
		} else
		    req_id = rp->secure->req_id;
	
		/* fix for bugid 1103890. use Putsecure instead. */
		if (
		    Putsecure(req_file, rp->secure) == -1
		 || putrequest(req_file, rp->request) == -1
		)
		    status = MNOMEM;

		else
		{
		    status = MOK;

		    insertr(rp);
		    NewRequest = 0;
    
		    if (rp->slow)
			schedule (EV_SLOWF, rp);
		    else
			schedule (EV_INTERF, rp->printer);

		    del_flt_act(md, FLT_FILES);
		}
	    }
	}
    }

Return:
    NewRequest = 0;
    Free(req_file);
    Free(idno);
    if (status != MOK && rp) {
	rmfiles(rp, 0);
	freerstatus(rp);
    }
    mputm(md, R_PRINT_REQUEST, status, NB(req_id), chkprinter_result);
    return;
}

/**
 ** s_start_change_request()
 **/

void s_start_change_request (char *m, MESG *md)
{
    char		*req_id;
    char		*req_file	= "";
    short		status;
    RSTATUS		*rp;
    char		*path;
    
    (void) getmessage(m, S_START_CHANGE_REQUEST, &req_id);
    syslog(LOG_DEBUG, "s_start_change_request(%s)",
	   (req_id ? req_id : "NULL"));

    
    if (!(rp = request_by_id(req_id)))
	status = MUNKNOWN;
    else if (rp->request->outcome & RS_DONE)
	status = M2LATE;
    else if (!md->admin && md->uid != rp->secure->uid)
	status = MNOPERM;
    else if (rp->request->outcome & RS_CHANGING)
	status = MNOOPEN;
    else if (rp->request->outcome & RS_NOTIFYING)
	status = MBUSY;
    else {
	status = MOK;

	if (
	    rp->request->outcome & RS_FILTERING
	 && !(rp->request->outcome & RS_STOPPED)
	)
	{
	    rp->request->outcome |= (RS_REFILTER|RS_STOPPED);
	    terminate (rp->exec);
	}

	if (
	    rp->request->outcome & RS_PRINTING
	 && !(rp->request->outcome & RS_STOPPED)
	)
	{
	    rp->request->outcome |= RS_STOPPED;
	    terminate (rp->printer->exec);
	}

	rp->request->outcome |= RS_CHANGING;	
    
	/*
	 * Change the ownership of the request file to be "md->uid".
	 * Either this is identical to "rp->secure->uid", or it is
	 * "Lp_Uid" or it is root. The idea is that the
	 * person at the other end needs access, and that may not
	 * be who queued the request.
	 */
	path = makepath(Lp_Tmp, rp->req_file, (char *)0);
	(void) Chown(path, md->uid, rp->secure->gid);
	Free (path);

	add_flt_act(md, FLT_CHANGE, rp);
	req_file = rp->req_file;

    }
    mputm(md, R_START_CHANGE_REQUEST, status, req_file);
    return;
}

/**
 ** s_end_change_request()
 **/

void s_end_change_request(char *m, MESG *md)
{
    char		*req_id;
    RSTATUS		*rp;
    off_t		size;
    off_t		osize;
    short		err;
    short		status;
    REQUEST		*r		= 0;
    REQUEST		oldr;
    int			call_schedule	= 0;
    int			move_ok		= 0;
    char		*path;
    
    (void) getmessage(m, S_END_CHANGE_REQUEST, &req_id);
    syslog(LOG_DEBUG, "s_end_change_request(%s)", (req_id ? req_id : "NULL"));

    if (!(rp = request_by_id(req_id)))
	status = MUNKNOWN;
    else if (!(rp->request->outcome & RS_CHANGING))
	status = MNOSTART;
    else {
	path = makepath(Lp_Tmp, rp->req_file, (char *)0);
	(void) Chmod(path, 0600);
	(void) Chown(path, Lp_Uid, Lp_Gid);
	Free (path);

	rp->request->outcome &= ~(RS_CHANGING);
	del_flt_act(md, FLT_CHANGE);
	/*
	 * The RS_CHANGING bit may have been the only thing preventing
	 * this request from filtering or printing, so regardless of what
	 * happens below, we must check to see if the request can proceed.
	 */
	call_schedule = 1;

	if (!(r = Getrequest(rp->req_file)))
	    status = MNOOPEN;
	else {
	    oldr = *(rp->request);
	    *(rp->request) = *r;

	    move_ok = STREQU(oldr.destination, r->destination);

	    /*
	     * Preserve the current request status!
	     */
	    rp->request->outcome = oldr.outcome;

	    /*
	     * Here's an example of the dangers one meets when public
	     * flags are used for private purposes. ".actions" (indeed,
	     * anything in the REQUEST structure) is set by the person
	     * changing the job. However, lpsched uses ".actions" as
	     * place to indicate that a job came from a remote system
	     * and we must send back job completion--this is a strictly
	     * private flag that we must preserve.
	     */
	    rp->request->actions |= (oldr.actions & ACT_NOTIFY);

	    if ((rp->request->actions & ACT_SPECIAL) == ACT_HOLD) {
		rp->request->outcome |= RS_HELD;
		/*
		 * To be here means either the user owns the request
		 * or he or she is the administrator. Since we don't
		 * want to set the RS_ADMINHELD flag if the user is
		 * the administrator, the following compare will work.
		 */
		if (md->uid != rp->secure->uid)
		    rp->request->outcome |= RS_ADMINHELD;
	    }

	    if ((rp->request->actions & ACT_SPECIAL) == ACT_RESUME) {
		if ((rp->request->outcome & RS_ADMINHELD) && !md->admin) {
		    status = MNOPERM;
		    goto Return;
		}
		rp->request->outcome &= ~(RS_ADMINHELD|RS_HELD);
	    }

	    if((rp->request->actions & ACT_SPECIAL) == ACT_IMMEDIATE) {
		if (!md->admin)
		{
		    status = MNOPERM;
		    goto Return;
		}
		rp->request->outcome |= RS_IMMEDIATE;
	    }

	    size = chfiles(rp->request->file_list, Lp_Uid, Lp_Gid);
	    if (size < 0) {
		status = MUNKNOWN;
		goto Return;
	    }
	    if (!(rp->request->outcome & RS_HELD) && size == 0) {
		status = MNOPERM;
		goto Return;
	    }

	    osize = rp->secure->size;
	    rp->secure->size = size;

	    if ((err = validate_request(rp, (char **)0, move_ok)) != MOK) {
		status = err;
		rp->secure->size = osize;
	    } else {
		status = MOK;

		if ((rp->request->outcome & RS_IMMEDIATE) ||
		    (rp->request->priority != oldr.priority)) {
		    remover(rp);
		    insertr(rp);
		}

		(void) time(&rp->secure->date);

		freerequest(&oldr);
		(void) putrequest(rp->req_file, rp->request);
		/* fix for bugid 1103890. use Putsecure instead.       */
		(void) Putsecure(rp->req_file, rp->secure);
	    }
	}
    }

Return:
    if (status != MOK && rp) {
	if (r) {
	    freerequest(r);
	    *(rp->request) = oldr;
	}
	if (status != MNOSTART)
	    (void) putrequest(rp->req_file, rp->request);
    }

    if (call_schedule)
	maybe_schedule(rp);

    mputm(md, R_END_CHANGE_REQUEST, status, chkprinter_result);
    return;
}

/**
 ** _cancel()
 **	user may be (host!user)
 **/

static char *
_cancel(MESG *md, char *dest, char *user, char *req_id)
{
    static RSTATUS	*rp;
    static char		*s_dest;
    static char		*s_user;
    static char		*s_req_id;
    static int		current;
    RSTATUS		*crp;
    char		*creq_id;

    syslog(LOG_DEBUG, "_cancel(%s, %s, %s)", (dest ? dest : "NULL"),
	   (user ? user : "NULL"), (req_id ? req_id : "NULL"));

    if (dest || user || req_id) {
	s_dest = dest;
        if (STREQU(user, "!"))
		s_user = strdup("all!all");
	else
		s_user = user;
	s_req_id = req_id;
	rp = Request_List;
	current = 0;
	if (STREQU(s_req_id, CURRENT_REQ)) {
	    current = 1;
	    s_req_id = NULL;
	}
    }

    while (rp != NULL) {
	crp = rp;
	rp = rp->next;
	
	if (*s_dest && !STREQU(s_dest, crp->request->destination))
	    continue;

	if (current && !(crp->request->outcome & RS_PRINTING))
	    continue;

	if (s_req_id && *s_req_id && !STREQU(s_req_id, crp->secure->req_id))
	    continue;

	if (*s_user && !bangequ(s_user, crp->secure->user))
	    continue;

	if (!md->admin && md->uid != crp->secure->uid) {
	    errno = MNOPERM;
	    return(Strdup(crp->secure->req_id));
	}

	crp->reason = MOK;
	creq_id = Strdup(crp->secure->req_id);

	syslog(LOG_DEBUG, "cancel reqid (%s) uid: %d, secureuid: %d\n",
		creq_id, md->uid, crp->secure->uid);

	if (cancel(crp, (md->uid != crp->secure->uid)))
	    errno = MOK;
	else
	    errno = M2LATE;
	return(creq_id);
    }

    errno = MUNKNOWN;
    return(NULL);
}

/**
 ** s_cancel_request()
 **/

void s_cancel_request(char *m, MESG *md)
{
	char	*req_id, *rid;
	short	status;

	(void) getmessage(m, S_CANCEL_REQUEST, &req_id);
	syslog(LOG_DEBUG, "s_cancel_request(%s)", (req_id ? req_id : "NULL"));

	if ((rid = _cancel(md, "", "", req_id)) != NULL)
		Free(rid);
	status = (short)errno;
	
	mputm(md, R_CANCEL_REQUEST, status);
}

/**
 ** s_cancel()
 **/

void s_cancel(char *m, MESG *md)
{
    char	*req_id;
    char	*user;
    char	*destination;
    char	*rid;
    char	*nrid;
    int		nerrno;
    int		oerrno;

    (void) getmessage(m, S_CANCEL, &destination, &user, &req_id);
    syslog(LOG_DEBUG, "s_cancel(%s, %s, %s)",
	   (destination ? destination : "NULL"), (user ? user : "NULL"),
	   (req_id ? req_id : "NULL"));

    if (STREQU(destination, NAME_ALL))
	destination = "";
    if (STREQU(req_id, NAME_ALL))
	req_id = "";

    if (rid = _cancel(md, destination, user, req_id)) {
	oerrno = errno;

	while ((nrid = _cancel(md, NULL, NULL, NULL)) != NULL) {
	    nerrno = errno;
	    mputm(md, R_CANCEL, MOKMORE, oerrno, rid);
	    Free(rid);
	    rid = nrid;
	    oerrno = nerrno;
	}
	mputm(md, R_CANCEL, MOK, oerrno, rid);
	Free(rid);
	return;
    }

    mputm(md, R_CANCEL, MOK, MUNKNOWN, "");
}

/**
 ** s_inquire_request()
 **/

void s_inquire_request(char *m, MESG *md)
{
    char	*form;
    char	*dest;
    char	*pwheel;
    char	*user;
    char	*req_id;
    RSTATUS	*rp;
    RSTATUS	*found;
    char files[BUFSIZ];

    found = (RSTATUS *)0;

    (void) getmessage(m, S_INQUIRE_REQUEST,&form,&dest,&req_id,&user,&pwheel);
    syslog(LOG_DEBUG, "s_inquire_request(%s, %s, %s, %s, %s)",
	   (form ? form : "NULL"), (dest ? dest : "NULL"),
	   (req_id ? req_id : "NULL"), (user ? user : "NULL"),
	   (pwheel ? pwheel : "NULL"));

    for(rp = Request_List; rp != NULL; rp = rp->next) {
	if (*form && !SAME(form, rp->request->form))
	    continue;

	if (*dest && !STREQU(dest, rp->request->destination)) {
	    if (!rp->printer)
		continue;
	    if (!STREQU(dest, rp->printer->printer->name))
		continue;
	}
	if (*req_id && !STREQU(req_id, rp->secure->req_id))
	    continue;

	if (*user && !bangequ(user, rp->secure->user))
	    continue;

	if (*pwheel && !SAME(pwheel, rp->pwheel_name))
	    continue;
	
	if (found) {
	    GetRequestFiles(found->request, files, sizeof(files));
	    mputm(md, R_INQUIRE_REQUEST,
		 MOKMORE,
		 found->secure->req_id,
		 found->request->user,
			/* bgolden 091996, bug 1257405 */
		 found->secure->size,
		 found->secure->date,
		 found->request->outcome,
		 found->printer->printer->name,
		 (found->form? found->form->form->name : ""),
		 NB(found->pwheel_name),
		 files
	    );
	}
	found = rp;
    }

    if (found) {
	GetRequestFiles(found->request, files, sizeof(files));
	mputm(md, R_INQUIRE_REQUEST,
	     MOK,
	     found->secure->req_id,
	     found->request->user, /* bgolden 091996, bug 1257405 */
	     found->secure->size,
	     found->secure->date,
	     found->request->outcome,
	     found->printer->printer->name,
	     (found->form? found->form->form->name : ""),
	     (NB(found->pwheel_name)), 
	     files
	);
    } else
	mputm(md, R_INQUIRE_REQUEST, MNOINFO, "", "", 0L, 0L, 0, "", "", "",
	      "");

    return;
}


/*
 * s_inquire_request_rank()
 */

void s_inquire_request_rank(char *m, MESG *md)
{
	char		*form;
	char		*dest;
	char		*pwheel;
	char		*user;
	char		*req_id;
	RSTATUS		*rp;
	RSTATUS		*found = NULL;
	PSTATUS		*pps;
	int		found_rank = 0;
	short		prop;
	char		files[BUFSIZ];

	(void) getmessage(m, S_INQUIRE_REQUEST_RANK, &prop, &form, &dest,
		&req_id, &user, &pwheel);
	syslog(LOG_DEBUG, "s_inquire_request_rank(%d, %s, %s, %s, %s, %s)",
	   prop, (form ? form : "NULL"), (dest ? dest : "NULL"),
	   (req_id ? req_id : "NULL"), (user ? user : "NULL"),
	   (pwheel ? pwheel : "NULL"));

	if (!prop)
		goto SendBackStatus;

	for (rp = Request_List; rp != NULL; rp = rp->next) {
		if (*form && !SAME(form, rp->request->form))
			continue;

		if (*dest && !STREQU(dest, rp->request->destination))
			continue;

		if (*req_id && !STREQU(req_id, rp->secure->req_id))
			continue;

		if (*user && !bangequ(user, rp->secure->user))
			continue;

		if (*pwheel && !SAME(pwheel, rp->pwheel_name))
			continue;
	}

SendBackStatus:
	for (pps = walk_ptable(1); pps; pps = walk_ptable(0))
		pps->nrequests = 0;

	for (rp = Request_List; rp != NULL; rp = rp->next) {
		if (rp->printer && !(rp->request->outcome & RS_DONE))
			rp->printer->nrequests++;

		if (*form && !SAME(form, rp->request->form))
			continue;

		if (*dest && !STREQU(dest, rp->request->destination))
			continue;
	
		if (*req_id && !STREQU(req_id, rp->secure->req_id))
			continue;

		if (*user && !bangequ(user, rp->secure->user))
			continue;

		if (*pwheel && !SAME(pwheel, rp->pwheel_name))
			continue;

		if (found) {
			GetRequestFiles(found->request, files, sizeof(files));
			mputm(md, R_INQUIRE_REQUEST_RANK,
				MOKMORE,
				found->secure->req_id,
				found->request->user,
					/* bgolden 091996, bug 1257405 */
				found->secure->size,
				found->secure->date,
				found->request->outcome,
				found->printer->printer->name,
				(found->form? found->form->form->name : ""),
				NB(found->pwheel_name),
				((found->status & RSS_RANK) ?
				    found->rank : found_rank),
				files
			);
		}
		found = rp;
		found_rank = found->printer->nrequests;
	}

	if (found) {
		GetRequestFiles(found->request, files, sizeof(files));
		mputm(md, R_INQUIRE_REQUEST_RANK,
			MOK,
			found->secure->req_id,
			found->request->user, /* bgolden 091996, bug 1257405 */
			found->secure->size,
			found->secure->date,
			found->request->outcome,
			found->printer->printer->name,
			(found->form? found->form->form->name : ""),
			NB(found->pwheel_name),
			((found->status & RSS_RANK) ? found->rank : found_rank),
			files
		);
	} else
		mputm(md, R_INQUIRE_REQUEST_RANK, MNOINFO, "", "", 0L, 0L, 0,
			"", "", "", 0, "");
}

static int
mv_file(RSTATUS *rp, char *dest)
{
	int	stat;
	char	*olddest;
	EXEC	*oldexec;
	SECURE * securep;
	RSTATUS * prs;
	char * reqno;

	oldexec = rp->printer->exec;
	olddest = rp->request->destination;
	rp->request->destination = Strdup(dest);
	if ((stat = validate_request(rp, (char **)0, 1)) == MOK) {
		Free(olddest);

		if (rp->request->outcome & RS_FILTERED) {
			int cnt = 0;
			char *reqno;
			char **listp;
			char tmp_nam[MAXPATHLEN];

			reqno = getreqno(rp->secure->req_id);
			for (listp = rp->request->file_list; *listp; listp++) {
				cnt++;
				sprintf(tmp_nam, "%s/%s/F%s-%d", Lp_Tmp,
					rp->secure->system, reqno, cnt);
				unlink(tmp_nam);

			}
			rp->request->outcome &= ~RS_FILTERED;
		}

		/* update /var/spool/lp/tmp/<host>/nnn-0 */
		if (putrequest(rp->req_file, rp->request) < 0) {
			note("putrequest failed\n");
			return (MNOMEM);
		}

		/* update /var/spool/lp/requests/<host>/nnn-0 */
		if ((securep = Getsecure(rp->req_file))) {
			reqno = strdup(getreqno(securep->req_id));
			(void) free(securep->req_id);
			if ((securep->req_id = calloc(strlen(dest) + 1 +
				strlen(reqno) +1, sizeof (char))) == NULL) 
				return (MNOMEM);
			(void) sprintf(securep->req_id, "%s-%s", dest, reqno);
			/* remove the old request file; save new one */
			(void) rmsecure(rp->secure->req_id);
			if (Putsecure(rp->req_file, securep) < 0) {
				/* Putsecure includes note/errmessage */
				return (MNOMEM);
			}
		} else {
			note("Getsecure failed\n");
			return (MNOMEM);
		}

		/* update internal jobs list: Request_list */
		if (prs = request_by_id(rp->secure->req_id)) {
			free(prs->secure->req_id);
			prs->secure->req_id = strdup(securep->req_id);
		} else {
			note("request_by_id failed\n");
			return (MUNKNOWN);
		}

		/*
		 * If the request was being filtered or was printing,
		 * it would have been stopped in "validate_request()",
		 * but only if it has to be refiltered. Thus, the
		 * filtering has been stopped if it has to be stopped,
		 * but the printing may still be going.
		 */
		if (rp->request->outcome & RS_PRINTING &&
		    !(rp->request->outcome & RS_STOPPED)) {
			rp->request->outcome |= RS_STOPPED;
			terminate (oldexec);
	        }

		maybe_schedule(rp);
		return (MOK);
	}

	Free(rp->request->destination);
	rp->request->destination = olddest;
	return (stat);
}

/*
 * s_move_request()
 */

void s_move_request(char *m, MESG *md)
{
	RSTATUS	*rp;
	short	err;
	char	*req_id;
	char	*dest;

	(void) getmessage(m, S_MOVE_REQUEST, &req_id, &dest);
	syslog(LOG_DEBUG, "s_move_request(%s, %s)", (req_id ? req_id : "NULL"),
	       (dest ? dest : "NULL"));


	if (!(search_ptable(dest)) && !(search_ctable(dest))) {
		mputm(md, R_MOVE_REQUEST, MNODEST, 0L);
		return;
	}

	if ((rp = request_by_id(req_id))) {
		if (STREQU(rp->request->destination, dest)) {
			mputm(md, R_MOVE_REQUEST, MOK, 0L);
			return;
		}
		if (rp->request->outcome & (RS_DONE|RS_NOTIFYING)) {
			mputm(md, R_MOVE_REQUEST, M2LATE, 0L);
			return;
		}
		if (rp->request->outcome & RS_CHANGING)	{
			mputm(md, R_MOVE_REQUEST, MBUSY, 0L);
			return;
		}
		if ((err = mv_file(rp, dest)) == MOK) {
			mputm(md, R_MOVE_REQUEST, MOK, 0L);
			return;
		}
		mputm(md, R_MOVE_REQUEST, err, chkprinter_result);
		return;
	}
	mputm(md, R_MOVE_REQUEST, MUNKNOWN, 0L);
}

/**
 ** s_move_dest()
 **/

void s_move_dest(char *m, MESG *md)
{
    char		*dest;
    char		*fromdest;
    RSTATUS		*rp;
    char		*found = (char *)0;
    short		num_ok = 0;

    (void) getmessage(m, S_MOVE_DEST, &fromdest, &dest);
    syslog(LOG_DEBUG, "s_move_dest(%s, %s)", (fromdest ? fromdest : "NULL"),
	   (dest ? dest : "NULL"));

    if (!search_ptable(fromdest) && !search_ctable(fromdest))
    {
	mputm(md, R_MOVE_DEST, MNODEST, fromdest, 0);
	return;
    }

    if (!(search_ptable(dest)) && !(search_ctable(dest)))
    {
	mputm(md, R_MOVE_DEST, MNODEST, dest, 0);
	return;
    }

    if (STREQU(dest, fromdest))
    {
	mputm(md, R_MOVE_DEST, MOK, "", 0);
	return;
    }

    BEGIN_WALK_BY_DEST_LOOP (rp, fromdest)
	if (!(rp->request->outcome &
	    (RS_DONE|RS_CHANGING|RS_NOTIFYING))) {
	    if (mv_file(rp, dest) == MOK) {
		num_ok++;
		continue;
	    }
	}

	if (found)
	    mputm(md, R_MOVE_DEST, MMORERR, found, 0);

	found = rp->secure->req_id;
    END_WALK_LOOP

    if (found)
	mputm(md, R_MOVE_DEST, MERRDEST, found, num_ok);
    else
	mputm(md, R_MOVE_DEST, MOK, "", num_ok);
}

/**
 ** reqpath
 **/

static char *
reqpath(char *file, char **idnumber)
{
    char	*path;
    char	*cp;
    char	*cp2;
    
    /*
    **	/var/spool/lp/tmp/machine/123-0
    **	/var/spool/lp/temp/123-0
    **	/usr/spool/lp/temp/123-0
    **	/usr/spool/lp/tmp/machine/123-0
    **	123-0
    **	machine/123-0
    **
    **	/var/spool/lp/tmp/machine/123-0 + 123
    */
    if (*file == '/')
    {
	/*CONSTCOND*/
	if (STRNEQU(file, Lp_Spooldir, strlen(Lp_Spooldir)))
	    cp = file + strlen(Lp_Spooldir) + 1;
	else
	    if(STRNEQU(file, "/usr/spool/lp", 13))
		cp = file + strlen("/usr/spool/lp") + 1;
	    else
	    {
		*idnumber = NULL;
		return(NULL);
	    }

	if (STRNEQU(cp, "temp", 4))
	{
	    cp += 5;
	    path = makepath(Local_System, cp, NULL);
	}
	else
	    path = Strdup(cp);
    }
    else
    {
	if (strchr(file, '/'))
	    path = makepath(file, NULL);
	else
	    path = makepath(Local_System, file, NULL);
    }

    cp = strrchr(path, '/');
    cp++;
    if ((cp2 = strrchr(cp, '-')) == NULL)
	*idnumber = Strdup(cp);
    else
    {
	*cp2 = '\0';
	*idnumber = Strdup(cp);
	*cp2 = '-';
    }

    return(path);
}
