/*
 * Copyright (c) 1986-1991,1997-1998,1999, by Sun Microsystems Inc.
 */

#ident	"@(#)clnt_perror.c	1.29	99/07/19 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_perror.c 1.31 89/03/31 Copyr 1984 Sun Micro";
#endif

/*
 * clnt_perror.c
 *
 */

#include "rpc_mt.h"
#ifndef KERNEL
#include <stdio.h>
#include <libintl.h>
#include <string.h>
#endif

#include <rpc/types.h>
#include <rpc/trace.h>
#include <rpc/auth.h>
#include <sys/tiuser.h>
#include <rpc/clnt.h>
#include <stdlib.h>
#include <syslog.h>


extern char *strcpy();
extern char *strcat();
extern char *netdir_sperror();

const char __nsl_dom[]  = "SUNW_OST_NETRPC";

#ifndef KERNEL

static char *
__buf()
{
	char *buf = NULL;
	static char buf_main[512];
	static thread_key_t perror_key;
	extern mutex_t tsd_lock;

	trace1(TR___buf, 0);
	if (_thr_main())
		return (buf_main);
	if (perror_key == 0) {
		mutex_lock(&tsd_lock);
		if (perror_key == 0)
			thr_keycreate(&perror_key, free);
		mutex_unlock(&tsd_lock);
	}
	thr_getspecific(perror_key, (void **) &buf);
	if (buf == NULL) {
		buf = malloc(512);
		if (buf == NULL)
			syslog(LOG_WARNING,
		"clnt_sperror: malloc failed when trying to create buffer\n");
		else
			thr_setspecific(perror_key, (void *) buf);
	}
	trace1(TR___buf, 1);
	return (buf);
}

static char *
auth_errmsg(stat)
	enum auth_stat stat;
{
	trace1(TR_auth_errmsg, 0);
	switch (stat) {
	case AUTH_OK:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Authentication OK"));
	case AUTH_BADCRED:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Invalid client credential"));
	case AUTH_REJECTEDCRED:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Server rejected credential"));
	case AUTH_BADVERF:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Invalid client verifier"));
	case AUTH_REJECTEDVERF:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Server rejected verifier"));
	case AUTH_TOOWEAK:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Client credential too weak"));
	case AUTH_INVALIDRESP:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Invalid server verifier"));
	case AUTH_FAILED:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Failed (unspecified error)"));

	/* kerberos specific */
	case AUTH_DECODE:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Could not decode authenticator"));
	case AUTH_TIMEEXPIRE:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Time of credential expired"));
	case AUTH_TKT_FILE:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom,
			"Something wrong with kerberos ticket file"));
	case AUTH_NET_ADDR:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom,
		"Incorrect network address in kerberos ticket"));
	case AUTH_KERB_GENERIC:
		trace1(TR_auth_errmsg, 1);
		return (dgettext(__nsl_dom, "Kerberos generic error"));
	}
	trace1(TR_auth_errmsg, 1);
	return (dgettext(__nsl_dom, "Unknown authentication error"));
}

/*
 * Return string reply error info. For use after clnt_call()
 */
char *
clnt_sperror(cl, s)
	const CLIENT *cl;
	const char *s;
{
	struct rpc_err e;
	char *err;
	char *str = __buf();
	char *strstart = str;

	trace2(TR_clnt_sperror, 0, cl);
	if (str == NULL) {
		trace2(TR_clnt_sperror, 1, cl);
		return (NULL);
	}
	CLNT_GETERR((CLIENT *) cl, &e);

	(void) sprintf(str, "%s: ", s);
	str += strlen(str);

	(void) strcpy(str, clnt_sperrno(e.re_status));
	str += strlen(str);

	switch (e.re_status) {
	case RPC_SUCCESS:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_TIMEDOUT:
	case RPC_PROGUNAVAIL:
	case RPC_PROCUNAVAIL:
	case RPC_CANTDECODEARGS:
	case RPC_SYSTEMERROR:
	case RPC_UNKNOWNHOST:
	case RPC_UNKNOWNPROTO:
	case RPC_UNKNOWNADDR:
	case RPC_NOBROADCAST:
	case RPC_RPCBFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
		break;

	case RPC_N2AXLATEFAILURE:
		(void) sprintf(str, "; %s", netdir_sperror());
		str += strlen(str);
		break;

	case RPC_TLIERROR:
		(void) sprintf(str, "; %s", t_errlist[e.re_terrno]);
		str += strlen(str);
		if (e.re_errno) {
			(void) sprintf(str, "; %s", strerror(e.re_errno));
			str += strlen(str);
		}
		break;

	case RPC_CANTSEND:
	case RPC_CANTRECV:
		if (e.re_errno) {
			(void) sprintf(str, "; errno = %s",
					strerror(e.re_errno));
			str += strlen(str);
		}
		if (e.re_terrno) {
			(void) sprintf(str, "; %s", t_errlist[e.re_terrno]);
			str += strlen(str);
		}
		break;

	case RPC_VERSMISMATCH:
		(void) sprintf(str, "; low version = %lu, high version = %lu",
				e.re_vers.low, e.re_vers.high);
		str += strlen(str);
		break;

	case RPC_AUTHERROR:
		err = auth_errmsg(e.re_why);
		(void) sprintf(str, "; why = ");
		str += strlen(str);
		if (err != NULL) {
			(void) sprintf(str, "%s", err);
		} else {
			(void) sprintf(str,
				"(unknown authentication error - %d)",
				(int)e.re_why);
		}
		str += strlen(str);
		break;

	case RPC_PROGVERSMISMATCH:
		(void) sprintf(str, "; low version = %lu, high version = %lu",
				e.re_vers.low, e.re_vers.high);
		str += strlen(str);
		break;

	default:	/* unknown */
		(void) sprintf(str, "; s1 = %lu, s2 = %lu",
				e.re_lb.s1, e.re_lb.s2);
		str += strlen(str);
		break;
	}
	trace2(TR_clnt_sperror, 1, cl);
	return (strstart);
}

void
clnt_perror(cl, s)
	const CLIENT *cl;
	const char *s;
{
	trace2(TR_clnt_perror, 0, cl);
	(void) fprintf(stderr, "%s\n", clnt_sperror(cl, s));
	trace2(TR_clnt_perror, 1, cl);
}

void
clnt_perrno(num)
	enum clnt_stat num;
{
	trace1(TR_clnt_perrno, 0);
	(void) fprintf(stderr, "%s\n", clnt_sperrno(num));
	trace1(TR_clnt_perrno, 1);
}

/*
 * Why a client handle could not be created
 */
char *
clnt_spcreateerror(s)
	const char *s;
{
	char *errstr;
	char *str = __buf();

	trace1(TR_clnt_spcreateerror, 0);
	if (str == NULL) {
		trace1(TR_clnt_spcreateerror, 1);
		return (NULL);
	}
	(void) sprintf(str, "%s: ", s);
	(void) strcat(str, clnt_sperrno(rpc_createerr.cf_stat));

	switch (rpc_createerr.cf_stat) {
	case RPC_N2AXLATEFAILURE:
		(void) strcat(str, " - ");
		(void) strcat(str, netdir_sperror());
		break;

	case RPC_RPCBFAILURE:
		(void) strcat(str, " - ");
		(void) strcat(str,
			clnt_sperrno(rpc_createerr.cf_error.re_status));
		break;

	case RPC_SYSTEMERROR:
		(void) strcat(str, " - ");
		errstr = strerror(rpc_createerr.cf_error.re_errno);
		if (errstr != NULL)
			(void) strcat(str, errstr);
		else
			(void) sprintf(&str[strlen(str)], "Error %d",
			    rpc_createerr.cf_error.re_errno);
		break;

	case RPC_TLIERROR:
		(void) strcat(str, " - ");
		if ((rpc_createerr.cf_error.re_terrno > 0) &&
			(rpc_createerr.cf_error.re_terrno < t_nerr)) {
			(void) strcat(str,
				t_errlist[rpc_createerr.cf_error.re_terrno]);
			if (rpc_createerr.cf_error.re_terrno == TSYSERR) {
				char *err;
				err = strerror(rpc_createerr.cf_error.re_errno);
				if (err) {
					strcat(str, " (");
					strcat(str, err);
					strcat(str, ")");
				}
			}
		} else {
			(void) sprintf(&str[strlen(str)],
			    dgettext(__nsl_dom,  "TLI Error %d"),
			    rpc_createerr.cf_error.re_terrno);
		}
		errstr = strerror(rpc_createerr.cf_error.re_errno);
		if (errstr != NULL)
			(void) strcat(str, errstr);
		else
			(void) sprintf(&str[strlen(str)], "Error %d",
			    rpc_createerr.cf_error.re_errno);
		break;

	case RPC_AUTHERROR:
		(void) strcat(str, " - ");
		(void) strcat(str,
			auth_errmsg(rpc_createerr.cf_error.re_why));
		break;
	}
	trace1(TR_clnt_spcreateerror, 1);
	return (str);
}

void
clnt_pcreateerror(s)
	const char *s;
{
	trace1(TR_clnt_pcreateerror, 0);
	(void) fprintf(stderr, "%s\n", clnt_spcreateerror(s));
	trace1(TR_clnt_pcreateerror, 1);
}
#endif /* ! KERNEL */

/*
 * This interface for use by rpc_call() and rpc_broadcast()
 */
const char *
clnt_sperrno(stat)
	const enum clnt_stat stat;
{
	trace1(TR_clnt_sperrno, 0);
	switch (stat) {
	case RPC_SUCCESS:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Success"));
	case RPC_CANTENCODEARGS:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Can't encode arguments"));
	case RPC_CANTDECODERES:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Can't decode result"));
	case RPC_CANTSEND:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Unable to send"));
	case RPC_CANTRECV:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Unable to receive"));
	case RPC_TIMEDOUT:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Timed out"));
	case RPC_VERSMISMATCH:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom,
			"RPC: Incompatible versions of RPC"));
	case RPC_AUTHERROR:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Authentication error"));
	case RPC_PROGUNAVAIL:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Program unavailable"));
	case RPC_PROGVERSMISMATCH:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Program/version mismatch"));
	case RPC_PROCUNAVAIL:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Procedure unavailable"));
	case RPC_CANTDECODEARGS:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom,
			"RPC: Server can't decode arguments"));

	case RPC_SYSTEMERROR:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Remote system error"));
	case RPC_UNKNOWNHOST:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Unknown host"));
	case RPC_UNKNOWNPROTO:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Unknown protocol"));
	case RPC_RPCBFAILURE:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Rpcbind failure"));
	case RPC_N2AXLATEFAILURE:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom,
			"RPC: Name to address translation failed"));
	case RPC_NOBROADCAST:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Broadcast not supported"));
	case RPC_PROGNOTREGISTERED:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Program not registered"));
	case RPC_UNKNOWNADDR:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom,
			"RPC: Remote server address unknown"));
	case RPC_TLIERROR:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Miscellaneous tli error"));
	case RPC_FAILED:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Failed (unspecified error)"));
	case RPC_INPROGRESS:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: RAC call in progress"));
	case RPC_STALERACHANDLE:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Stale RAC handle"));
	case RPC_CANTCONNECT:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Couldn't make connection"));
	case RPC_XPRTFAILED:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom,
			"RPC: Received disconnect from remote"));
	case RPC_CANTCREATESTREAM:
		trace1(TR_clnt_sperrno, 1);
		return (dgettext(__nsl_dom, "RPC: Can't push RPC module"));
	}
	trace1(TR_clnt_sperrno, 1);
	return (dgettext(__nsl_dom, "RPC: (unknown error code)"));
}
