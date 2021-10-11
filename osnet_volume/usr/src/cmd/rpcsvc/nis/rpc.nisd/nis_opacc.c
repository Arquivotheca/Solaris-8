/*
 * Copyright (c) 1998, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nis_opacc.c	1.1	99/03/10 SMI"

#include <stdio.h>
#include <malloc.h>
#include <syslog.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpcsvc/nis.h>
#include "nis_proc.h"


typedef enum {STRING, EMPTY, NIL, NONE} __match_class;

static bool_t		__nis_dir_op_access(char *, char *, nis_name, char *);
static __match_class	__subop_match_class(char *);
static __match_class	__entry_match_class(char *, entry_obj *);

#define	nil(x)		(x)?(x):"<NULL>"

#define	OP_ACC_TABLE	"proto_op_access"
#define	OP_COL		"op"
#define	SUBOP_COL	"subop"
#define	SUBOP_COL_NUM	1
#define	MAX_OPNAME	"NIS_FINDDIRECTORY"
#define	MAX_SUBOPNAME	"Make sure this is longer than the longest tag etc."

/*
 * Verify access to the specified NIS+ protocol operation (and, optionally,
 * sub-operation). Parameters:
 *
 *	op	Required. Name of operation. Example: "NIS_PING".
 *
 *	subop	Optional. Name of sub-operation. Example: "TAG_DEBUG"
 *		for the "NIS_STATUS" operation.
 *
 *	dir	Optional. NIS+ directory for which check is performed.
 *		If NULL, all directories served by this rpc.nisd are
 *		checked.
 *
 *	pname	Optional. Name of principal. If NULL, this routine
 *		derives the principal name from the reqstp argument.
 *
 *	reqstp	Optional and ignored unless pname == NULL. The RPC
 *		request.
 */
bool_t
nis_op_access(char *op, char *subop, nis_name dir, char *pname,
		struct svc_req *reqstp) {

	char		pnamebuf[1024];
	char		*dirl;


	/* No check at security levels 0 and 1 */
	if (secure_level < 2)
		return (TRUE);

	/* Sanity check arguments */
	if (op == 0 ||
		(pname == 0 && reqstp == 0) ||
		(strlen(op) > sizeof (MAX_OPNAME)) ||
		(subop != 0 && strlen(subop) > sizeof (MAX_SUBOPNAME)))
		return (FALSE);

	/* Get the principal name */
	if (pname == 0) {
		pname = pnamebuf;
		nis_getprincipal(pname, reqstp);
	}

#ifdef	OPACCDEBUG
	printf("nis_op_access(%s, %s, %s, %s, 0x%x)\n",
	       op, (subop!=0)?subop:"<NULL>", (dir!=0)?dir:"<NULL>", 
	       pname, reqstp);
#endif /* OPACCDEBUG */

	if (dir != 0 && dir[0] != '\0') {
		return (__nis_dir_op_access(op, subop, dir, pname));
	} else {
		char		*curdir, *nxtdir;

		if (nis_server_control(SERVING_LIST, DIR_GETLIST, &dirl) == 0)
			return (FALSE);

#ifdef	OPACCDEBUG
		printf("serving list = \"%s\"\n", dirl);
#endif	/* OPACCDEBUG */

		for (curdir = nxtdir = dirl; *curdir != '\0'; curdir = nxtdir) {
			while (*nxtdir != ' ' && *nxtdir != '\0')
				nxtdir++;
			if (*nxtdir != '\0')
				*nxtdir++ = '\0';
			if (!__nis_dir_op_access(op, subop, curdir, pname)) {
				free(dirl);
				return (FALSE);
			}
		}
		free(dirl);
	}

	return (TRUE);
}


static
bool_t __nis_dir_op_access(char *op, char *subop, nis_name dir, char *pname) {

	char		lookup[NIS_MAXNAMELEN +		/* dir name */
				sizeof (OP_ACC_TABLE) +	/* table name */
				sizeof (MAX_OPNAME) +	/* search ... */
				sizeof (MAX_SUBOPNAME)+	/* ... criteria */
				sizeof ("[=,=]. ")];	/* syntax + NUL */
	ib_request	req;
	nis_error	err;
	nis_result	*res;
	int		i;
	__match_class	required_match, best_entry, be_match, tmp;
	bool_t		ret = TRUE;


	if (subop == 0)
		sprintf(lookup, "[%s=%s]%s.%s", OP_COL, op, OP_ACC_TABLE, dir);
	else
		sprintf(lookup, "[%s=%s,%s=%s]%s.%s",
			OP_COL, op, SUBOP_COL, subop, OP_ACC_TABLE, dir);

	err = nis_get_request(lookup, 0, 0, &req);
	if (err != NIS_SUCCESS)
		return (TRUE);

	res = __nis_local_lookup(&req, 0, 1, 0, 0);

	nis_free_request(&req);

#ifdef	OPACCDEBUG
	printf("nis_local_lookup(%s) => 0x%x, status = %d\n",
	       lookup, res, (res!=0)?res->status:-1);
#endif /* OPACCDEBUG */

	/* No result at all or no such table => assume access OK */
	if (res == 0)
		return (TRUE);
	else if (res->status == NIS_NOSUCHTABLE) {
		nis_freeresult(res);
		return (TRUE);
	}

	/*
	 * If we didn't find any entries, then one of two situations apply:
	 *
	 * (1)	We were looking for 'op' only, and if it isn't in the
	 *	the table, we allow access.
	 *
	 * (2)	We were looking for both 'op' and 'subop'. It's possible
	 *	that there's an entry for 'op' only, so try again.
	 */
	if (res->status == NIS_NOTFOUND || res->status == NIS_PARTIAL) {
		nis_freeresult(res);
		if (subop == 0) {
			return (TRUE);
		} else {
			return (__nis_dir_op_access(op, 0, dir, pname));
		}
	} else if (res->status != NIS_SUCCESS) {
		/*
		 * XXX Should we succeed or fail ?
		 * For maximum backward compatibility, we declare success
		 */
#ifdef	OPACCDEBUG
		for (i = 0; i < NIS_RES_NUMOBJ(res); i++) {
			printf("\t------------- %d --------------\n", i);
			nis_print_object(&(NIS_RES_OBJECT(res)[i]));
		}
#endif /* OPACCDEBUG */
		nis_freeresult(res);
		return (TRUE);
	}

	/*
	 * If there was more than one result, look for one that:
	 *
	 *	(1)	has a matching sub-operation, or
	 *
	 *	(2)	has an empty string in the subop field, or
	 *
	 *	(3)	has a NIL subop field, or
	 *
	 *	(4)	has no subop field
	 *
	 * in that order.
	 */
	if (NIS_RES_NUMOBJ(res) == 1) {
#ifdef	OPACCDEBUG
		printf("\tone matching entry\n");
#endif /* OPACCDEBUG */
		ret = __can_do(NIS_READ_ACC, NIS_RES_OBJECT(res)->zo_access,
				NIS_RES_OBJECT(res), pname);
	} else if (NIS_RES_NUMOBJ(res) > 1) {
		required_match	= __subop_match_class(subop);
		best_entry	= 0;
		be_match	= __entry_match_class(subop,
					&(NIS_RES_OBJECT(res)->EN_data));
#ifdef	OPACCDEBUG
		printf("\t%d matching entries\n", NIS_RES_NUMOBJ(res));
#endif /* OPACCDEBUG */
		for (i = 1; i < NIS_RES_NUMOBJ(res); i++) {
#ifdef	OPACCDEBUG
			printf("\t%s\n", NIS_RES_OBJECT(res)[i].zo_name);
#endif /* OPACCDEBUG */
			tmp = __entry_match_class(subop,
				&(NIS_RES_OBJECT(res)[i].EN_data));
			if (tmp <= be_match) {
				be_match = tmp;
				best_entry = i;
				if (be_match <= required_match)
					break;
			}
		}
#ifdef	OPACCDEBUG
		printf("\trequired_match = %d, be_match = %d, best_entry = %d\n",
		       required_match, be_match, best_entry);
		printf("\t\t%s %s\n",
		       ENTRY_VAL(&(NIS_RES_OBJECT(res)[best_entry]), 0),
		       nil(ENTRY_VAL(&(NIS_RES_OBJECT(res)[best_entry]), 1)));
#endif /* OPACCDEBUG */
		ret = __can_do(NIS_READ_ACC,
				NIS_RES_OBJECT(res)[best_entry].zo_access,
				&(NIS_RES_OBJECT(res)[best_entry]), pname);
	}

	nis_freeresult(res);
#ifdef	OPACCDEBUG
	printf("\t%s\n", ret?"OK":"NO ACCESS");
#endif	/* OPACCDEBUG */
	return (ret);
}


/* Map callback child pid to anonymous number */
static callback_pid	*callback_pid_list = NULL;
static anonid_t		callback_anonid = 0;

/*
 * Add pid to callback list
 */
anonid_t
nis_add_callback_pid(pid_t pid, nis_name pname) {

	callback_pid	*entry = malloc(sizeof(callback_pid));
	callback_pid	*cp;

	if (entry == NULL) {
		syslog(LOG_WARNING, "nis_add_callback_pid: unable to malloc");
		return(NOANONID);
	}

	entry->pid	= pid;
	entry->anonid	= callback_anonid++;
	entry->pname	= strdup(pname);

	cp = callback_pid_list;
	entry->next = cp;
	callback_pid_list = entry;

	return(entry->anonid);
}

/*
 * Remove callback pid
 */
void
nis_delete_callback_pid(pid_t pid) {

	callback_pid	*entry, **prev;

	if (pid == NOPID)
		return;

	for (prev = &callback_pid_list, entry = *prev; entry != NULL;
	     prev = &(entry->next), entry = entry->next) {
		if (entry->pid == pid) {
			*prev = entry->next;
			free(entry->pname);
			free(entry);
			break;
		}
	}

	return;
}

/*
 * Return the pid and principal name corresponding to an anonymous id
 */
pid_t
nis_get_callback_pid(anonid_t anonid, nis_name pname, int pnamelen) {

	callback_pid	*entry;
	pid_t		pid = NOPID;

	for (entry = callback_pid_list; entry != NULL; entry = entry->next) {
		if (entry->anonid == anonid) {
			pid = entry->pid;
			if (pname != 0) {
				strncpy(pname, entry->pname, pnamelen);
			}
			break;
		}
	}

	return(pid);
}

/* Classify a subop string */
__match_class
__subop_match_class(char *subop) {

	if (subop == 0) {
#ifdef	OPACCDEBUG
		printf("\t\t\t<NIL>\n");
#endif /* OPACCDEBUG */
		return (NIL);
	} else if (*subop == '\0') {
#ifdef	OPACCDEBUG
		printf("\t\t\t<EMPTY>\n");
#endif /* OPACCDEBUG */
		return (EMPTY);
	} else {
#ifdef	OPACCDEBUG
		printf("\t\t\t%s\n", subop);
#endif /* OPACCDEBUG */
		return (STRING);
	}
}

/*
 * Classify the subop in an entry. If it's a string, compare to the
 * subop argument.
 */
__match_class
__entry_match_class(char *subop, entry_obj *entry) {

	__match_class	ret;
	char		*entry_subop;

	if (entry == 0 || entry->en_cols.en_cols_len <= SUBOP_COL_NUM) {
		ret = NONE;
	} else {
		entry_subop =
	entry->en_cols.en_cols_val[SUBOP_COL_NUM].ec_value.ec_value_val;
		ret = __subop_match_class(entry_subop);
		if (ret == STRING &&
			(subop == 0 || strcmp(subop, entry_subop) != 0))
			ret = NONE;
	}
	return (ret);
}
