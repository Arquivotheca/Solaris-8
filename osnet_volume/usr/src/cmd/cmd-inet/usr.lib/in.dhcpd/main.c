/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.62	99/10/23 SMI"

/*
 * This file contains the argument parsing routines of the dhcpd daemon.
 * It corresponds to the START state as spec'ed.
 */

/*
 * Multithreading Notes:
 * =====================
 *
 * libdhcp is not MT-safe; thus only the main thread can successfully access
 * routines contained therein.
 *
 * There is a thread per configured interface which reads requests,
 * determines if they are for this server, and appends them to the
 * interface's PKT list.
 *
 * The main thread creates a thread to handle signals. All subsequent threads
 * (and the main thread) mask out all signals.
 *
 * The main thread will deal with the -t option. This is done using a
 * condition variable and cond_timedwait() to provide the idle timeout we
 * used to get via poll(). The condition the main thread is waiting for
 * is for npkts to become greater than zero.
 *
 * dhcp_offer: if the main thread determines that a icmp_echo check is
 * needed, it calls icmp_echo_register(), which creates a DETACHED thread,
 * puts the pkt on the interface's PKT list marked with a DHCP_ICMP_PENDING
 * flag. The ICMP validation thread (icmp_echo_async) creates the ICMP echo
 * request, and waits for a replies. If one is returned, it locates the PKT
 * list entry, and changes the flag from DHCP_ICMP_PENDING to DHCP_ICMP_IN_USE.
 * If one isn't returned in the time limit, it marks the flag from
 * DHCP_ICMP_PENDING to DHCP_ICMP_AVAILABLE. We prevent dhcp_offer from
 * registering the same address for ICMP validation due to multiple DISCOVERS
 * by using icmp_echo_status() in select_offer() to ensure we don't offer IP
 * addresses currently undergoing ICMP validation.
 *
 * bootp: If automatic allocation is in effect,
 * bootp behaves in the same fashion as dhcp_offer.
 *
 * Summary:
 *
 *	Threads:
 *		1) Main thread: Handles responding to clients; database reads
 *		   and writes. Also implements the -t option thru the use
 *		   of cond_timedwait() - The main thread goes into action
 *		   if npkts becomes non-zero or the timeout occurs. If the
 *		   timeout occurs, the main thread runs the idle routine.
 *
 *		2) Signal thread: The main thread creates this thread, and
 *		   then masks out all signals. The signal thread waits on
 *		   sigwait(), and processes all signals. It notifies the
 *		   main thread of EINTR or ETERM via a global variable, which
 *		   the main thread checks upon the exit to cond_timedwait.
 *		   This thread is on it's own LWP, and is DETACHED | DAEMON.
 *		   The thread function is sig_handle().
 *
 *		3) Interface threads: Each interface structure has a thread
 *		   associated with it (created in open_interfaces) which is
 *		   responsible for polling the interface, validating bootp
 *		   packets received, and placing them on the interface's
 *		   PKT_LIST. The thread function is monitor_interface().
 *		   When notified by the main thread via the thr_exit flag,
 *		   the thread prints interface statistics for the interface,
 *		   and then exits.
 *
 *		4) ICMP validation threads: Created as needed when dhcp_offer
 *		   or bootp_compatibility wish to ICMP validate a potential
 *		   OFFER candidate IP address. These threads are created
 *		   DETACHED and SUSPENDED by the main thread, which then
 *		   places the plp structure associated with the request
 *		   back on the interface's PKT_LIST, then continues the
 *		   thread. A ICMP validation thread exits when it has
 *		   determined the status of an IP address, changing the
 *		   d_icmpflag in the plp appropriately. It then bumps npkts
 *		   by one, and notifies the main thread that there is work
 *		   to do via cond_signal. It then exits. The thread function
 *		   is icmp_echo_async().
 *
 *	Locks:
 *		1) npkts_mtx	-	Locks the global npkts.
 *		   npkts_cv	-	Condition variable for npkts.
 *					Lock-order independent.
 *
 *		2) totpkts_mtx	-	Locks the global totpkts.
 *					Lock-order independent.
 *
 *		3) if_head_mtx	-	Locks the global interface list.
 *
 *		4) ifp_mtx	-	Locks contents of the enclosed
 *					interface (IF) structure, including
 *					such things as thr_exit flag and
 *					statistics counters.
 *
 *		5) pkt_mtx	-	Locks PKT_LIST head list within the
 *					enclosed interface (IF) structure.
 *					This lock should be held before
 *					plp_mtx is held, and unlocked AFTER
 *					plp_mtx is unlocked.
 *
 *		6) plp_mtx	-	Mutex lock protecting access to
 *					a plp's structure elements,
 *					specifically those associated with
 *					a ICMP validation - off_ip and
 *					d_icmpflag. pkt_mtx of IF structure
 *					containing the list for which this
 *					plp is a member should be locked
 *					before this lock is held. Note that
 *					if this plp is not currently on
 *					a PKT_LIST, there's no need to hold
 *					either lock.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/dhcp.h>
#include <synch.h>
#include <netdb.h>
#include <dhcdata.h>
#include <dhcp_defaults.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern int optind, opterr;
extern char *optarg;

typedef struct dhcp_dflt {
	char		*def_name;			/* opt name */
	boolean_t	def_present;			/* opt present? */
	boolean_t	(*def_vinit)(struct dhcp_dflt *, const char *);
	union {
		char 		*udef_string;
		boolean_t	udef_bool;
		int		udef_num;
	} dhcp_dflt_un;
#define	def_bool	dhcp_dflt_un.udef_bool		/* opt val: boolean_t */
#define	def_num		dhcp_dflt_un.udef_num		/* opt val: int */
#define	def_string	dhcp_dflt_un.udef_string	/* opt val: string */
} DHCP_DFLT;

static boolean_t boolean_v(DHCP_DFLT *, const char *);
static boolean_t integer_v(DHCP_DFLT *, const char *);
static boolean_t string_v(DHCP_DFLT *, const char *);
static boolean_t bootp_v(DHCP_DFLT *, const char *);
static boolean_t logging_v(DHCP_DFLT *, const char *);
static boolean_t offer_v(DHCP_DFLT *, const char *);
static boolean_t runmode_v(DHCP_DFLT *, const char *);
static int collect_options(int, char **);
static void usage(void);
static void local_closelog(void);
static void *sig_handle(void *);

#define	D_RUNMODE	0
#define	D_DEBUG		1
#define	D_VERBOSE	2
#define	D_HOPS		3
#define	D_LOGGING	4
#define	D_IF		5
#define	D_OFFER		6
#define	D_ETHERS	7
#define	D_ICMP		8
#define	D_RESCAN	9
#define	D_BOOTP		10
#define	D_RELAY		11
#define	D_LAST		D_RELAY

static DHCP_DFLT options[D_LAST + 1] = {
/* name			Present?    Verify func	Value */
/* ====			========    ===========	===== */
	/* Run mode / BOOTP relay agent selection option */
{ RUN_MODE,		B_FALSE,    runmode_v,	SERVER			},
	/* Generic daemon options */
{ "DEBUG",		B_FALSE,    boolean_v,	B_FALSE			},
{ VERBOSE,		B_FALSE,    boolean_v,	B_FALSE			},
{ RELAY_HOPS,		B_FALSE,    integer_v,	(char *)DEFAULT_HOPS	},
{ LOGGING_FACILITY,	B_FALSE,    logging_v,	0			},
{ INTERFACES,		B_FALSE,    string_v,	NULL			},
	/* DHCP server run mode options */
{ OFFER_CACHE_TIMEOUT,	B_FALSE,    offer_v,	(char *)DEFAULT_OFFER_TTL},
{ ETHERS_COMPAT,	B_FALSE,    boolean_v,	(char *)B_TRUE		},
{ ICMP_VERIFY,		B_FALSE,    boolean_v,	(char *)B_TRUE		},
{ RESCAN_INTERVAL,	B_FALSE,    integer_v,	0			},
{ BOOTP_COMPAT,		B_FALSE,    bootp_v,	NULL			},
	/* BOOTP relay agent options */
{ RELAY_DESTINATIONS,	B_FALSE,    string_v,	NULL			}
};

#define	DHCPDFLT_NAME(x)	(options[x].def_name)
#define	DHCPDFLT_PRESENT(x)	(options[x].def_present)
#define	DHCPDFLT_VINIT(x, y)	(options[x].def_vinit(&options[x], y))
#define	DHCPDFLT_BOOL(x)	(options[x].def_bool)
#define	DHCPDFLT_NUM(x)		(options[x].def_num)
#define	DHCPDFLT_STRING(x)	(options[x].def_string)

int debug;
boolean_t verbose;
boolean_t noping;		/* Always ping before offer by default */
boolean_t ethers_compat;	/* set if server should check ethers table */
boolean_t no_dhcptab;		/* set if no dhcptab exists */
boolean_t server_mode;		/* set if running in server mode */
boolean_t be_automatic;		/* set if bootp server should allocate IPs */
boolean_t reinitialize;		/* set to reinitialize when idle */
int max_hops; 			/* max relay hops before discard */
int log_local = -1;		/* syslog local facility number */
int icmp_tries = DHCP_ICMP_ATTEMPTS; /* Number of attempts @ icmp_timeout */
time_t off_secs;		/* def ttl of an offer */
time_t rescan_interval;	/* dhcptab rescan interval */
time_t abs_rescan;		/* absolute dhcptab rescan time */
time_t icmp_timeout = DHCP_ICMP_TIMEOUT; /* milliseconds to wait for response */
struct in_addr	server_ip;	/* IP address of server's default hostname */

/*
 * This global variable keeps the total number of packets waiting to
 * be processed.
 */
ulong_t npkts;
mutex_t	npkts_mtx;
cond_t	npkts_cv;

/*
 * This global keeps a running total of all packets received by all
 * interfaces.
 */
ulong_t totpkts;
mutex_t totpkts_mtx;

/*
 * This global is set by the signal handler when the main thread (and thus
 * the daemon should exit. No need to lock this one, as it is set once, and
 * then read (until main thread reads it and starts the exit procedure).
 */
static int time_to_go = 0;
static int log_facilities[] = {
	LOG_LOCAL0,
	LOG_LOCAL1,
	LOG_LOCAL2,
	LOG_LOCAL3,
	LOG_LOCAL4,
	LOG_LOCAL5,
	LOG_LOCAL6,
	LOG_LOCAL7
};

int
main(int argc, char *argv[])
{
	int		tnpkts;
	boolean_t	bootp_compat, found;
	sigset_t	set;
	int		i, ns;
	int		err = 0;
	IF		*ifp;
	PKT_LIST	*plp, *tplp;
	char		*tp, *datastore;
	struct rlimit	rl;
	struct hostent	*hp;
	thread_t	sigthread;
	int		tbl_err;
	char		*pathp = NULL;
	char		scratch[MAXHOSTNAMELEN + 1];
	timestruc_t	to;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif	/* ! TEXT_DOMAIN */

	(void) textdomain(TEXT_DOMAIN);

	if (geteuid() != (uid_t)0) {
		(void) fprintf(stderr, gettext("Must be 'root' to run %s.\n"),
		    DHCPD);
		return (EPERM);
	}

	if (collect_options(argc, argv) != 0) {
		if (errno == EAGAIN) {
			(void) fprintf(stderr, gettext("DHCP daemon defaults "
			    "file (%s) still locked.\n"), TBL_NS_FILE);
			err = EAGAIN;
		} else {
			usage();
			err = EINVAL;
		}
		return (err);
	}

	/* Deal with run mode generic options first */
	debug = DHCPDFLT_BOOL(D_DEBUG);
	verbose = DHCPDFLT_BOOL(D_VERBOSE);
	max_hops = DHCPDFLT_NUM(D_HOPS);
	interfaces = DHCPDFLT_STRING(D_IF);
	bootp_compat = DHCPDFLT_PRESENT(D_BOOTP); /* present then yes */
	ethers_compat = DHCPDFLT_BOOL(D_ETHERS);

	if (DHCPDFLT_PRESENT(D_LOGGING))
		log_local = log_facilities[DHCPDFLT_NUM(D_LOGGING)];

	server_mode = (strcasecmp(DHCPDFLT_STRING(D_RUNMODE), SERVER) == 0);

	if (server_mode) {

		if (bootp_compat) {
			be_automatic = (strcasecmp(DHCPDFLT_STRING(D_BOOTP),
			    AUTOMATIC) == 0);
		}

		if (ethers_compat && stat_boot_server() == 0) {
			/*
			 * Respect user's -b setting. Use -b manual as the
			 * default.
			 */
			if (!bootp_compat) {
				bootp_compat = B_TRUE;
				be_automatic = B_FALSE;
			}
		}

		if ((noping = DHCPDFLT_BOOL(D_ICMP)) == B_FALSE) {
			(void) fprintf(stderr, gettext("\nWARNING: Disabling \
duplicate IP address detection!\n\n"));
		}

		off_secs = DHCPDFLT_NUM(D_OFFER);

		if ((rescan_interval = DHCPDFLT_NUM(D_RESCAN)) != 0)
			abs_rescan = (rescan_interval * 60L) + time(NULL);

		ns = dd_ns(&tbl_err, &pathp);
		if (ns == TBL_FAILURE) {
			switch (tbl_err) {
			case TBL_BAD_SYNTAX:
				(void) fprintf(stderr, gettext(
"%s: Bad syntax: keyword is missing colon (:)\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_NS:
				(void) fprintf(stderr, gettext(
"%s: Bad resource name. Must be 'files' or 'nisplus'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_DIRECTIVE:
				(void) fprintf(stderr, gettext(
"%s: Unsupported keyword. Must be 'resource:' or 'path:'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_STAT_ERROR:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value does not exist.\n"), TBL_NS_FILE);
				break;
			case TBL_BAD_DOMAIN:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value must be a valid nisplus domain name.\n"),
				    TBL_NS_FILE);
				err = 1;
				break;
			case TBL_OPEN_ERROR:
				if (ethers_compat) {
					(void) fprintf(stderr, gettext(
					    "WARNING: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 0; /* databases not required */
				} else {
					(void) fprintf(stderr, gettext(
					    "FATAL: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 1;
				}
				break;
			}
			if (err != 0)
				return (err);
		}
	} else {
		if (!DHCPDFLT_PRESENT(D_RELAY)) {
			(void) fprintf(stderr, gettext("Missing BOOTP "
			    "relay destinations (%s)\n"),
			    RELAY_DESTINATIONS);
			return (1);
		}
		if ((err = relay_agent_init(DHCPDFLT_STRING(D_RELAY))) != 0)
			return (err);
	}

	if (!debug) {
		/* Daemon (background, detach from controlling tty). */
		switch (fork()) {
		case -1:
			(void) fprintf(stderr,
			    gettext("Daemon cannot fork(): %s\n"),
			    strerror(errno));
			return (errno);
		case 0:
			/* child */
			break;
		default:
			/* parent */
			return (0);
		}

		if ((err = getrlimit(RLIMIT_NOFILE, &rl)) < 0) {
			dhcpmsg(LOG_ERR, "Cannot get open file limit: %s\n",
			    strerror(errno));
			return (err);
		}

		/* handle cases where limit is infinity */
		if (rl.rlim_cur == RLIM_INFINITY) {
			rl.rlim_cur = (rl.rlim_max == RLIM_INFINITY) ?
			    OPEN_MAX : rl.rlim_max;
		}
		for (i = 0; (rlim_t)i < rl.rlim_cur; i++)
			(void) close(i);

		errno = 0;	/* clean up benign bad file no error */

		(void) open("/dev/null", O_RDONLY, 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);

		/* set NOFILE to unlimited */
		rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
		if ((err = setrlimit(RLIMIT_NOFILE, &rl)) < 0) {
			dhcpmsg(LOG_ERR, "Cannot set open file limit: %s\n",
			    strerror(errno));
			return (err);
		}

		/* Detach console */
		(void) setsid();

		(void) openlog(DHCPD, LOG_PID, LOG_DAEMON);
		if (verbose)
			dhcpmsg(LOG_INFO, "Daemon started.\n");
	}

	/*
	 * Block all signals in main thread - threads created will also
	 * ignore signals.
	 */
	(void) sigfillset(&set);
	(void) thr_sigsetmask(SIG_SETMASK, &set, NULL);

	/*
	 * Create signal handling thread.
	 */
	if ((err = thr_create(NULL, 0, sig_handle, NULL,
	    THR_NEW_LWP | THR_DAEMON | THR_DETACHED, &sigthread)) != 0) {
		(void) fprintf(stderr,
		    gettext("Cannot start signal handling thread, error: %d\n"),
			err);
		return (err);
	}

	(void) sigdelset(&set, SIGABRT);	/* except for abort */

#ifdef	DEBUG
	(void) fprintf(stderr,
	    gettext("Started signal handling thread: %d\n"), sigthread);
#endif	/* DEBUG */

	/* Save away the IP address associated with our HOSTNAME. */
	(void) sysinfo(SI_HOSTNAME, scratch, MAXHOSTNAMELEN + 1);
	if ((tp = strchr(scratch, '.')) != NULL)
		*tp = '\0';

	if ((hp = gethostbyname(scratch)) != NULL &&
	    hp->h_addrtype == AF_INET &&
	    hp->h_length == sizeof (struct in_addr)) {
		(void) memcpy((char *)&server_ip, hp->h_addr_list[0],
		    sizeof (server_ip));
	} else {
		dhcpmsg(LOG_ERR,
		    "Cannot determine server hostname/IP address.\n");
		local_closelog();
		return (1);
	}

	if (verbose) {
		dhcpmsg(LOG_INFO, "Daemon Version: %s\n", DAEMON_VERS);
		dhcpmsg(LOG_INFO, "Maximum relay hops: %d\n", max_hops);
		if (log_local > -1) {
			dhcpmsg(LOG_INFO,
			    "Transaction logging to %s enabled.\n",
			    debug ? "console" : "syslog");
		}
		if (server_mode) {
			dhcpmsg(LOG_INFO, "Run mode is: DHCP Server Mode.\n");
			switch (ns) {
			case TBL_NS_UFS:
				datastore = "files";
				break;
			case TBL_NS_NISPLUS:
				datastore = "nisplus";
				pathp = (
				    (tp = getenv("NIS_PATH")) == NULL ? pathp :
				    tp);
				break;
			default:
				datastore = pathp = "none";
				break;
			}
			dhcpmsg(LOG_INFO, "Datastore: %s\n", datastore);
			dhcpmsg(LOG_INFO, "Path: %s\n", pathp);
			dhcpmsg(LOG_INFO, "DHCP offer TTL: %d\n", off_secs);
			if (ethers_compat)
				dhcpmsg(LOG_INFO,
				    "Ethers compatibility enabled.\n");
			if (bootp_compat)
				dhcpmsg(LOG_INFO,
				    "BOOTP compatibility enabled.\n");
			if (rescan_interval != 0) {
				dhcpmsg(LOG_INFO,
				    "Dhcptab rescan interval: %d minutes.\n",
				    rescan_interval);
			}
			dhcpmsg(LOG_INFO, "ICMP validation timeout: %d "
			    "milliseconds, Attempts: %d.\n", icmp_timeout,
			    icmp_tries);
		} else
			dhcpmsg(LOG_INFO, "Run mode is: Relay Agent Mode.\n");
	}

	if ((err = open_interfaces()) != 0) {
		local_closelog();
		return (err);
	}

	(void) mutex_init(&npkts_mtx, USYNC_THREAD, 0);
	(void) cond_init(&npkts_cv, USYNC_THREAD, 0);
	(void) mutex_init(&totpkts_mtx, USYNC_THREAD, 0);

	if (server_mode) {

		if (inittab() != 0) {
			dhcpmsg(LOG_ERR, "Cannot allocate macro hash table.\n");
			local_closelog();
			(void) mutex_destroy(&npkts_mtx);
			(void) cond_destroy(&npkts_cv);
			(void) mutex_destroy(&totpkts_mtx);
			return (1);
		}

		if ((err = checktab()) != 0 ||
		    (err = readtab(NEW_DHCPTAB)) != 0) {
			if (err == ENOENT || ethers_compat) {
				no_dhcptab = B_TRUE;
			} else {
				dhcpmsg(LOG_ERR,
				    "Error reading macro table.\n");
				local_closelog();
				(void) mutex_destroy(&npkts_mtx);
				(void) cond_destroy(&npkts_cv);
				(void) mutex_destroy(&totpkts_mtx);
				return (err);
			}
		} else
			no_dhcptab = B_FALSE;
	}

	/*
	 * While forever, read packets off selected/available interfaces
	 * and dispatch off to handle them.
	 */
	for (;;) {
		(void) mutex_lock(&npkts_mtx);
		if (server_mode) {
			to.tv_sec = time(NULL) + DHCP_IDLE_TIME;
			to.tv_nsec = 0;
			while (npkts == 0 && time_to_go == 0) {
				if (cond_timedwait(&npkts_cv, &npkts_mtx,
				    &to) == ETIME || reinitialize) {
					err = idle();
					break;
				}
			}
		} else {
			/* No idle processing */
			while (npkts == 0 && time_to_go == 0)
				(void) cond_wait(&npkts_cv, &npkts_mtx);
		}
		tnpkts = npkts;
		(void) mutex_unlock(&npkts_mtx);

		/*
		 * Fatal error during idle() processing or sig_handle thread
		 * says it's time to go...
		 */
		if (err != 0 || time_to_go)
			break;

		/*
		 * We loop through each interface and process one packet per
		 * interface. (that's one non-DHCP_ICMP_PENDING packet). Relay
		 * agent tasks are handled by the per-interface threads, thus
		 * we should only be dealing with bootp/dhcp server bound
		 * packets here.
		 *
		 * The main thread treats the interface packet lists as
		 * "stacks", or FIFO objects. We do this so that we get
		 * the latest, equivalent request from the client before
		 * responding, thus keeping the chance of responding to
		 * moldy requests to an absolute minimum.
		 */
		(void) mutex_lock(&if_head_mtx);
		for (ifp = if_head; ifp != NULL && tnpkts != 0;
		    ifp = ifp->next) {
			(void) mutex_lock(&ifp->pkt_mtx);
#ifdef	DEBUG_PKTLIST
			dhcpmsg(LOG_DEBUG, "Main: npkts is %d\n", npkts);
			display_pktlist(ifp);
#endif	/* DEBUG_PKTLIST */
			/*
			 * Remove the last packet from the list
			 * which is not awaiting ICMP ECHO
			 * validation (DHCP_ICMP_PENDING).
			 */
			plp = ifp->pkttail;
			found = B_FALSE;
			while (plp != NULL) {
				(void) mutex_lock(&plp->plp_mtx);
				switch (plp->d_icmpflag) {
				case DHCP_ICMP_NOENT:
					detach_plp(ifp, plp);
					(void) mutex_unlock(&plp->plp_mtx);
					/*
					 * See if there's an earlier one
					 * with a different status, exchanging
					 * this plp for that one. If that
					 * one is DHCP_ICMP_PENDING, skip it.
					 */
					plp = refresh_pktlist(ifp, plp);
					(void) mutex_lock(&plp->plp_mtx);
					if (plp->d_icmpflag !=
					    DHCP_ICMP_PENDING) {
						found = B_TRUE;
					}
					break;
				case DHCP_ICMP_AVAILABLE:
					/* FALLTHRU */
				case DHCP_ICMP_IN_USE:
					detach_plp(ifp, plp);
					found = B_TRUE;
					break;
				case DHCP_ICMP_PENDING:
					/* Skip this one. */
					break;
				case DHCP_ICMP_FAILED:
					/* FALLTHRU */
				default:
					/* clean up any failed ICMP attempts */
#ifdef	DEBUG
					{
						char	ntoab[NTOABUF];
						dhcpmsg(LOG_DEBUG,
						    "Failed ICMP attempt: %s\n",
						    inet_ntoa_r(plp->off_ip,
						    ntoab));
					}
#endif	/* DEBUG */
					tplp = plp;
					plp = plp->prev;
					detach_plp(ifp, tplp);
					(void) mutex_unlock(&tplp->plp_mtx);
					free_plp(tplp);
					continue;
				}
				(void) mutex_unlock(&plp->plp_mtx);

				if (found)
					break;
				else
					plp = plp->prev;
			}
			(void) mutex_unlock(&ifp->pkt_mtx);

			if (plp == NULL)
				continue;  /* nothing on this interface */

			/*
			 * No need to hold plp_mtx for balance of loop.
			 * We have already avoided potential DHCP_ICMP_PENDING
			 * packets.
			 */

			(void) mutex_lock(&npkts_mtx);
			tnpkts = --npkts; /* one less packet to process. */
			(void) mutex_unlock(&npkts_mtx);

			/*
			 * Based on the packet type, process accordingly.
			 */
			if (plp->pkt->op == BOOTREQUEST) {
				if (plp->opts[CD_DHCP_TYPE]) {
					/* DHCP packet */
					if (dhcp(ifp, plp) == B_FALSE) {
						/* preserve plp */
						continue;
					}
				} else {
					/* BOOTP packet */
					if (!bootp_compat) {
						dhcpmsg(LOG_INFO,
"BOOTP request received on interface: %s ignored.\n",
						    ifp->nm);
					} else {
						if (bootp(ifp, plp) ==
						    B_FALSE) {
							/* preserve plp */
							continue;
						}
					}
				}
			} else {
				dhcpmsg(LOG_ERR,
"Unexpected packet received on BOOTP server port. Interface: %s. Ignored.\n",
				    ifp->nm);
			}
			(void) mutex_lock(&ifp->ifp_mtx);
			ifp->processed++;
			(void) mutex_unlock(&ifp->ifp_mtx);
			free_plp(plp); /* Free the packet. */
		}
		(void) mutex_unlock(&if_head_mtx);
	}

	/* Daemon terminated. */
	if (server_mode)
		resettab();

	close_interfaces();	/* reaps threads */
	local_closelog();
	(void) fflush(NULL);
	(void) mutex_destroy(&npkts_mtx);
	(void) mutex_destroy(&totpkts_mtx);
	return (err);
}

/*
 * Signal handler routine. All signals handled by calling thread.
 */
/* ARGSUSED */
static void *
sig_handle(void *arg)
{
	int		sig;
	sigset_t	set;
	char buf[SIG2STR_MAX];

	(void) sigfillset(&set);	/* catch all signals */

	/* wait for a signal */
	for (;;) {
		switch (sig = sigwait(&set)) {
		case SIGHUP:
			reinitialize = sig;
			break;
		case SIGTERM:
			/* FALLTHRU */
		case SIGINT:
			(void) sig2str(sig, buf);
			dhcpmsg(LOG_NOTICE, "Signal: %s received...Exiting\n",
			    buf);
			time_to_go = 1;
			break;
		default:
			if (verbose) {
				(void) sig2str(sig, buf);
				dhcpmsg(LOG_INFO,
				    "Signal: %s received...Ignored\n",
				    buf);
			}
			break;
		}
		if (time_to_go) {
			(void) mutex_lock(&npkts_mtx);
			(void) cond_broadcast(&npkts_cv);
			(void) mutex_unlock(&npkts_mtx);
			break;
		}
	}
	thr_exit(NULL);
	return ((void *)NULL);	/* NOTREACHED */
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "%s:\n\n\tCommon: [-d] [-v] [-i interface, ...] "
	    "[-h hops] [-l local_facility]\n\n\t"
	    "Server: [-e] [-n] [-t rescan_interval] [-o DHCP_offer_TTL]\n\t\t"
	    "[ -b automatic | manual]\n\n\t"
	    "Relay Agent: -r IP | hostname, ...\n"), DHCPD);
}

static void
local_closelog(void)
{
	dhcpmsg(LOG_INFO, "Daemon terminated.\n");
	if (!debug)
		closelog();
}

/*
 * Given a received BOOTP packet, generate an appropriately sized,
 * and generically initialized BOOTP packet.
 */
PKT *
gen_bootp_pkt(int size, PKT *srcpktp)
{
	/* LINTED [smalloc returns lw aligned addresses.] */
	PKT *pkt = (PKT *)smalloc(size);

	pkt->htype = srcpktp->htype;
	pkt->hlen = srcpktp->hlen;
	pkt->xid = srcpktp->xid;
	pkt->secs = srcpktp->secs;
	pkt->flags = srcpktp->flags;
	pkt->giaddr.s_addr = srcpktp->giaddr.s_addr;
	(void) memcpy(pkt->cookie, srcpktp->cookie, 4);
	(void) memcpy(pkt->chaddr, srcpktp->chaddr, srcpktp->hlen);

	return (pkt);
}

/*
 * Points field serves to identify those packets whose allocated size
 * and address is not represented by the address in pkt.
 */
void
free_plp(PKT_LIST *plp)
{
	char *tmpp;

	(void) mutex_lock(&plp->plp_mtx);
#ifdef	DEBUG
	{
		char	ntoab[NTOABUF];
		dhcpmsg(LOG_DEBUG,
"%04d: free_plp(0x%x)pkt(0x%x)len(%d)icmp(%d)IP(%s)next(0x%x)prev(0x%x)\n",
		    thr_self(), plp, plp->pkt, plp->len, plp->d_icmpflag,
		    inet_ntoa_r(plp->off_ip, ntoab), plp->next, plp->prev);
	}
#endif	/* DEBUG */
	if (plp->pkt) {
		if (plp->offset != 0)
			tmpp = (char *)((uint_t)plp->pkt - plp->offset);
		else
			tmpp = (char *)plp->pkt;
		free(tmpp);
	}
	(void) mutex_unlock(&plp->plp_mtx);
	(void) mutex_destroy(&plp->plp_mtx);
	free(plp);
	plp = PKT_LIST_NULL;
}

/*
 * Validate boolean is "TRUE" or "FALSE".
 * Returns B_TRUE if successful, B_FALSE otherwise.
 */
static boolean_t
boolean_v(DHCP_DFLT *dp, const char *option)
{
	boolean_t	i;

	if (dp == NULL || option == NULL)
		return (B_FALSE);
	if (strcasecmp(option, DEFAULT_TRUE) == 0) {
		i = B_TRUE;
	} else if (strcasecmp(option, DEFAULT_FALSE) == 0) {
		i = B_FALSE;
	} else {
		return (B_FALSE); /* huh? */
	}
	dp->def_bool = i;
	return (B_TRUE);
}

/*
 * Validate integer data.
 * Returns B_TRUE if successful, B_FALSE otherwise.
 */
static boolean_t
integer_v(DHCP_DFLT *dp, const char *option)
{
	if (dp == NULL || option == NULL || !isdigit(*option))
		return (B_FALSE);
	dp->def_num = atoi(option);
	return (B_TRUE);
}

/*
 * Check if value is a string.
 * Returns B_TRUE if successful, B_FALSE otherwise
 */
static boolean_t
string_v(DHCP_DFLT *dp, const char *option)
{
	if (dp == NULL || option == NULL ||
	    (dp->def_string = strdup(option)) == NULL) {
		return (B_FALSE);
	}
	return (B_TRUE);
}

/*
 * Validate bootp compatibility options. Must be "automatic" or
 * "manual".
 * Returns B_TRUE if successful, B_FALSE otherwise.
 */
static boolean_t
bootp_v(DHCP_DFLT *dp, const char *option)
{
	if (dp == NULL || option == NULL)
		return (B_FALSE);

	if ((strcasecmp(option, AUTOMATIC) == 0 ||
	    strcasecmp(option, MANUAL) == 0) &&
	    (dp->def_string = strdup(option)) != NULL) {
		return (B_TRUE);
	}
	return (B_FALSE);
}

/*
 * Validate logging facility. Must be a number between
 * 0 and 7 inclusive.
 * Returns B_TRUE if successful, B_FALSE otherwise.
 */
static boolean_t
logging_v(DHCP_DFLT *dp, const char *option)
{
	if (integer_v(dp, option) && (dp->def_num >= 0 && dp->def_num <= 7))
		return (B_TRUE);

	(void) fprintf(stderr, gettext("Syslog local facility must be in the "
	    "range of 0 through 7.\n"));
	return (B_FALSE);
}

/*
 * Validate offer cache timeout. Must be a number greater
 * than 0.
 * Returns B_TRUE if successful, B_FALSE otherwise.
 */
static boolean_t
offer_v(DHCP_DFLT *dp, const char *option)
{
	if (integer_v(dp, option) && dp->def_num > 0)
		return (B_TRUE);
	return (B_FALSE);
}

/*
 * Validate run mode. Must be "server" or "relay".
 * Returns B_TRUE if successful, B_FALSE otherwise
 */
static boolean_t
runmode_v(DHCP_DFLT *dp, const char *option)
{
	if (dp == NULL || option == NULL)
		return (B_FALSE);
	if ((strcasecmp(option, SERVER) == 0 ||
	    strcasecmp(option, RELAY) == 0) &&
	    (dp->def_string = strdup(option)) != NULL) {
		return (B_TRUE);
	}
	return (B_FALSE);
}

/*
 * Initialize options table based upon defaults file settings or command
 * line flags. Handle all option inter-dependency checking here. No value
 * checking is done here.
 *
 * Returns 0 if successful, nonzero otherwise.
 */
static int
collect_options(int count, char **args)
{
	int			c, i, j;
	dhcp_defaults_t		*dsp = NULL;
	char			*mode;

	/* First, load the defaults from the file, if present. */
	for (errno = 0, i = 0; i < DHCP_RDDFLT_RETRIES &&
	    read_dhcp_defaults(&dsp) < 0; i++) {
		(void) fprintf(stderr, gettext(
		    "WARNING: DHCP daemon defaults file: %s\n"),
		    strerror(errno));
		if (errno == EAGAIN) {
			/* file's busy, wait one second and try again */
			sleep(1);
		} else
			break;
	}
	if (errno == EAGAIN)
		return (EAGAIN);

	/* set default RUN_MODE to server if it wasn't found in the file */
	if (query_dhcp_defaults(dsp, RUN_MODE, &mode) < 0) {
		if (errno == ENOENT) {
			if (add_dhcp_defaults(&dsp, RUN_MODE, SERVER) != 0)
				return (errno);
		}
	} else
		free(mode);

	/*
	 * Second, pick up the user's preferences from the command line,
	 * which modify the defaults file settings.
	 */
	while ((c = getopt(count, args, "denvh:o:r:b:i:t:l:")) != -1) {

		boolean_t	relay_mode = B_FALSE;
		char		*key = NULL, *value = NULL;

		switch (c) {
		case 'd':
			key = "DEBUG";
			value = DEFAULT_TRUE;
			break;
		case 'n':
			key = ICMP_VERIFY;
			value = DEFAULT_FALSE;
			break;
		case 'v':
			key = VERBOSE;
			value = DEFAULT_TRUE;
			break;
		case 'e':
			key = ETHERS_COMPAT;
			value = DEFAULT_FALSE;
			break;
		case 'r':
			key = RELAY_DESTINATIONS;
			value = optarg;
			relay_mode = B_TRUE;
			break;
		case 'b':
			key = BOOTP_COMPAT;
			value = optarg;
			break;
		case 'h':
			key = RELAY_HOPS;
			value = optarg;
			break;
		case 'i':
			key = INTERFACES;
			value = optarg;
			break;
		case 'o':
			key = OFFER_CACHE_TIMEOUT;
			value = optarg;
			break;
		case 't':
			key = RESCAN_INTERVAL;
			value = optarg;
			break;
		case 'l':
			key = LOGGING_FACILITY;
			value = optarg;
			break;
		default:
			(void) fprintf(stderr, gettext("Unknown option: %c\n"),
			    c);
			return (EINVAL);
		}

		/*
		 * Create defaults if they don't exist, or replace
		 * their value if they exist.
		 */
		if (replace_dhcp_defaults(&dsp, key, value) < 0)
			return (errno);

		if (relay_mode) {
			if (replace_dhcp_defaults(&dsp, RUN_MODE, RELAY) < 0)
				return (errno);
		}
	}

	/* load options table, validating value portions of present as we go */
	for (i = 0; dsp != NULL && dsp[i].def_key != NULL; i++) {
		if (dsp[i].def_type != DHCP_KEY)
			continue;	/* comment */
		for (j = 0; j <= D_LAST; j++) {
			if (strcasecmp(DHCPDFLT_NAME(j),
			    dsp[i].def_key) == 0) {
				DHCPDFLT_PRESENT(j) = B_TRUE;
				if (DHCPDFLT_VINIT(j, dsp[i].def_value))
					break;
				else {
					(void) fprintf(stderr,
					    "Invalid value for option: %s\n",
					    DHCPDFLT_NAME(j));
					return (EINVAL);
				}
			}
		}
	}

	free_dhcp_defaults(dsp);

	return (0);
}
