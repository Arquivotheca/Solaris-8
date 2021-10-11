/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 *
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 */

#ident	"@(#)file_db.c	1.11	98/08/06 SMI"

/*
 * This is the C library "getXXXbyYYY" routines after they have been stripped
 * down to using just the file /etc/hosts and /etc/services. When linked with
 * the internal name to address resolver routines for TCP/IP they provide
 * the file based version of those routines.
 *
 * This file defines gethostbyname(), gethostbyaddr(), getservbyname(),
 * and getservbyport(). The C library routines are not used as they may
 * one day be based on these routines, and that would cause recursion
 * problems.
 *
 * ==== This implementation returns pointers to static data, and hence is
 *	MT-very-unsafe, but we serialize all requests (in tcpip.c) and
 *	don't drop the lock until we're done with the data.
 *
 *	A better approach would be to reimplement this with _r interfaces
 *	(preferably finding ways to use the code in libnsl and nss_files.so
 *	rather than duplicating it here).
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netdir.h>
#include <netinet/in.h>

char *malloc(), *calloc();

#define	MAXADDRSIZE	14

static struct hostdata {
	FILE	*hostf;
	char	*current;
	int	currentlen;
	int	stayopen;
	char	*host_aliases[MAXALIASES];
	char	hostaddr[MAXADDRSIZE][MAXADDRS];
	char	*addr_list[MAXADDRS+1];
	char	line[BUFSIZ+1];
	struct	hostent host;
} *hostdata, *_hostdata();

static struct hostent *_gethostent(), *he_interpret();
static char HOSTDB[] = "/etc/hosts";

static struct servdata {
	FILE	*servf;
	char	*current;
	int	currentlen;
	int	stayopen;
	char	*serv_aliases[MAXALIASES];
	struct	servent serv;
	char	line[BUFSIZ+1];
} *servdata, *_servdata();

static struct servent *_getservent(), *se_interpret();
static char SERVDB[] = "/etc/services";

static struct hostdata *
_hostdata()
{
	register struct hostdata *d = hostdata;

	if (d == 0) {
		d = (struct hostdata *)calloc(1, sizeof (struct hostdata));
		hostdata = d;
	}
	return (d);
}

struct hostent *
_files_gethostbyname(name)
	register char *name;
{
	register struct hostdata *d = _hostdata();
	register struct hostent *p;
	register char **cp;

	if (d == 0)
		return ((struct hostent*)NULL);

	if (((int)inet_addr(name)) != -1) { /* parse 1.2.3.4 case */
		sprintf(d->line, "%s %s\n", name, name);
		return (he_interpret());
	}

	if (strcmp(name, HOST_ANY) == 0)
		return ((struct hostent *)NULL);

	sethostent(0);
	while (p = _gethostent()) {
		if (strcasecmp(p->h_name, name) == 0) {
			break;
		}
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				break;
		if (*cp)
			break;	/* We found it */
	}

	endhostent();
	return (p);
}

static int
sethostent(f)
	int f;
{
	register struct hostdata *d = _hostdata();

	if (d == 0)
		return;
	if (d->hostf == NULL)
		d->hostf = fopen(HOSTDB, "r");
	else
		rewind(d->hostf);
	if (d->current)
		free(d->current);
	d->current = NULL;
	d->stayopen |= f;
}

static int
endhostent()
{
	register struct hostdata *d = _hostdata();

	if (d == 0)
		return;
	if (d->current && !d->stayopen) {
		free(d->current);
		d->current = NULL;
	}
	if (d->hostf && !d->stayopen) {
		fclose(d->hostf);
		d->hostf = NULL;
	}
}

static struct hostent *
_gethostent()
{
	register struct hostdata *d = _hostdata();

	if (d->hostf == NULL && (d->hostf = fopen(HOSTDB, "r")) == NULL)
		return (NULL);
	if (fgets(d->line, BUFSIZ, d->hostf) == NULL)
		return (NULL);
	return (he_interpret());
}

/*
 * This routine interprets the current line.
 */
static struct hostent *
he_interpret()
{
	register struct hostdata *d = _hostdata();
	char *p;
	register char *cp, **q;

	if (d == 0)
		return (0);

	p = d->line;
	/* Check for comment lines (start with # mark) */
	if (*p == '#')
		return (_gethostent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (_gethostent());
	*cp = '\0'; /* Null out any trailing comment */

	/* Now check for whitespace */
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (_gethostent());
	*cp++ = '\0'; /* This breaks the line into name/address components */

	/* return one address */
	d->addr_list[0] = (d->hostaddr[0]);
	d->addr_list[1] = NULL;
	d->host.h_addr_list = d->addr_list;
	*((in_addr_t *)d->host.h_addr) = inet_addr(p);
	d->host.h_length = NS_INADDRSZ;
	d->host.h_addrtype = AF_INET;

	/* skip whitespace between address and the name */
	while (*cp == ' ' || *cp == '\t')
		cp++;
	d->host.h_name = cp;
	q = d->host.h_aliases = d->host_aliases;

	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';

	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(d->host_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&d->host);
}

struct hostent *
_files_gethostbyaddr(addr, len, type)
	char *addr;
	int len, type;
{
	register struct hostdata *d = _hostdata();
	register struct hostent *p;

	if (d == 0)
		return ((struct hostent*)NULL);
	sethostent(0);
	while (p = _gethostent()) {
		if (p->h_addrtype != type || p->h_length != len)
			continue;
		if (memcmp(p->h_addr_list[0], addr, len) == 0)
			break;
	}
	endhostent();
	return (p);
}

/*
 * The services routines. These nearly identical to the host routines
 * above. The Interpret routine differs. Seems there should be some way
 * to fold this code together.
 */

static struct servdata *
_servdata()
{
	register struct servdata *d = servdata;

	if (d == 0) {
		d = (struct servdata *)calloc(1, sizeof (struct servdata));
		servdata = d;
	}
	return (d);
}

struct servent *
_files_getservbyport(svc_port, proto)
	int svc_port;
	char *proto;
{
	register struct servdata *d = _servdata();
	register struct servent *p = NULL;
	register u_short port = svc_port;

	if (d == 0)
		return (0);

	setservent(0);
	while (p = _getservent()) {
		if (p->s_port != port)
			continue;
		if (proto == 0 || strcasecmp(p->s_proto, proto) == 0)
			break;
	}
	endservent();
	return (p);
}

struct servent *
_files_getservbyname(name, proto)
	register char *name, *proto;
{
	register struct servdata *d = _servdata();
	register struct servent *p;
	register char **cp;

	if (d == 0)
		return (0);
	setservent(0);
	while (p = _getservent()) {
		if (proto != 0 && strcasecmp(p->s_proto, proto) != 0)
			continue;
		if (strcasecmp(name, p->s_name) == 0)
			break;
		for (cp = p->s_aliases; *cp; cp++)
			if (strcasecmp(name, *cp) == 0)
				break;
		if (*cp)
			break;	/* we found it */
	}
	endservent();
	return (p);
}

static int
setservent(f)
	int f;
{
	register struct servdata *d = _servdata();

	if (d == 0)
		return;
	if (d->servf == NULL)
		d->servf = fopen(SERVDB, "r");
	else
		rewind(d->servf);
	if (d->current)
		free(d->current);
	d->current = NULL;
	d->stayopen |= f;
}

static int
endservent()
{
	register struct servdata *d = _servdata();

	if (d == 0)
		return;
	if (d->current && !d->stayopen) {
		free(d->current);
		d->current = NULL;
	}
	if (d->servf && !d->stayopen) {
		fclose(d->servf);
		d->servf = NULL;
	}
}

static struct servent *
_getservent()
{
	register struct servdata *d = _servdata();

	if (d == 0)
		return (NULL);

	if (d->servf == NULL && (d->servf = fopen(SERVDB, "r")) == NULL)
		return (NULL);

	if (fgets(d->line, BUFSIZ, d->servf) == NULL)
		return (NULL);

	return (se_interpret());
}

static struct servent *
se_interpret()
{
	register struct servdata *d = _servdata();
	char *p;
	register char *cp, **q;

	if (d == 0)
		return (0);

	p = d->line;
	if (*p == '#')
		return (_getservent());

	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (_getservent());
	*cp = '\0';

	d->serv.s_name = p;
	p = strpbrk(p, " \t");
	if (p == NULL)
		return (_getservent());
	*p++ = '\0';

	while (*p == ' ' || *p == '\t')
		p++;
	cp = strpbrk(p, ",/");
	if (cp == NULL)
		return (_getservent());
	*cp++ = '\0';
	d->serv.s_port = htons((u_short)atoi(p));
	d->serv.s_proto = cp;
	q = d->serv.s_aliases = d->serv_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(d->serv_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&d->serv);
}
