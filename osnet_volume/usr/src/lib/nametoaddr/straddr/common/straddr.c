/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	 All Rights Reserved 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)straddr.c	1.20	99/06/04 SMI"

#include <ctype.h>
#include <mtlib.h>
#include <stdio.h>
#include <tiuser.h>
#include <netdir.h>
#include <netconfig.h>
#include <sys/utsname.h>
#include <string.h>
#include <malloc.h>
#include <synch.h>

/*
 *	The generic name to address mappings for any transport that
 *	has strings for address (e.g., ISO Starlan).
 *
 *	Address in ISO Starlan consist of arbitrary strings of
 *	characters.  Because of this, the following routines
 *	create an "address" based on two strings, one gotten
 *	from a "host" file and one gotten from a "services" file.
 *	The two strings are catenated together (with a "." between
 *	them).  The hosts file is /etc/net/starlan/hosts and
 *	contain lines of the form:
 *
 *		arbitrary_string	machname
 *
 *	To make things simple, the "arbitrary string" should be the
 *	machine name.
 *
 *	The services file is /etc/net/starlan/services and has lines
 *	of the form:
 *
 *		service_name	arbitrary_string
 *
 *	Again, to make things easer, the "arbitrary name" should be the
 *	service name.
 */

#define	HOSTFILE	"/etc/net/%s/hosts"
#define	SERVICEFILE	"/etc/net/%s/services"
#define	FIELD1		1
#define	FIELD2		2

extern	int	_uname(struct utsname *);

#ifndef PIC
char *str_taddr2uaddr(struct netconfig *, struct netbuf *);
#endif

/*
 *	Local functions used.
 */

static int searchhost(FILE *, char *, int *, int, char *);
static int searchserv(FILE *, char *, int, char *);
static char *mergeaddr(struct netconfig *, char	*, char *);

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define	FALSE	0
#endif

/*
 *	_netdir_getbyname() returns all of the addresses for
 *	a specified host and service.
 *	Routine names are prepended with str_ if this is statically
 *	compiled.
 */

static mutex_t	byname_lock = DEFAULTMUTEX;

struct nd_addrlist *
#ifdef PIC
_netdir_getbyname(netconfigp, nd_hostservp)
#else
str_netdir_getbyname(netconfigp, nd_hostservp)
#endif PIC
struct netconfig    *netconfigp;
struct nd_hostserv  *nd_hostservp;
{
	char   searchfile[BUFSIZ]; /* the name of the file to  be opened   */
	char   fulladdr[BUFSIZ];   /* holds the full address string	   */
	FILE   *fp;		   /* points to the hosts and service file */
	struct nd_addrlist *retp;  /* the return structure		   */
	struct netbuf *netbufp;    /* indexes through the addresses	   */
	static char *save_myname;  /* my own nodename			   */
	const char  *myname;

	mutex_lock(&byname_lock);
	if (save_myname == (char *)NULL) {
		static struct utsname utsname;


#if defined(i386)
		(void) _nuname(&utsname);
#else
		(void) _uname(&utsname);
#endif
		save_myname = strdup(utsname.nodename);
		if (save_myname == (char *)NULL) {
			mutex_unlock(&byname_lock);
			_nderror = ND_NOMEM;
			return (NULL);
		}
	}
	myname = save_myname;
	mutex_unlock(&byname_lock);

	/*
	 *	If the given hostname is HOST_SELF_{BIND,CONNECT}, simply re-set
	 *	it to the uname of this machine.  If it is HOST_ANY
	 *	or HOST_BROADCAST, return an error (since they are not
	 *	supported).
	 */

	if (strcmp(nd_hostservp->h_host, HOST_BROADCAST) == 0) {
		_nderror = ND_NOHOST;
		return (NULL);
	}

	if ((strcmp(nd_hostservp->h_host, HOST_SELF_BIND) == 0) ||
		(strcmp(nd_hostservp->h_host, HOST_SELF_CONNECT) == 0) ||
	    (strcmp(nd_hostservp->h_host, HOST_ANY) == 0) ||
	    (strcmp(nd_hostservp->h_host, myname) == 0)) {
		(void) strcpy(fulladdr, myname);
	} else {
		/*
		 *	Get the first part of the address.  The hosts file
		 *	is created with the local name of the loopback transport
		 *	(given in netconfigp->nc_netid).
		 *
		 *	XXX: there is really no need for this code to exist.
		 *	it should just return NULL in this section.
		 */
		int num;

		(void) sprintf(searchfile, HOSTFILE, netconfigp->nc_netid);
		if (((fp = fopen(searchfile, "r")) == NULL) ||
		    (searchhost(fp, nd_hostservp->h_host, &num, FIELD2,
				fulladdr) == FALSE)) {
			if (fp != NULL) {
				(void) fclose(fp);
			}
			_nderror = ND_NOHOST;
			return (NULL);
		}
		(void) fclose(fp);
	}

	/*
	 *	Now simply fill in the address by forming strings of the
	 *	form "string_from_hosts.string_from_services"
	 */

	if (nd_hostservp->h_serv &&
	    (strcmp(nd_hostservp->h_serv, "rpcbind") == 0)) {
		(void) strcat(fulladdr, ".");
		(void) strcat(fulladdr, "rpc");	/* hard coded */
	} else {
		/*
		 *	Get the address from the services file
		 */

		if (nd_hostservp->h_serv && (nd_hostservp->h_serv[0] != '\0')) {
			(void) sprintf(searchfile, SERVICEFILE,
				       netconfigp->nc_netid);
			(void) strcat(fulladdr, ".");
			if (((fp = fopen(searchfile, "r")) == NULL) ||
			    ((searchserv(fp, nd_hostservp->h_serv, FIELD1,
				fulladdr + (int)strlen(fulladdr))) == FALSE)) {
				if (fp != NULL) {
					(void) fclose(fp);
				}
				_nderror = ND_NOSERV;
				return (NULL);
			}
			(void) fclose(fp);
		}
	}

	if ((retp = (struct nd_addrlist *)
		malloc(sizeof (struct nd_addrlist))) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}

	/*
	 *	We do not worry about multiple addresses here.  Loopbacks
	 *	have only one interface.
	 */

	retp->n_cnt = 1;
	if ((retp->n_addrs = (struct netbuf *)
	     malloc(sizeof (struct netbuf))) == NULL) {
		free(retp);
		_nderror = ND_NOMEM;
		return (NULL);
	}

	netbufp = retp->n_addrs;

	/*
	 *	don't include the terminating NULL character in the
	 *	length.
	 */

	netbufp->len = netbufp->maxlen = (int)strlen(fulladdr);
	if ((netbufp->buf = strdup(fulladdr)) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}
	_nderror = ND_OK;
	return (retp);
}

/*
 *	_netdir_getbyaddr() takes an address (hopefully obtained from
 *	someone doing a _netdir_getbyname()) and returns all hosts with
 *	that address.
 */

struct nd_hostservlist *
#ifdef PIC
_netdir_getbyaddr(netconfigp, netbufp)
#else
str_netdir_getbyaddr(netconfigp, netbufp)
#endif PIC
struct netconfig *netconfigp;
struct netbuf	 *netbufp;
{
	char   searchfile[BUFSIZ];	  /* the name of file to be opened  */
	char   fulladdr[BUFSIZ];	  /* a copy of the address string   */
	char   servbuf[BUFSIZ];	  	  /* a buffer for service string    */
	char   hostbuf[BUFSIZ];		  /* points to list of host names   */
	char   *hostname;		  /* the "first" path of the string */
	char   *servname;		  /* the "second" part of string    */
	struct nd_hostservlist *retp;	  /* the return structure	    */
	FILE   *fp;			  /* pointer to open files	    */
	char   *serv;			  /* resultant service name obtained */
	int    nhost;			  /* the number of hosts in hostpp  */
	struct nd_hostserv *nd_hostservp; /* traverses the host structures  */

	/*
	 *	Separate the two parts of the address string.
	 */

	(void) strncpy(fulladdr, netbufp->buf, netbufp->len);
	fulladdr[netbufp->len] = '\0';
	hostname = strtok(fulladdr, ".");
	if (hostname == NULL) {
		_nderror = ND_NOHOST;
		return (NULL);
	}
	servname = strtok(NULL, " \n\t");

	/*
	 *	Search for all the hosts associated with the
	 *	first part of the address string.
	 */

	(void) sprintf(searchfile, HOSTFILE, netconfigp->nc_netid);
	hostbuf[0] = NULL;
	if (((fp = fopen(searchfile, "r")) == NULL) ||
	    ((searchhost(fp, hostname, &nhost, FIELD1, hostbuf)) == FALSE)) {
		_nderror = ND_NOHOST;
		if (fp != NULL) {
			(void) fclose(fp);
		}
		return (NULL);
	}
	(void) fclose(fp);

	/*
	 *	Search for the service associated with the second
	 *	path of the address string.
	 */

	(void) sprintf(searchfile, SERVICEFILE, netconfigp->nc_netid);

	if (servname == NULL) {
		_nderror = ND_NOSERV;
		return (NULL);
	}

	servbuf[0] = NULL;
	serv = servbuf;
	if (((fp = fopen(searchfile, "r")) == NULL) ||
	    ((searchserv(fp, servname, FIELD2, servbuf)) == FALSE)) {
#ifdef PIC
		serv = _taddr2uaddr(netconfigp, netbufp);
#else
		serv = str_taddr2uaddr(netconfigp, netbufp);
#endif
		(void) strcpy(servbuf, serv);
		free(serv);
		serv = servbuf;
		while (*serv != '.')
			serv ++;
	} 
	if (fp != NULL) {
		(void) fclose(fp);
	}

	/*
	 *	Allocate space to hold the return structure, set the number
	 *	of hosts, and allocate space to hold them.
	 */

	if ((retp = (struct nd_hostservlist *)
		malloc(sizeof (struct nd_hostservlist))) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}

	retp->h_cnt = nhost;
	if ((retp->h_hostservs = (struct nd_hostserv *)
	    malloc(nhost * sizeof (struct nd_hostserv))) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}

	/*
	 *	Loop through the host structues and fill them in with
	 *	each host name (and service name).
	 */

	nd_hostservp = retp->h_hostservs;
	hostname = strtok(hostbuf, ",");
	while (hostname && nhost--) {
		if (((nd_hostservp->h_host = strdup(hostname)) == NULL) ||
		((nd_hostservp->h_serv = strdup(serv)) == NULL)) {
			_nderror = ND_NOMEM;
			return (NULL);
		}
		nd_hostservp++;
		hostname = strtok(NULL, ",");
	}

	_nderror = ND_OK;
	return (retp);
}

/*
 *	_taddr2uaddr() translates a address into a "universal" address.
 *	Since the address is a string, simply return the string as the
 *	universal address (but replace all non-printable characters with
 *	the \ddd form, where ddd is three octal digits).  The '\n' character
 *	is also replace by \ddd and the '\' character is placed as two
 *	'\' characters.
 */

char *
/* ARGSUSED */
#ifdef PIC
_taddr2uaddr(netconfigp, netbufp)
#else
str_taddr2uaddr(netconfigp, netbufp)
#endif
struct netconfig *netconfigp;
struct netbuf    *netbufp;
{
	char *retp;	/* pointer the return string			*/
	char *to;	/* traverses and populates the return string	*/
	char *from;	/* traverses the string to be converted		*/
	int i;		/* indexes through the given string		*/

	/*
	 * BUFSIZE is perhaps too big for this one and there is a better
	 * way to optimize it, but for now we will just assume BUFSIZ
	 */
	if ((retp = malloc(BUFSIZ)) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}
	to = retp;
	from = netbufp->buf;

	for (i = 0; i < netbufp->len; i++) {
		if (*from == '\\') {
			*to++ = '\\';
			*to++ = '\\';
		} else {
			if (*from == '\n' || !isprint((unsigned char)*from)) {
				(void) sprintf(to, "\\%.3o", *from & 0xff);
				to += 4;
			} else {
				*to++ = *from;
			}
		}
		from++;
	}
	*to = '\0';
	return (retp);
}

/*
 *	_uaddr2taddr() translates a universal address back into a
 *	netaddr structure.  Since the universal address is a string,
 *	put that into the TLI buffer (making sure to change all \ddd
 *	characters back and strip off the trailing \0 character).
 */

struct netbuf *
/* ARGSUSED */
#ifdef PIC
_uaddr2taddr(netconfigp, uaddr)
#else
str_uaddr2taddr(netconfigp, uaddr)
#endif PIC
char *uaddr;
struct netconfig *netconfigp;
{
	struct netbuf *retp;	/* the return structure			   */
	char *holdp;		/* holds the converted address		   */
	char *to;		/* traverses and populates the new address */
	char *from;		/* traverses the universal address	   */

	holdp = malloc(strlen(uaddr) + 1);
	if (holdp == (char *)NULL) {
		_nderror = ND_NOMEM;
		return ((struct netbuf *)NULL);
	}
	from = uaddr;
	to = holdp;

	while (*from) {
		if (*from == '\\') {
			if (*(from+1) == '\\') {
				*to = '\\';
				from += 2;
			} else {
				*to = ((*(from+1) - '0') << 6) +
					((*(from+2) - '0') << 3) +
					(*(from+3) - '0');
				from += 4;
			}
		} else {
			*to = *from++;
		}
		to++;
	}
	*to = '\0';

	if ((retp = (struct netbuf *)malloc(sizeof (struct netbuf))) == NULL) {
		_nderror = ND_NOMEM;
		return (NULL);
	}
	retp->maxlen = retp->len = (int)(to - holdp);
	retp->buf = holdp;
	return (retp);
}

/*
 *	_netdir_options() is a "catch-all" routine that does
 *	transport specific things.  The only thing that these
 *	routines have to worry about is ND_MERGEADDR.
 */

int
/* ARGSUSED */
#ifdef PIC
_netdir_options(netconfigp, option, fd, par)
#else
str_netdir_options(netconfigp, option, fd, par)
#endif PIC
struct netconfig *netconfigp;
int		  option;
int		  fd;
void		 *par;
{
	struct nd_mergearg *argp;  /* the argument for mergeaddr */

	switch (option) {
	    case ND_MERGEADDR:
		argp = (struct nd_mergearg *)par;
		argp->m_uaddr = mergeaddr(netconfigp, argp->s_uaddr,
						argp->c_uaddr);
		return (argp->m_uaddr == NULL? -1 : 0);
	    default:
		_nderror = ND_NOCTRL;
		return (-1);
	}
}

/*
 *	mergeaddr() translates a universal address into something
 *	that makes sense to the caller.  This is a no-op in loopback's case,
 *	so just return the universal address.
 */

static char *
/* ARGSUSED */
mergeaddr(netconfigp, uaddr, ruaddr)
struct netconfig *netconfigp;
char		 *uaddr;
char		 *ruaddr;
{
	return (strdup(uaddr));
}

/*
 *	searchhost() looks for the specified token in the host file.
 *	The "field" parameter signifies which field to compare the token
 *	on, and returns all comma separated values associated with the token.
 */

static
searchhost(fp, token, nelements, field, hostbuf)
FILE  *fp;
char  *token;
int   *nelements;
int   field;
char  *hostbuf;
{
	char buf[BUFSIZ];	/* holds each line of the file		    */
	char *fileaddr;		/* the first token in each line		    */
	char *filehost;		/* the second token in each line	    */
	char *cmpstr;		/* the string to compare token to	    */
	char *retstr;		/* the string to return if compare succeeds */

	/*
	 *	nelements will contain the number of token found, so
	 *	initially set it to 0.
	 */

	*nelements = 0;

	/*
	 *	Loop through the file looking for the tokens and creating
	 *	the list of strings to be returned.
	 */

	while (fgets(buf, BUFSIZ, fp) != NULL) {

		/*
		 *	Ignore comments and bad lines.
		 */

		if (((fileaddr = strtok(buf, " \t\n")) == NULL) ||
		    (*fileaddr == '#') ||
		    ((filehost = strtok(NULL, " \t\n")) == NULL)) {
			continue;
		}

		/*
		 *	determine which to compare the token to, then
		 *	compare it, and if they match, add the return
		 *	string to the list.
		 */

		cmpstr = (field == FIELD1)? fileaddr : filehost;
		retstr = (field == FIELD1)? filehost : fileaddr;

		if (strcmp(token, cmpstr) == 0) {
			(*nelements)++;
			if (field == FIELD2) {
				/*
				 * called by _netdir_getbyname
				 */

				(void) strcpy(hostbuf, retstr);
				break;
			}
			if (*nelements > 1) {
				/*
				 * Assuming that "," will never be a part
				 * of any host name.
				 */
				(void) strcat(hostbuf, ",");
			}
			(void) strcat(hostbuf, retstr);
		}
	}

	/*
	 *	If *nelements is 0 then no matches were found.
	 */

	if (*nelements == 0) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 *	searchserv() looks for the specified token in the service file.
 *	The "field" parameter signifies which field to compare the token
 *	on, and returns the string associated with the token in servname.
 */

static
searchserv(fp, token, field, servname)
FILE  *fp;
char  *token;
int   field;
char  *servname;
{
	char buf[BUFSIZ];	 /* buffer space for lines in file	   */
	char *fileservice;	 /* the first token in each line	   */
	char *fileport;		 /* the second token in each line	   */
	char *cmpstr;		 /* the string to compare the token to	   */
	char *retstr;		 /* temporarily hold token in line of file */

	/*
	 *	Loop through the services file looking for the token.
	 */

	while (fgets(buf, BUFSIZ, fp) != NULL) {
		/*
		 *	If comment or bad line, continue.
		 */
		if (((fileservice = strtok(buf, " \t\n")) == NULL) ||
		    (*fileservice == '#') ||
		    ((fileport = strtok(NULL, " \t\n")) == NULL)) {
			continue;
		}

		cmpstr = (field == FIELD1)? fileservice : fileport;
		retstr = (field == FIELD1)? fileport : fileservice;

		if (strcmp(token, cmpstr) == 0) {
			(void) strcpy(servname, retstr);
			return (TRUE);
		}
	}
	return (FALSE);
}
