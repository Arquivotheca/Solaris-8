/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)in.talkd.c	1.5	97/05/16 SMI"	/* SVr4.0 1.3	*/

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


/*
 * Invoked by the Internet daemon to handle talk requests
 * Processes talk requests until MAX_LIFE seconds go by with 
 * no action, then dies.
 */

#include "ctl.h"

#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/systeminfo.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

CTL_MSG request;
CTL_RESPONSE response;

char hostname[HOST_NAME_LENGTH];

int debug = 0;

CTL_MSG swapmsg();

void print_error(char *string);
extern void print_response(CTL_RESPONSE *response);
extern void print_request(CTL_MSG *request);
extern void process_request(CTL_MSG *request, CTL_RESPONSE *response);

int
main()
{
    struct sockaddr_in from;
    socklen_t from_size = (socklen_t)sizeof(from);
    int cc;
    int name_length = sizeof(hostname);
    fd_set rfds;
    struct timeval tv;

    (void) sysinfo(SI_HOSTNAME, hostname, name_length);

    for (;;) {
	tv.tv_sec = MAX_LIFE;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	if (select(1, &rfds, 0, 0, &tv) <= 0)
		return (0); 
	cc = recvfrom(0, (char *)&request, sizeof (request), 0, 
		      (struct sockaddr *)&from, &from_size);

	if (cc != sizeof(request)) {
	    if (cc < 0 && errno != EINTR) {
		print_error("receive");
	    }
	} else {

	    if (debug) printf("Request received : \n");
	    if (debug) print_request(&request);

	    request = swapmsg(request);
	    process_request(&request, &response);

	    if (debug) printf("Response sent : \n");
	    if (debug) print_response(&response);

		/* can block here, is this what I want? */

	    cc = sendto(0, (char *) &response, sizeof(response), 0,
			(struct sockaddr *)&request.ctl_addr,
			(socklen_t)sizeof(request.ctl_addr));

	    if (cc != sizeof(response)) {
		print_error("Send");
	    }
	}
    }
}

void
print_error(char *string)
{
    FILE *cons;
    char *err_dev = "/dev/console";
    char *sys;
    pid_t val, pid;

    if (debug) err_dev = "/dev/tty";

    if ((sys = strerror(errno)) == (char *) NULL)
	    sys = "Unknown error";

	/* don't ever open tty's directly, let a child do it */
    if ((pid = fork()) == 0) {
	cons = fopen(err_dev, "a");
	if (cons != NULL) {
	    fprintf(cons, "Talkd : %s : %s(%d)\n\r", string, sys,
		    errno);
	    fclose(cons);
	}
	exit(0);
    } else {
	    /* wait for the child process to return */
	do {
	    val = wait(0);
	    if (val == (pid_t)-1) {
		if (errno == EINTR) {
		    continue;
		} else if (errno == ECHILD) {
		    break;
		}
	    }
	} while (val != pid);
    }
}

#define swapshort(a) (((a << 8) | ((unsigned short) a >> 8)) & 0xffff)
#define swaplong(a) ((swapshort(a) << 16) | (swapshort(((unsigned)a >> 16))))

/*  
 * heuristic to detect if need to swap bytes
 */

CTL_MSG
swapmsg(req)
	CTL_MSG req;
{
	CTL_MSG swapreq;
	
	if (req.ctl_addr.sin_family == swapshort(AF_INET)) {
		swapreq = req;
		swapreq.id_num = swaplong(req.id_num);
		swapreq.pid = swaplong(req.pid);
		swapreq.addr.sin_family = swapshort(req.addr.sin_family);
		swapreq.ctl_addr.sin_family =
			swapshort(req.ctl_addr.sin_family);
		return swapreq;
	}
	else
		return req;
}
