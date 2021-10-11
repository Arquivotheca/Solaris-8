/*
 * Copyright (c) 1986-1992,1997 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ether_addr.c	1.18	97/08/22 SMI"	/* SVr4.0 1.1 */

/*
 * All routines necessary to deal the "ethers" database.  The sources
 * contain mappings between 48 bit ethernet addresses and corresponding
 * hosts names.  The addresses have an ascii representation of the form
 * "x:x:x:x:x:x" where x is a hex number between 0x00 and 0xff;  the
 * bytes are always in network order.
 */

#ident	"@(#)ether_addr.c	1.18	97/08/22	SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <thread.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <nss_dbdefs.h>

extern int _thr_main();

static int str2ether(const char *, int, void *, char *, int);

static DEFINE_NSS_DB_ROOT(db_root);

static void
_nss_initf_ethers(nss_db_params_t *p)
{
	p->name = NSS_DBNAM_ETHERS;
	p->default_config = NSS_DEFCONF_ETHERS;
}

/*
 * Given a host's name, this routine finds the corresponding 48 bit
 * ethernet address based on the "ethers" policy in /etc/nsswitch.conf.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_hostton(
	const char *host,		/* function input */
	ether_addr_t e	/* function output */
)
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	/*
	 * let the backend do the allocation to store stuff for parsing.
	 */
	NSS_XbyY_INIT(&arg, e, NULL, 0, str2ether);
	arg.key.name = host;
	res = nss_search(&db_root, _nss_initf_ethers,
			NSS_DBOP_ETHERS_HOSTTON, &arg);
	(void) NSS_XbyY_FINI(&arg);
	return (arg.status = res);
}

/*
 * Given a 48 bit ethernet address, it finds the corresponding hostname
 * ethernet address based on the "ethers" policy in /etc/nsswitch.conf.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_ntohost(
	char *host,		/* function output */
	ether_addr_t e		/* function input */
)
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	/*
	 * let the backend do the allocation to store stuff for parsing.
	 */
	NSS_XbyY_INIT(&arg, NULL, host, 0, str2ether);
	arg.key.ether = e;
	res = nss_search(&db_root, _nss_initf_ethers,
			NSS_DBOP_ETHERS_NTOHOST, &arg);
	/* memcpy(host, ether_res.host, strlen(ether_res.host)); */
	(void) NSS_XbyY_FINI(&arg);
	return (arg.status = res);
}

/*
 * Parses a line from "ethers" database into its components.  The line has
 * the form 8:0:20:1:17:c8	krypton
 * where the first part is a 48 bit ethernet addrerss and the second is
 * the corresponding hosts name.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_line(
	const char *s,		/* the string to be parsed */
	ether_addr_t e,		/* ethernet address struct to be filled in */
	char *hostname		/* hosts name to be set */
)
{
	int i;
	unsigned int t[6];

	i = sscanf(s, " %x:%x:%x:%x:%x:%x %s",
	    &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], hostname);
	if (i != 7) {
		return (7 - i);
	}
	for (i = 0; i < 6; i++)
		e[i] = (u_char) t[i];
	return (0);
}

/*
 * Parses a line from "ethers" database into its components.
 * Useful for the wile purposes of the backends that
 * expect a str2ether() format.
 *
 * This function, after parsing the instr line, will
 * place the resulting ether_addr_t in b->buf.result only if
 * b->buf.result is initialized (not NULL). I.e. it always happens
 * for "files" backend (that needs to parse input line and
 * then do a match for the ether key) and happens for "nis"
 * backend only if the call was ether_hostton.
 *
 * Also, it will place the resulting hostname into b->buf.buffer
 * only if b->buf.buffer is initialized. I.e. it always happens
 * for "files" backend (that needs to parse input line and
 * then do a match for the host key) and happens for "nis"
 * backend only if the call was ether_ntohost.
 *
 * Cannot use the sscanf() technique for parsing because instr
 * is a read-only, not necessarily null-terminated, buffer.
 *
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
#define	DIGIT(x)	(isdigit(x) ? (x) - '0' : \
		islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
#define	lisalnum(x)	(isdigit(x) || \
		((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))
/* ARGSUSED */
static int
str2ether(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	u_char	*ether =  (u_char *)ent;
	char	*host = buffer;
	const char	*p, *limit, *start;
	ptrdiff_t i;

	p = instr;
	limit = p + lenstr;

	/* skip beginning whitespace, if any */
	while (p < limit && isspace(*p))
		p++;

	if (ether) {	/* parse ether */
		for (i = 0; i < 6; i++) {
			int	j = 0, n = 0;

			start = p;
			while (p < limit && lisalnum(start[j])) {
				/* don't worry about overflow here */
				n = 16 * n + DIGIT(start[j]);
				j++;
				p++;
			}
			if (*p != ':' && i < 5) {
				return (NSS_STR_PARSE_PARSE);
			} else {
				p++;
				*(ether + i) = (u_char)n;
			}
		}
	} else {	/* skip ether */
		while (p < limit && !isspace(*p))
			p++;
	}
	if (host) {	/* parse host */
		while (p < limit && isspace(*p))	/* skip whitespace */
			p++;
		start = p;
		while (p < limit && !isspace(*p))	/* skip hostname */
			p++;
		if ((i = (p - start)) < MAXHOSTNAMELEN) {
			(void) memcpy(host, start, i);
			host[i] = '\0';
		} else
			return (NSS_STR_PARSE_ERANGE); /* failure */
	}
	return (NSS_STR_PARSE_SUCCESS);
}

static mutex_t tsd_lock = DEFAULTMUTEX;

/*
 * Converts a 48 bit ethernet number to its string representation.
 */
char *
ether_ntoa(const ether_addr_t e)
{
	char *s = NULL;
	static char s_main[18];
	static thread_key_t ntoa_key;

	if (_thr_main()) {
		s = s_main;
	} else {
		if (ntoa_key == 0) {
			mutex_lock(&tsd_lock);
			if (ntoa_key == 0)
				thr_keycreate(&ntoa_key, free);
			mutex_unlock(&tsd_lock);
		}
		thr_getspecific(ntoa_key, (void **)&s);
		if (s == NULL) {
			s = (char *)malloc((unsigned)18);
			if (s == NULL) {
				return (NULL);
			}
			thr_setspecific(ntoa_key, (void *)s);
		}
	}
	s[0] = 0;
	(void) sprintf(s, "%x:%x:%x:%x:%x:%x",
		e[0], e[1], e[2], e[3], e[4], e[5]);
	return (s);
}

/*
 * Converts a ethernet address representation back into its 48 bits.
 */
ether_addr_t *
ether_aton(const char *s)
{
	ether_addr_t *e = NULL;
	static ether_addr_t e_main;
	static thread_key_t aton_key;
	int i;
	unsigned int t[6];

	if (_thr_main())
		e = &e_main;
	else {
		if (aton_key == 0) {
			mutex_lock(&tsd_lock);
			if (aton_key == 0)
				thr_keycreate(&aton_key, free);
			mutex_unlock(&tsd_lock);
		}
		thr_getspecific(aton_key, (void **)&e);
		if (e == NULL) {
			e = malloc(sizeof (ether_addr_t));
			if (e == NULL)
				return (NULL);
			thr_setspecific(aton_key, (void *)e);
		}
	}

	i = sscanf(s, " %x:%x:%x:%x:%x:%x",
	    &t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
	if (i != 6)
	    return (NULL);
	for (i = 0; i < 6; i++)
		(*e)[i] = (u_char)t[i];
	return (e);
}
