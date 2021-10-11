/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)process.c	1.5	96/05/03 SMI"	/* SVr4.0 1.2	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */


    /* process.c handles the requests, which can be of three types:

		ANNOUNCE - announce to a user that a talk is wanted

		LEAVE_INVITE - insert the request into the table
		
		LOOK_UP - look up to see if a request is waiting in
			  in the table for the local user

		DELETE - delete invitation

     */

#include "ctl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <utmpx.h>
#include <unistd.h>
#include <stdlib.h>

static void do_announce(CTL_MSG *request, CTL_RESPONSE *response);
static int find_user(char *name, char *tty);

extern CTL_MSG *find_request(CTL_MSG *request);
extern CTL_MSG *find_match(CTL_MSG *request);
extern void insert_table(CTL_MSG *request, CTL_RESPONSE *response);
extern int delete_invite(int id_num);
extern int announce(CTL_MSG *request, char *remote_machine);
extern int new_id(void);

void
process_request(CTL_MSG *request, CTL_RESPONSE *response)
{
    CTL_MSG *ptr;

    response->type = request->type;
    response->id_num = 0;

    switch (request->type) {

	case ANNOUNCE :

	    do_announce(request, response);
	    break;

	case LEAVE_INVITE :

	    ptr = find_request(request);
	    if (ptr != (CTL_MSG *) 0) {
		response->id_num = ptr->id_num;
		response->answer = SUCCESS;
	    } else {
		insert_table(request, response);
	    }
	    break;

	case LOOK_UP :

	    ptr = find_match(request);
	    if (ptr != (CTL_MSG *) 0) {
		response->id_num = ptr->id_num;
		response->addr = ptr->addr;
		response->answer = SUCCESS;
	    } else {
		response->answer = NOT_HERE;
	    }
	    break;

	case DELETE :

	    response->answer = delete_invite(request->id_num);
	    break;

	default :

	    response->answer = UNKNOWN_REQUEST;
	    break;
    }
}

static void
do_announce(CTL_MSG *request, CTL_RESPONSE *response)
{
    struct hostent *hp;
    CTL_MSG *ptr;
    int result;

	/* see if the user is logged */

    result = find_user(request->r_name, request->r_tty);

    if (result != SUCCESS) {
	response->answer = result;
	return;
    }

    hp = gethostbyaddr((const char *)&request->ctl_addr.sin_addr,
			  sizeof(struct in_addr), AF_INET);

    if ( hp == (struct hostent *) 0 ) {
	response->answer = MACHINE_UNKNOWN;
	return;
    }

    ptr = find_request(request);
    if (ptr == (CTL_MSG *) 0) {
	insert_table(request,response);
	response->answer = announce(request, hp->h_name);
    } else if (request->id_num > ptr->id_num) {
	    /*
	     * this is an explicit re-announce, so update the id_num
	     * field to avoid duplicates and re-announce the talk 
	     */
	ptr->id_num = response->id_num = new_id();
	response->answer = announce(request, hp->h_name);
    } else {
	    /* a duplicated request, so ignore it */
	response->id_num = ptr->id_num;
	response->answer = SUCCESS;
    }

    return;
}

/*
 * Search utmp for the local user
 */

static int
find_user(char *name, char *tty)
{
    struct utmpx *ubuf;
    int tfd;
    char dev[100];


    setutxent();		/* reset the utmpx file */

    while (ubuf = getutxent()) {
 	if (ubuf->ut_type == USER_PROCESS &&
  	    strncmp(ubuf->ut_user, name, sizeof ubuf->ut_user) == 0) {

	    /* check if this entry is really a tty */
	    strcpy(dev, "/dev/");
	    strncat(dev, ubuf->ut_line, sizeof(ubuf->ut_line));
	    if ((tfd = open(dev, O_WRONLY|O_NOCTTY)) == -1) {
		continue;
	    }
	    if (!isatty(tfd)) {
		close(tfd);
		openlog("talk", 0, LOG_AUTH);
		syslog(LOG_CRIT, "%.*s in utmp is not a tty\n",
			sizeof(ubuf->ut_line), ubuf->ut_line);
		closelog();
		continue;
	    }
	    close(tfd);
	    if (*tty == '\0') {
		    /* no particular tty was requested */
		(void) strcpy(tty, ubuf->ut_line);
    		endutxent();		/* close the utmpx file */
		return(SUCCESS);
	    } else if (strcmp(ubuf->ut_line, tty) == 0) {
    		endutxent();		/* close the utmpx file */
		return(SUCCESS);
	    }
	}
    }

    endutxent();		/* close the utmpx file */
    return(NOT_HERE);
}
