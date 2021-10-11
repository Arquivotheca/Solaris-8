/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dr_ap.c	1.28	99/06/04 SMI"

/*
 * This file implements the Dynamic Reconfiguration <-> Alternate Path
 * interfaces.
 */

#include <sys/utsname.h>
#include <malloc.h>
#include <string.h>
#include <dlfcn.h>
#include "dr_subr.h"
#include "dr_ap.h"

#ifdef AP

/*
 * This table contains the names of the libap.so functions and variables
 * which are needed for DR & AP interaction.  We use dlopen() to open
 * libap.so on the fly and dlsym() to find the addresses of these routines
 * and variables.  We then access libap by dereferencing these pointers.
 *
 * If we were to directly link dr_daemon with libap.so then dr_daemon would
 * require that libap.so be present in order for the dr_daemon executable
 * to load, even if we never accessed the AP code.  Having these static
 * dependencies between DR and AP is undesirable from a software packaging
 * point of view.
 *
 * By default, during initialization the dr_daemon attempts to open libap.so
 * If it is present, the dr_daemon communicates with the ap_daemon during
 * DR attach, detach, and info display requests.  If libap.so cannot be
 * opened, the dr_daemon operations in noapdaemon mode.  This noapdaemon
 * mode may also be made the default by specifying the -a flag when
 * dr_daemon is invoked.
 */

static struct {
	char	*apname;
	void	*apaddr;
} libapsyms[] = {
	{"_ap_host", 			NULL},
	{"ap_format_error",		NULL},
	{"ap_set_error_handler",	NULL},
	{"ap_strerror", 		NULL},
	{"apd_attach",			NULL},
	{"apd_db_query",		NULL},
	{"apd_detach_complete",		NULL},
	{"apd_detach_start",		NULL},
	{"apd_dr_query",		NULL},
	{"apd_drain",			NULL},
	{"apd_resume",			NULL},
	{"ap_init_rpc",			NULL},
	{"xdr_dr_arg",			NULL},
	{"xdr_dr_query_arg",		NULL},
	{"xdr_db_query",		NULL},
	{"xdr_dr_query",		NULL},
	{"apd_pathgroup_reset",		NULL},
	{NULL,				NULL}	/* end of the table */
};

/*
 * Typedefs needed to define the macros for libap calls
 */
typedef	int	(*apd_intfunc_t)();
typedef	void	(*apd_voidfunc_t)();
typedef	char 	*(*apd_charpfunc_t)();
typedef	db_query	(*apd_db_queryfunc_t)();
typedef	dr_query	(*apd_dr_queryfunc_t)();

/*
 * If this is not a LINT compile, define the AP subroutines and variables
 * to be accessed via their addresses which we have determined via the
 * dlsym() calls.  Note that these defines MUST be in the same order as
 * defined in the libapsyms table above.
 *
 * If this _is_ a lint compile, we don't want to override the ap_lib.h
 * definitions of these routines since we want to check that the function
 * prototypes defined in ap_lib.h match what we've got here.
 *
 * When compiling with AP_TEST enabled, we're overriding almost all the
 * libap.so routines (except for the xdr routines) in this modules so we
 * don't want these defines enabled either.
 */
#ifndef lint
#define	_ap_host		(*(char **)libapsyms[0].apaddr)
#define	ap_format_error		(*(apd_voidfunc_t)libapsyms[1].apaddr)
#define	ap_set_error_handler	(*(apd_voidfunc_t)libapsyms[2].apaddr)
#endif /* !lint */
#if !defined(AP_TEST) && !defined(lint)
#define	ap_strerror		(*(apd_charpfunc_t)libapsyms[3].apaddr)
#define	apd_attach		(*(apd_intfunc_t)libapsyms[4].apaddr)
#define	apd_db_query		(*(apd_db_queryfunc_t)libapsyms[5].apaddr)
#define	apd_detach_complete	(*(apd_intfunc_t)libapsyms[6].apaddr)
#define	apd_detach_start	(*(apd_intfunc_t)libapsyms[7].apaddr)
#define	apd_dr_query		(*(apd_dr_queryfunc_t)libapsyms[8].apaddr)
#define	apd_drain		(*(apd_intfunc_t)libapsyms[9].apaddr)
#define	apd_resume		(*(apd_intfunc_t)libapsyms[10].apaddr)
#define	ap_init_rpc		(*(apd_intfunc_t)libapsyms[11].apaddr)
#endif /* !defined (AP_TEST) && !defined(lint) */

#ifndef lint
#define	xdr_dr_arg		(*(xdrproc_t)libapsyms[12].apaddr)
#define	xdr_dr_query_arg	(*(xdrproc_t)libapsyms[13].apaddr)
#define	xdr_db_query		(*(xdrproc_t)libapsyms[14].apaddr)
#define	xdr_dr_query		(*(xdrproc_t)libapsyms[15].apaddr)
#define	apd_pathgroup_reset	(*(apd_intfunc_t)libapsyms[16].apaddr)
#endif /* !lint */

/* forward references */
static int init_ap_rpc(void);
static void print_alt_names(char *routine, int count, ctlr_t *ctlp);

#endif /* AP */

/*
 * dr_ap_init
 *
 * Use dlopen to dynamically load libap.so and then find all the
 * symbols addresses which DR needs to do it's work.  Failures are
 * reported via dr_info and future AP communication is disabled
 * by setting the noapdaemon flag.
 *
 * Note that we use dr_logerr() here to report errors.  This routine
 * is called before the default syslog() attributes are setup via
 * openlog() in dr_daemon_main.c.  Even though dr_logerr() sets the
 * dr_err global, since we are called before any RPC calls are
 * accetped, this error won't get passed back to our callers.  It will
 * just get logged to the console and /var/adm/messages file (assuming
 * the syslog file is setup correctly).
 */
void
dr_ap_init(void)
{
#ifdef AP
	void	*handle;
	void	*apaddr;
	int	i;

	if (verbose)
		dr_loginfo("dr_daemon attempting AP interaction");

	if ((handle = dlopen("libap.so", RTLD_LAZY)) == NULL) {
		dr_logerr(DRV_FAIL, 0, dlerror());
		dr_logerr(DRV_FAIL, 0,
			"dr_daemon operating in NO AP interaction mode");
		noapdaemon = 1;
		return;
	}

	for (i = 0; libapsyms[i].apname != NULL; i++) {

		apaddr = dlsym(handle, libapsyms[i].apname);
		if (apaddr == NULL) {
			dr_logerr(DRV_FAIL, 0, dlerror());
			dr_logerr(DRV_FAIL, 0,
			"dr_daemon operating in NO AP interaction mode");
			noapdaemon = 1;
			return;
		}

		libapsyms[i].apaddr = apaddr;
	}

	if (verbose)
		dr_loginfo("dr_daemon successfully initialized AP interaction");
#endif /* AP */
}

/*
 * dr_ap_notify
 *
 * Tell the AP daemon about changes to the board state for controllers
 * on the board.
 *
 * Input:
 *	board
 *	state	current ones recognized are:
 *		DR_DRAIN - start of drain operation
 *		DR_DETACH_IP - start of detach operation
 *		DR_OS_DETACHED - detach operation complete
 *		DR_IN_USE - detach operation aborted (resumed)
 *		DR_OS_ATTACHED - attach operation complete
 */
void
dr_ap_notify(int board, dr_board_state_t state)
{
#ifdef AP
	ctlr_t		*ptr_ctlrs;
	static dr_arg	drap;
	int		ret;
	const char	*errstr;

	/* If comm setup fails, return */
	if (init_ap_rpc())
		return;

	/*
	 * Calls with state DR_DETACH_IP and DR_OS_DETACHED are paired
	 * calls in that the first call sets up and saves the
	 * controllers on the board in the drap structures.  The
	 * second call, DR_OS_DETACHED, uses this saved list of
	 * controllers.  The caller with these two states must be sure
	 * that DR_DETACH_IP and DR_OS_DETACHED are always called in that
	 * order.
	 */
	if (state != DR_OS_DETACHED) {

		/* Release old malloc'd memory if we have any */
		xdr_free(xdr_dr_arg, (char *)&drap);

		/* Get the list of potential AP controllers */
		drap.ctlrs.ctlrs_len = dr_controller_names(board, &ptr_ctlrs);
		drap.ctlrs.ctlrs_val = ptr_ctlrs;
	}

	print_alt_names("dr_ap_notify", drap.ctlrs.ctlrs_len,
			drap.ctlrs.ctlrs_val);

	switch (state) {
	case DR_OS_ATTACHED:
		ret = apd_attach(&drap);
		/* force AP to update it's flags just to make sure */
		ret = apd_pathgroup_reset();
		break;
	case DR_DRAIN:
		ret = apd_drain(&drap);
		break;
	case DR_DETACH_IP:
		ret = apd_detach_start(&drap);
		break;
	case DR_OS_DETACHED:
		ret = apd_detach_complete(&drap);
		break;
	case DR_IN_USE:
		ret = apd_resume(&drap);
		/* force AP to update it's flags just to make sure */
		ret = apd_pathgroup_reset();
		break;
	default:
		dr_loginfo("dr_ap_notify: unknown state %d\n", state);
		break;
	}

	if (ret) {
		errstr = ap_strerror(ret, 1);
		if (errstr != NULL)
			dr_loginfo("AP daemon call failed: %s\n",
				    errstr);
		else
			dr_loginfo("AP daemon call failed: error = %d\n",
				    ret);
	}
#endif /* AP */
}

#ifdef AP
/*
 * print_alt_names
 *
 * Print a loginfo message saying which controllers we're calling
 * the apd with.
 */
static void
print_alt_names(char *routine, int count, ctlr_t *ctlp)
{
	char	message[1024];
	char	instance[40];
	int	i;

	sprintf(message, "%s: Calling apd with:", routine);

	for (i = 0; i < count; i++) {
		strncat(message, " ", sizeof (message));
		strncat(message, ctlp[i].name, sizeof (message));
		sprintf(instance, "%d", ctlp[i].instance);
		strncat(message, instance, sizeof (message));
	}
	strncat(message, "\n", sizeof (message));

	if (verbose)
		dr_loginfo(message);
}


/*
 * drap_error_handler
 *
 * Error handler for messages coming out of aplib().  Format the
 * message and then pass it only dr_loginfo.  Since AP errors are not
 * fatal, we don't want to pass them back to the application as would be
 * done if we called dr_logerr.  Logging these messages via syslog should
 * be sufficient.
 */
static void
drap_error_handler(int class, char *proc, char *format)
{
	char	ap_error_buf[MAXMSGLEN];

	/* place message in error buffer, from ap_error.c */
	ap_format_error(ap_error_buf, "libap", proc, class, format);

	dr_loginfo(ap_error_buf);
}

/*
 * init_ap_rpc
 *
 * The AP daemon resides on the same host as the dr_daemon.  _ap_host
 * is an ap_lib global which must be set to the host where the AP
 * daemon lives.
 *
 * Every time we need to contact the ap daemon, we open an RPC
 * connection to it.
 *
 * Function return indicates success or failure.  */
static int
init_ap_rpc(void)
{
	struct utsname	utname;
	int		ret;
	const char	*errstr;

	/* if we've requested this be enabled, silently return failure */
	if (noapdaemon)
		return (DRV_FAIL);

	if (_ap_host == NULL) {

		if (uname(&utname) < 0) {
			dr_loginfo("init_ap_rpc: Unable to get host name\n");
			return (DRV_FAIL);
		}

		_ap_host = strdup(utname.nodename);
		ap_set_error_handler(drap_error_handler);
	}

	if (ret = ap_init_rpc()) {
		errstr = ap_strerror(ret, 1);
		if (errstr != NULL)
			dr_loginfo("AP daemon comm init failed: %s\n",
				    errstr);
		else
			dr_loginfo("AP daemon comm init failed: error = %d\n",
				    ret);
		return (DRV_FAIL);
	}

	return (DRV_SUCCESS);
}

/*
 * do_ap_query
 *
 * Call the ap_daemon to query on the status of the given controller.
 * This is called from add_ap_meta_devices() when the device tree is built.
 *
 * Input: ctlr_count - number of ctlr_t structures malloc'ed in ctlr_ptr.
 *		This routine is responsible for freeing the ctrl_t structure.
 *
 * Output: NULL, if there is an error.
 *	   otherwise, a pointer to the dr_info  structure returned by
 *		the AP daemon.  ap_lib is responsible for deallocating
 *		this dynamic memory when next called to do a dr_query.
 */
struct dr_info *
do_ap_query(int ctlr_count, ctlr_t *ctlr_ptr)
{
	dr_query_arg	request;
	dr_query	result;
	const char	*errstr;

	request.ctlrs.ctlrs_len = ctlr_count;
	request.ctlrs.ctlrs_val = ctlr_ptr;
	request.return_aliases = 1;

	/*
	 * If comm setup fails, return.  Note that in the noapdaemon
	 * case, this routine should not even get called since
	 * add_ap_meta_devices() checked the noapdaemon flag.  This
	 * Is important since xdr_dr_query_arg below may not be defined
	 * if noapdaemon is true.
	 */
	if (init_ap_rpc()) {
		xdr_free(xdr_dr_query_arg, (char *)&request);
		return (NULL);
	}

	print_alt_names("do_ap_query", ctlr_count, ctlr_ptr);

	result = apd_dr_query(&request);

	xdr_free(xdr_dr_query_arg, (char *)&request);

	if (result.errno != 0) {
		errstr = ap_strerror(result.errno, 1);
		if (errstr != NULL)
			dr_loginfo("AP daemon query failed: %s\n",
				    errstr);
		else
			dr_loginfo("AP daemon query failed: error = %d\n",
				    result.errno);
		return (NULL);
	}
	if (result.dr_query_u.info.info_len != ctlr_count) {
		dr_loginfo("AP daemon query failed: length mismatch\n");
		return (NULL);
	}

	return (result.dr_query_u.info.info_val);
}

/*
 * do_ap_db_query
 *
 * Call the ap_daemon to query on the location of the AP databases.
 * This is called from add_ap_db_locs() when we're gathing AP device
 * usage.
 *
 * Output: NULL, if there is an error.
 *	   otherwise, a pointer to the all_db_info  structure returned by
 *		the AP daemon.  ap_lib is responsible for deallocating
 *		this dynamic memory when next called to do a dr_query.
 */
struct all_db_info *
do_ap_db_query(void)
{
	static db_query	result;
	const char	*errstr;

	/* If comm setup fails, return */
	if (init_ap_rpc()) {
		return (NULL);
	}

	result = apd_db_query();

	if (result.errno != 0) {
		errstr = ap_strerror(result.errno, 1);
		if (errstr != NULL)
			dr_loginfo("AP daemon db_query failed: %s\n",
				    errstr);
		else
			dr_loginfo("AP daemon db_query failed: error = %d\n",
				    result.errno);
		return (NULL);
	}

	if (verbose)
		dr_loginfo("AP daemon reported a database count of %d.\n",
			result.db_query_u.info.numdb);

	return (&(result.db_query_u.info));
}

#ifdef AP_TEST
/* Test stubs for the AP daemon calls */
int
init_rpc(void)
{
	return (0);
}
static void
print_ctlrs(char *type, dr_arg *da)
{
	int i;
	ctlr_t *cp;
	char	buf[MAXMSGLEN];
	char	temp[40];

	strncat(buf, type, MAXMSGLEN);
	cp = da->ctlrs.ctlrs_val;

	for (i = 0; i < da->ctlrs.ctlrs_len; i++) {
		sprintf(temp, "  %s%d", cp[i].name, cp[i].instance);
		strncat(buf, temp, MAXMSGLEN);
	}
	dr_loginfo(buf);
}
int
apd_drain(dr_arg * da)
{
	print_ctlrs("AP DRAIN:", da);
	return (0);
}
int
apd_detach_start(dr_arg * da)
{
	print_ctlrs("AP DETACH START:", da);
	return (0);
}
int
apd_detach_complete(dr_arg * da)
{
	print_ctlrs("AP DETACH COMPLETE:", da);
	return (0);
}
int
apd_resume(dr_arg * da)
{
	print_ctlrs("AP RESUME:", da);
	return (0);
}
int
apd_attach(dr_arg * da)
{
	print_ctlrs("AP ATTACH:", da);
	return (0);
}
/* ARGSUSED */
const char *
ap_strerror(long ap_errno, int log)
{
	static char *msg = "AP ERROR";
	return (msg);
}

#include <sys/stat.h>
dr_query
apd_dr_query(dr_query_arg * dqa)
{
	int		i, j;
	dr_info		*infop;
	static dr_query	result;
	ctlr_t		*cp;
	ap_alias_t	*ap;
	struct stat	sst;
	char		temp[40];

	xdr_free(xdr_dr_query, (char *)&result);

	infop = calloc(dqa->ctlrs.ctlrs_len, sizeof (dr_info));
	if (infop == NULL) {
		result.errno = 1;
		return (result);
	}

	result.errno = 0;
	result.dr_query_u.info.info_len = dqa->ctlrs.ctlrs_len;
	result.dr_query_u.info.info_val = infop;

	cp = dqa->ctlrs.ctlrs_val;

	/* Dummy up entries for some AP controllers */
	for (i = 0; i < dqa->ctlrs.ctlrs_len; i++) {

		if (!(strcmp(cp[i].name, "le")) && cp[i].instance == 0) {
			infop[i].is_alternate = 1;
			infop[i].is_active = 1;

			ap = calloc(1, sizeof (ap_alias_t));
			if (ap == NULL) {
				result.errno = 1;
				return (result);
			}
			ap->name = strdup("mle0");
			infop[i].ap_aliases.ap_aliases_len = 1;
			infop[i].ap_aliases.ap_aliases_val = ap;
		}

		if (i == 3)
			infop[i].is_alternate = 1;

		/*
		 * This is kludged for a marvin config: board 0, esp0,
		 * c4t4d4s?
		 */
		if (!(strcmp(cp[i].name, "esp")) && cp[i].instance == 0) {
			infop[i].is_alternate = 1;
			infop[i].is_active = 1;

			ap = calloc(8, sizeof (ap_alias_t));
			if (ap == NULL) {
				result.errno = 1;
				return (result);
			}
			infop[i].ap_aliases.ap_aliases_len = 8;
			infop[i].ap_aliases.ap_aliases_val = ap;

			for (j = 0; j < 8; j++) {
				sprintf(temp, "/dev/ap/dsk/mc4t4d0s%d", j);
				ap[j].name = strdup(temp);

				sprintf(temp, "/dev/dsk/c4t4d0s%d", j);
				if (stat(temp, &sst) != 0) {
					dr_loginfo(
				"apd_dr_query: stat of %s failed (errno=%d)",
						temp, errno);
				} else {
					ap[j].devid = sst.st_rdev;
				}
			}
		}
	}

	return (result);
}

db_query
apd_db_query(void)
{
	static db_query	result;

	xdr_free(xdr_db_query, (char *)&result);

	result.errno = 0;

	/*
	 * Dummy up entries for some AP controllers.  These are the
	 * only fields DR is interested in.
	 */
	result.db_query_u.info.numdb = 1;
	result.db_query_u.info.db[0].major = 1;
	result.db_query_u.info.db[0].minor = 2;

	return (result);
}
#endif /* AP_TEST */
#endif /* AP */
