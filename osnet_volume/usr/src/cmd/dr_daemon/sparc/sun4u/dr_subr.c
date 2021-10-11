/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_subr.c	1.33	98/08/30 SMI"

#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/varargs.h>
#include "dr_daemon.h"
#include "dr_subr.h"

/* from libc but not defined in any system header files */
extern int sys_nerr;
extern char *sys_errlist[];

#ifdef RPC_SVC_FG

#ifdef TEST_SVC
int _rpcpmstart = 0;		/* output always using printf during testing */
#else TEST_SVC
extern int _rpcpmstart;		/* Started by a port monitor ? */
#endif TEST_SVC

#endif RPC_SVC_FG

#ifdef DR_TEST_CONFIG
#include <stdio.h>
int	dr_testioctl;
int	dr_testconfig;
int	dr_testerrno = EINVAL;
#endif DR_TEST_CONFIG

static int		dbg = 1;

dr_err_t	dr_err; 	/* global for err return, set by dr_logerr() */

/*
 * dr_logerr logs the error to the system log
 */

void
dr_logerr(dr_error, dr_errno, msg)
int	dr_error, dr_errno;
char *msg;
{
	char errmsg[MAXMSGLEN];
	char errno_str[MAXMSGLEN];
	int ln, n;

	errmsg[0] = '\0';

	if (msg) {

		ln = MIN((MAXMSGLEN -1), (int)strlen(msg));
		strncpy(errmsg, msg, ln);
		errmsg[ln] = '\0';

		if (dbg) {
#ifdef RPC_SVC_FG
			if (_rpcpmstart)
				syslog(LOG_ERR, msg);
			else {
				fprintf(stderr, "dr_daemon(err): %s", msg);
				if (msg[strlen(msg)-1] != '\n')
					fputc('\n', stderr);
			}
#else RPC_SVC_FG
			syslog(LOG_ERR, msg);
#endif RPC_SVC_FG
		}
	}
	if (dr_errno) {
		if (dr_errno <= 0 || dr_errno > sys_nerr) {
			n = sprintf(errno_str, "...errno %d", dr_errno);
		} else {
			n = sprintf(errno_str, "...%s", sys_errlist[dr_errno]);
		}
		ln += n;
		ln = MIN((MAXMSGLEN -1), ln);

		strncat(errmsg, errno_str, ln);
		errmsg[ln] = '\0';

		if (dbg) {
#ifdef RPC_SVC_FG
			if (_rpcpmstart)
				syslog(LOG_ERR, errno_str);
			else {
				fprintf(stderr, "dr_daemon(err): %s",
				    errno_str);
				if (errno_str[strlen(errno_str)-1] != '\n')
					fputc('\n', stderr);
			}

#else RPC_SVC_FG
			syslog(LOG_ERR, errno_str);
#endif RPC_SVC_FG
		}
	}

	dr_err.err = dr_error;
	dr_err.errno = dr_errno;

	if (dr_err.msg != (void *)NULL)
		free(dr_err.msg);
	dr_err.msg = (drmsg_t)strdup(errmsg);
}

/* PRINTFLIKE1 */
void
dr_loginfo(char *format, ...)
{
	va_list	varg_ptr;
	char	buf[256];

	va_start(varg_ptr, format);
	vsprintf(buf, format, varg_ptr);
	va_end(varg_ptr);

	/* Syslog is unable to handle strings with \n in them */
	if (buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = 0;

#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_NOTICE, buf);
	else {
		fprintf(stderr, "dr_daemon(inf): %s", buf);
		if (buf[strlen(buf)-1] != '\n')
			fputc('\n', stderr);
	}
#else RPC_SVC_FG
	syslog(LOG_NOTICE, buf);
#endif RPC_SVC_FG
}

/*
 * dr_report_errors
 *
 *	When an ioctl fails, there are three different levels at which it
 *	could fail: external to the DR driver (i.e., the driver entry point
 *	didn't exist or the driver can't be loaded), at the DR driver's PIM
 *	layer, or at the DR driver's PSM layer.  In all cases, the errno
 *	after the ioctl will be set.  In the second case, the PIM error number
 *	and possibly the PIM error string will be set.  In the third case,
 *	the PSM's error number/string will be set.
 *
 *	The dr_logerr() function performs two functions, which affect this
 *	routine.  All dr_logerr() messages go to the system logs; and, the
 *	last dr_logerr() prior to the return from an RPC goes back to the SSP
 *	application.  The first function
 *
 */
void
dr_report_errors(sfdr_ioctl_arg_t *ioctl_arg, int dr_errno, char *msg)
{
	register int	pim_num = SFDR_ERR_NOERROR;
	register int	psm_num = SFDR_ERR_NOERROR;
	register char	*pim_str = NULL;
	register char	*psm_str = NULL;
	static char	sspmsg[MAXMSGLEN];
	static char	desc[MAXMSGLEN];
	static char	ioctl_msg[] = { "ioctl failed" };
	static char	dflt_msg[] = { "daemon failed" };
	static char	line_sep[] = { "\n  " };
	register int	ln;
	register int	ln2;

	/* Cover for any lack of a 'msg' */
	if (msg == NULL) {
		if (ioctl_arg == NULL)
			msg = dflt_msg;
		else
			msg = ioctl_msg;
	}

	/* Cache the pim/psm error numbers/strings */
	if (ioctl_arg != NULL) {
		pim_num = ioctl_arg->i_pim.ierr_num;
		psm_num = ioctl_arg->i_psm.ierr_num;
		pim_str = ioctl_arg->i_pim.ierr_str;
		psm_str = ioctl_arg->i_psm.ierr_str;
	}

	/* Initialize messages */
	sspmsg[0] = '\0';
	desc[0] = '\0';

	/* Construct and log the 1st part of the ssp message, and syslog it */
	(void) strncpy(sspmsg, msg, MAXMSGLEN - 1);
	ln = (MAXMSGLEN - 1) - strlen(sspmsg);
	dr_logerr(DRV_FAIL, 0, sspmsg);

	/* If there's an error number, log it and its description */
	if (ioctl_arg == NULL && dr_errno != 0) {

		/* generic daemon error */

		/* construct the second syslog line for this error */
		if (dr_errno <= 0 || dr_errno > sys_nerr)
			(void) sprintf(desc, \
					"Daemon (errno #%d): Unknown error.", \
					dr_errno);
		else
			(void) sprintf(desc, "Daemon (errno #%d): %s", \
					dr_errno, sys_errlist[dr_errno]);

		/* log the second line */
		dr_logerr(DRV_FAIL, 0, desc);

	} else {

		/* error within the driver */

		/* log any error in the PIM layer of the driver */
		if (pim_num != SFDR_ERR_NOERROR) {

			/* construct the second syslog line for this error */
			if (pim_str[0] != '\0') {
				(void) sprintf(desc, "PIM (errno #%d): ", \
						pim_num);
				ln = strlen(desc);
				ln2 = (MAXMSGLEN - 1) - ln;
				(void) strncat(desc, pim_str, ln2);
			} else {
				if (pim_num <= 0 || pim_num > sys_nerr)
					(void) sprintf(desc, "PIM (errno #" \
							"%d): Unknown error.", \
							pim_num);
				else
					(void) sprintf(desc, "PIM (errno #" \
							"%d): %s", pim_num, \
							sys_errlist[pim_num]);
			}

			/* log the second line */
			dr_logerr(DRV_FAIL, 0, desc);

			/* set dr_errno for this current error number */
			dr_errno = pim_num;
		}

		/* log any error in the PSM layer of the driver */
		if (psm_num != SFDR_ERR_NOERROR) {

			/* construct the second syslog line for this error */
			if (psm_str[0] != '\0') {
				(void) sprintf(desc, "PSM (error #%d): ", \
						psm_num);
				ln = strlen(desc);
				ln2 = (MAXMSGLEN - 1) - ln;
				(void) strncat(desc, psm_str, ln2);
			} else {
				if (psm_num <= 0 || psm_num > sys_nerr)
					(void) sprintf(desc, "PSM (error #" \
							"%d): Unknown error.", \
							psm_num);
				else
					(void) sprintf(desc, "PSM (error #" \
							"%d): %s", psm_num, \
							sys_errlist[psm_num]);
			}

			/* log the second line */
			dr_logerr(DRV_FAIL, 0, desc);

			/* set dr_errno for this current error number */
			dr_errno = psm_num;
		}
	}

	/* If a description was built above, report it to the ssp */
	if (desc[0] != '\0') {

		/* append a newline and indent the description a bit */
		ln = strlen(sspmsg);
		ln2 = (MAXMSGLEN - 1) - ln;
		(void) strncat(sspmsg, line_sep, ln2);

		/* Append the second line to the ssp message */
		ln = strlen(sspmsg);
		ln2 = (MAXMSGLEN - 1) - ln;
		(void) strncat(sspmsg, desc, ln2);

		/* Set the error message sent back to the ssp */
		if (dr_err.msg != (void *)NULL)
			free(dr_err.msg);
		dr_err.msg = (drmsg_t)strdup(sspmsg);
		dr_err.errno = dr_errno;
	}
}

#ifdef DR_TEST_CONFIG
/*
 * process_dr_config
 *
 * fill in dr_testioctl and dr_testconfig from the specified file.
 *
 * function return value: success/failure
 */
int
process_dr_config(char *cfile)
{
	FILE	*fp;
	char	line[80];
	extern int errno;

	dr_testioctl = dr_testconfig = 0;

	if (cfile == NULL)
		return (DRV_FAIL);
	if (access(cfile, 4) == -1) {
		syslog(LOG_ERR, "config: access() errno = %d\n", errno);
		return (DRV_FAIL);
	}
	if ((fp = fopen(cfile, "r")) == NULL) {
		syslog(LOG_ERR, "config: fopen() errno = %d\n", errno);
		return (DRV_FAIL);
	}

	while (fgets(line, sizeof (line), fp) != NULL) {
		set_test_config(line);
	}
	fclose(fp);

	syslog(LOG_ERR, "config: config = %#x ioctl = %#x",
		dr_testconfig, dr_testioctl);

	return (DRV_SUCCESS);
}

/*
 * dr_test_control
 *
 * Given a comma separated test control string, tear it apart,
 * parese it, set the test control variables, and return a
 * message displaying the current settings.
 */
void
dr_test_control(drmsg_t string, test_statusp_t resultp)
{
}

void
set_test_config(char *line)
{
	if (line[1] != '.') {
		dr_loginfo("test_config: unknown setting '%s'", line);
		return;
	}

	switch (line[0]) {

	case 'i':
		if (strcmp(&line[2], "probe") == 0) {
			dr_testioctl |= DR_IOCTL_PROBE;

		} else if (strcmp(&line[2], "iattach") == 0) {
			dr_testioctl |= DR_IOCTL_IATTACH;

		} else if (strcmp(&line[2], "cattach") == 0) {
			dr_testioctl |= DR_IOCTL_CATTACH;

		} else if (strcmp(&line[2], "checkdet") == 0) {
			dr_testioctl |= DR_IOCTL_CHECKDET;

		} else if (strcmp(&line[2], "devhold") == 0) {
			dr_testioctl |= DR_IOCTL_DEVHOLD;

		} else if (strcmp(&line[2], "devdetach") == 0) {
			dr_testioctl |= DR_IOCTL_DEVDETACH;

		} else if (strcmp(&line[2], "devresume") == 0) {
			dr_testioctl |= DR_IOCTL_DEVRESUME;

		} else if (strcmp(&line[2], "mvcpu0") == 0) {
			dr_testioctl |= DR_IOCTL_MVCPU0;

		} else if (strcmp(&line[2], "getstate") == 0) {
			dr_testioctl |= DR_IOCTL_GETSTATE;

		} else if (strcmp(&line[2], "setstate") == 0) {
			dr_testioctl |= DR_IOCTL_SETSTATE;

		} else if (strcmp(&line[2], "safdev") == 0) {
			dr_testioctl |= DR_IOCTL_SAFDEV;

		} else if (strcmp(&line[2], "memconfig") == 0) {
			dr_testioctl |= DR_IOCTL_MEMCONFIG;

		} else if (strcmp(&line[2], "memcost") == 0) {
			dr_testioctl |= DR_IOCTL_MEMCOST;

		} else if (strcmp(&line[2], "cpuconfig") == 0) {
			dr_testioctl |= DR_IOCTL_CPUCONFIG;

		} else if (strcmp(&line[2], "cpucost") == 0) {
			dr_testioctl |= DR_IOCTL_CPUCOST;

		} else if (strcmp(&line[2], "memstate") == 0) {
			dr_testioctl |= DR_IOCTL_MEMSTATE;

		} else {
			dr_loginfo("test_config: unknown setting '%s'", line);
		}
		break;

	case 'r':
		if (strcmp(&line[2], "state") == 0) {
			dr_testconfig |= DR_RPC_STATE;

		} else if (strcmp(&line[2], "iattach") == 0) {
			dr_testconfig |= DR_RPC_IATTACH;

		} else if (strcmp(&line[2], "cattach") == 0) {
			dr_testconfig |= DR_RPC_CATTACH;

		} else if (strcmp(&line[2], "aattach") == 0) {
			dr_testconfig |= DR_RPC_AATTACH;

		} else if (strcmp(&line[2], "fattach") == 0) {
			dr_testconfig |= DR_RPC_FATTACH;

		} else if (strcmp(&line[2], "dboard") == 0) {
			dr_testconfig |= DR_RPC_DBOARD;

		} else if (strcmp(&line[2], "drain") == 0) {
			dr_testconfig |= DR_RPC_DRAIN;

		} else if (strcmp(&line[2], "lock") == 0) {
			dr_testconfig |= DR_RPC_LOCK;

		} else if (strcmp(&line[2], "detach") == 0) {
			dr_testconfig |= DR_RPC_DETACH;

		} else if (strcmp(&line[2], "adetach") == 0) {
			dr_testconfig |= DR_RPC_ADETACH;

		} else if (strcmp(&line[2], "fdetach") == 0) {
			dr_testconfig |= DR_RPC_FDETACH;

		} else if (strcmp(&line[2], "cpu0mv") == 0) {
			dr_testconfig |= DR_RPC_CPU0MV;

		} else if (strcmp(&line[2], "obpconfig") == 0) {
			dr_testconfig |= DR_RPC_OBPCONFIG;

		} else if (strcmp(&line[2], "brdinfo") == 0) {
			dr_testconfig |= DR_RPC_BRDINFO;

		} else if (strcmp(&line[2], "getcpu0") == 0) {
			dr_testconfig |= DR_RPC_GETCPU0;

		} else if (strcmp(&line[2], "unsafed") == 0) {
			dr_testconfig |= DR_RPC_UNSAFED;

		} else {
			dr_loginfo("test_config: unknown setting '%s'", line);
		}
		break;

	case 'c':
		if (strcmp(&line[2], "attach_init") == 0) {
			dr_testconfig |= DR_CRASH_ATTACH_INIT;
		} else if (strcmp(&line[2], "complete_attach_ip") == 0) {
			dr_testconfig |= DR_CRASH_COMPLETE_ATTACH_IP;
		} else if (strcmp(&line[2], "os_attached") == 0) {
			dr_testconfig |= DR_CRASH_OS_ATTACHED;
		} else if (strcmp(&line[2], "drain") == 0) {
			dr_testconfig |= DR_CRASH_DRAIN;
		} else if (strcmp(&line[2], "lock") == 0) {
			dr_testconfig |= DR_CRASH_LOCK;
		} else if (strcmp(&line[2], "detach_ip_2") == 0) {
			dr_testconfig |= DR_CRASH_DETACH_IP_2;
		} else if (strcmp(&line[2], "os_detached") == 0) {
			dr_testconfig |= DR_CRASH_OS_DETACHED;
		} else if (strcmp(&line[2], "before_probe") == 0) {
			dr_testconfig |= DR_CRASH_BEFORE_PROBE;
		} else {
			dr_loginfo("test_config: unknown setting '%s'", line);
		}
		break;

	case 'e':
		if (strcmp(&line[2], "einval") == 0) {
			dr_testerrno = EINVAL;
		} else if (strcmp(&line[2], "eexist") == 0) {
			dr_testerrno = EEXIST;
		} else if (strcmp(&line[2], "eio") == 0) {
			dr_testerrno = EIO;
		} else if (strcmp(&line[2], "enodev") == 0) {
			dr_testerrno = ENODEV;
		} else {
			dr_loginfo("test_config: unknown setting '%s'", line);
		}
		break;

	default:
		dr_loginfo("test_config: unknown setting '%s'", line);
		break;
	}

}
#endif DR_TEST_CONFIG
