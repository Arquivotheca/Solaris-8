/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 *
 * Rentrant (MT-safe) getrpcYY interfaces.
 */

#ident	"@(#)getrpcent_r.c	1.8	97/08/01	SMI"

#include <ctype.h>
#include <nss_dbdefs.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpcent.h>
#include <rpc/trace.h>

int str2rpcent(const char *, int, void *,
		char *, int);

static int rpc_stayopen;	/* Unsynchronized, but it affects only	*/
				/*   efficiency, not correctness	*/
static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_rpc(p)
	nss_db_params_t	*p;
{
	trace1(TR__nss_initf_rpc, 0);
	p->name	= NSS_DBNAM_RPC;
	p->default_config = NSS_DEFCONF_RPC;
	trace1(TR__nss_initf_rpc, 1);
}

struct rpcent *
getrpcbyname_r(name, result, buffer, buflen)
	const char	*name;
	struct rpcent	*result;
	char		*buffer;
	int		buflen;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR_getrpcbyname_r, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2rpcent);
	arg.key.name	= name;
	arg.stayopen	= rpc_stayopen;
	res = nss_search(&db_root, _nss_initf_rpc,
		NSS_DBOP_RPC_BYNAME, &arg);
	arg.status = res;
	NSS_XbyY_FINI(&arg);
	trace2(TR_getrpcbyname_r, 1, buflen);
	return (struct rpcent *) arg.returnval;
}

struct rpcent *
getrpcbynumber_r(number, result, buffer, buflen)
	int		number;
	struct rpcent	*result;
	char		*buffer;
	int		buflen;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace3(TR_getrpcbyname_r, 0, number, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2rpcent);
	arg.key.number	= number;
	arg.stayopen	= rpc_stayopen;
	res = nss_search(&db_root, _nss_initf_rpc,
		NSS_DBOP_RPC_BYNUMBER, &arg);
	arg.status = res;
	NSS_XbyY_FINI(&arg);
	trace3(TR_getrpcbyname_r, 1, number, buflen);
	return (struct rpcent *) arg.returnval;
}

void
setrpcent(stay)
	int		stay;
{
	trace1(TR_setrpcent, 0);
	rpc_stayopen |= stay;
	nss_setent(&db_root, _nss_initf_rpc, &context);
	trace1(TR_setrpcent, 1);
}

void
endrpcent()
{
	trace1(TR_endrpcent, 0);
	rpc_stayopen = 0;
	nss_endent(&db_root, _nss_initf_rpc, &context);
	nss_delete(&db_root);
	trace1(TR_endrpcent, 1);
}

struct rpcent *
getrpcent_r(result, buffer, buflen)
	struct rpcent	*result;
	char		*buffer;
	int		buflen;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR_getrpcbyent_r, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2rpcent);
	/* No key, no stayopen */
	res = nss_getent(&db_root, _nss_initf_rpc, &context, &arg);
	arg.status = res;
	NSS_XbyY_FINI(&arg);
	trace2(TR_getrpcbyent_r, 1, buflen);
	return (struct rpcent *) arg.returnval;
}

int
str2rpcent(instr, lenstr, ent, buffer, buflen)
	const char	*instr;
	int		lenstr;
	void	*ent;
	char	*buffer;
	int	buflen;
{
	struct rpcent	*rpc	= (struct rpcent *)ent;
	const char	*p, *numstart, *limit, *namestart;
	ssize_t		numlen, namelen = 0;
	char		numbuf[12];
	char		*numend;

	trace3(TR_str2rpcent, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr)
		|| (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2rpcent, 1, lenstr, buflen);
		return NSS_STR_PARSE_PARSE;
	}

	p = instr;
	limit = p + lenstr;

	while (p < limit && isspace(*p)) {
		p++;
	}
	namestart = p;
	while (p < limit && !isspace(*p)) {
		p++;		/* Skip over the canonical name */
	}
	namelen = p - namestart;

	if (buflen <= namelen) { /* not enough buffer */
		trace3(TR_str2rpcent, 1, lenstr, buflen);
		return NSS_STR_PARSE_ERANGE;
	}
	(void) memcpy(buffer, namestart, namelen);
	buffer[namelen] = '\0';
	rpc->r_name = buffer;

	while (p < limit && isspace(*p)) {
		p++;
	}
	if (p >= limit) {
		/* Syntax error -- no RPC number */
		trace3(TR_str2rpcent, 1, lenstr, buflen);
		return NSS_STR_PARSE_PARSE;
	}
	numstart = p;
	do {
		p++;		/* Find the end of the RPC number */
	} while (p < limit && !isspace(*p));
	numlen = p - numstart;
	if (numlen >= sizeof (numbuf)) {
		/* Syntax error -- supposed number is too long */
		trace3(TR_str2rpcent, 1, lenstr, buflen);
		return NSS_STR_PARSE_PARSE;
	}
	(void) memcpy(numbuf, numstart, numlen);
	numbuf[numlen] = '\0';
	rpc->r_number = (int)strtol(numbuf, &numend, 10);
	if (*numend != '\0') {
		trace3(TR_str2rpcent, 1, lenstr, buflen);
		return NSS_STR_PARSE_PARSE;
	}

	while (p < limit && isspace(*p)) {
		p++;
	}
	/*
	 * Although nss_files_XY_all calls us with # stripped,
	 * we should be able to deal with it here in order to
	 * be more useful.
	 */
	if (p >= limit || *p == '#') { /* no aliases, no problem */
		char **ptr;

		ptr = (char **) ROUND_UP(buffer + namelen + 1,
							sizeof (char *));
		if ((char *)ptr >= buffer + buflen) {
			rpc->r_aliases = 0; /* hope they don't try to peek in */
			trace3(TR_str2rpcent, 1, lenstr, buflen);
			return NSS_STR_PARSE_ERANGE;
		} else {
			*ptr = 0;
			rpc->r_aliases = ptr;
			trace3(TR_str2rpcent, 1, lenstr, buflen);
			return NSS_STR_PARSE_SUCCESS;
		}
	}
	rpc->r_aliases = _nss_netdb_aliases(p, (int)(lenstr - (p - instr)),
			buffer + namelen + 1, (int)(buflen - namelen - 1));
	trace3(TR_str2rpcent, 1, lenstr, buflen);
	return NSS_STR_PARSE_SUCCESS;
}
