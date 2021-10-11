/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_main.c	1.74	99/07/15 SMI"

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<syslog.h>
#include	<errno.h>
#include	<string.h>
#include	<rpc/rpc.h>
#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<signal.h>
#include	<sys/signal.h>
#include	<rpcsvc/nfs_prot.h>
#include	<netinet/in.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<sys/mount.h>
#include	<sys/resource.h>
#include	<netdb.h>
#include	<sys/signal.h>
#include	<netdir.h>
#include	<locale.h>
#include	<ulimit.h>
#include	<ucontext.h>
#include	<pwd.h>
#include	<grp.h>
#include	<sys/systeminfo.h>
#include	<thread.h>
#include	<synch.h>
#include	"vold.h"


/* extern vars */
extern int 	trace;		/* nfs server trace enable */

/* extern prototypes */
extern int	__rpc_negotiate_uid(int fd);


/* local prototypes */
static struct netconfig *trans_loopback(void);
static void		trans_netbuf(struct netconfig *, struct netbuf *);
static void		catch(void);
static void		catch_n_exit(void);
static void		reread_config(void);
static void		catch_n_return(int, siginfo_t *, ucontext_t *);
static void		usage(void);
static void		vold_run(void);


/* global vars */
int 		verbose 	= DEFAULT_VERBOSE;
int 		debug_level 	= DEFAULT_DEBUG;
char		*vold_root 	= DEFAULT_VOLD_ROOT;
char		*vold_config 	= DEFAULT_VOLD_CONFIG;
char		*vold_devdir	= DEFAULT_VOLD_DEVDIR;
char		*volume_group	= DEFAULT_VOLUME_GROUP;
char		*nisplus_group	= DEFAULT_NISPLUS_GROUP;
int		never_writeback = 0;
uid_t		default_uid;
gid_t		default_gid;
char		self[MAXHOSTNAMELEN];
struct timeval	current_time;
rlim_t		original_nofile;
int		vold_running = 0;
cond_t		running_cv;
mutex_t		running_mutex;


/* local vars */
static int	vold_polltime = DEFAULT_POLLTIME;
static char	*prog_name;
#ifdef	DEBUG_MALLOC
static int	malloc_level;
#endif
static pid_t	mount_pid;
static int	mount_timeout 	= 30;
static int	reread_config_file = 0;
static int	do_main_poll = 1;

#define	MAXPOLLFD	5

mutex_t		polling_mutex;

bool_t		mount_complete = FALSE;

void
main(int argc, char **argv)
{
	extern void		nfs_program_2(struct svc_req *, SVCXPRT *);
	extern bool_t		vol_init(void);
	extern bool_t		config_read(void);
	extern int		vol_fd;
	SVCXPRT			*xprt;
	struct netconfig	*nconf;
	struct nfs_args		args;
	struct knetconfig	knconf;
	struct stat		sb;
	int			c;
	int			set_my_log = 0;
	struct passwd		*pw;
	struct group		*gr;
	struct sigaction	act;
	int			rpc_fd;
	struct t_bind		*tbind;
	struct rlimit		rlim;
	char			buf[BUFSIZ];
	struct vol_str		volstr;
	sec_data_t		secdata;
	char			mntopts[MNTMAXSTR];

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog_name = argv[0];



	/* argument processing */
	while ((c = getopt(argc, argv, "vtf:d:pl:L:m:g:no:G:P:")) != -1) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 't':
			trace++;
			break;
		case 'f':
			vold_config = (char *)optarg;
			break;
		case 'd':
			vold_root = (char *)optarg;
			break;
		case 'o':
			vold_devdir = (char *)optarg;
			break;
		case 'g':
			volume_group = (char *)optarg;
			break;
		case 'G':
			nisplus_group = (char *)optarg;
			break;
		case 'l':
			set_my_log = 1;
			setlog((char *)optarg);
			break;
		case 'L':
			debug_level = atoi((char *)optarg);
			break;
#ifdef	DEBUG_MALLOC
		case 'm':
			malloc_level = atoi((char *)optarg);
			break;
#endif
		case 'n':
			never_writeback = 1;
			break;
		case 'P':
			vold_polltime = atoi((char *)optarg) * 1000;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	if (set_my_log == 0)
		setlog(DEFAULT_VOLD_LOG);

#ifdef	FULL_DEBUG
	if (verbose == 0) {
		verbose++;
	}
	if (debug_level < 11) {
		debug_level = 11;
	}
	debug(5, "main: debug level %d (verbose = %d)\n",
	    debug_level, verbose);
#endif	/* FULL_DEBUG */

	/* for core dumps... not that we'd have any of those... */
	(void) chdir(vold_devdir);

	/* keep track of what time it is "now" (approx.) */
	(void) gettimeofday(&current_time, NULL);

	openlog(prog_name, LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);
	(void) umask(0);
	(void) setbuf(stdout, (char *)NULL);
	(void) sysinfo(SI_HOSTNAME, self, sizeof (self));

	if (geteuid() != 0) {
		fatal(gettext("Must be root to execute vold\n"));
	}

	/*
	 * Increase file descriptor limit to the most it can possibly
	 * be.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		fatal("getrlimit for fd's failed; %m\n");
	}

	original_nofile = rlim.rlim_cur;
	rlim.rlim_cur = rlim.rlim_max;

	if (setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		fatal("setrlimit for fd's failed; %m\n");
	}

	gr = getgrnam(DEFAULT_GROUP);
	if (gr == NULL) {
		fatal(gettext("Must have the \"%s\" group defined\n"),
		    DEFAULT_GROUP);
	}
	default_gid = gr->gr_gid;

	pw = getpwnam(DEFAULT_USER);
	if (pw == NULL) {
		fatal(gettext("Must have the \"%s\" user defined\n"),
			DEFAULT_USER);
	}
	default_uid = pw->pw_uid;

#ifdef	DEBUG_MALLOC
	if (malloc_level > 0) {
		debug(5, "main: setting malloc debug level to %d\n",
		    malloc_level);
		malloc_debug(malloc_level);
	}
#endif
	/* initialize mutexes/cond-vars */
	(void) mutex_init(&running_mutex, USYNC_THREAD, 0);
	(void) cond_init(&running_cv, USYNC_THREAD, 0);

	/* initialize interface with vol driver */
	if (vol_init() == FALSE) {
		fatal(gettext(
		    "vol_init failed (can't communicate with kernel)\n"));
		/*NOTREACHED*/
	}

	/* read in the config file */
	if (!config_read()) {
		fatal(gettext("vold can't start without a config file\n"));
	}

	nconf = trans_loopback();
	if (nconf == (struct netconfig *)NULL) {
		fatal(gettext("no tpi_clts loopback transport available\n"));
		/*NOTREACHED*/
	}
	if ((rpc_fd = t_open(nconf->nc_device, O_RDWR,
	    (struct t_info *)NULL)) < 0) {
		fatal(gettext("unable to t_open \"%s\"\n"), nconf->nc_device);
		/*NOTREACHED*/
	}

	/*
	 * Negotiate for returning the uid of the caller.
	 * This should be done before enabling the endpoint for
	 * service via t_bind() (called in svc_tli_create())
	 * so that requests to vold contain the uid.
	 */
	if (__rpc_negotiate_uid(rpc_fd) != 0) {
		t_close(rpc_fd);
		fatal(gettext(
		"Couldn't negotiate for uid with loopback transport %s\n"),
			nconf->nc_netid);
		/* NOT REACHED */
	}

	/*LINTED alignment ok*/
	if ((tbind = (struct t_bind *)t_alloc(rpc_fd, T_BIND,
	    T_ALL)) == NULL) {
		fatal(gettext("unable to t_alloc\n"));
		/*NOTREACHED*/
	}
	tbind->qlen = 1;
	trans_netbuf(nconf, &tbind->addr);
	xprt = svc_tli_create(rpc_fd, nconf, tbind, 0, 0);
	if (xprt == (SVCXPRT *) NULL) {
		fatal(
		    gettext("svc_tli_create: Cannot create server handle\n"));
		/*NOTREACHED*/
	}
	if (!svc_reg(xprt, NFS_PROGRAM, NFS_VERSION, nfs_program_2,
		(struct netconfig *)0)) {
		fatal(gettext("Could not register RPC service\n"));
		/*NOTREACHED*/
	}

	/*
	 * ensure the root node is set up before using it
	 *
	 * XXX: the only case that this seems to apply to is the one where
	 * no floppy or CD-ROM (or otherwise normal device) is present,
	 * but the pcmem forcload=TRUE option has loaded the dev_pcmem
	 * DSO (see bug id# 1244293)
	 */
	if (root == NULL) {
		debug(5, "main: have to set up root vvnode myself!?\n");
		db_root();			/* funky but true */
	}

	/*
	 *  Fork vold
	 *  For debugging, the sense of this is backwards -- here we fork
	 *  the mount half rather than the work half (so we can use dbx
	 *  easily).
	 */
	switch (mount_pid = fork()) {
	case -1:
		fatal(gettext("Cannot fork; %m\n"));
		/*NOTREACHED*/
	case 0:
		(void) memset(&args, 0, sizeof (args));
		(void) memset(&knconf, 0, sizeof (knconf));

		/* child */

		/*
		 * NFSMNT_NOAC flag needs to be turned off when NFS client
		 * side bugid 1110389 is fixed.
		 *
		 * NOTE: as of s494-ea, the NFSMNT_NOAC flag can NOT
		 *	be used, as it doesn't seem to be fully implemented.
		 *
		 * 10/14/94: symlinks seem to be hosed in 2.4 (NFS seems to
		 *	be caching READLINKs, so on goes NFSMNT_NOAC again
		 *	(see bug id# 1179769) -- also, 1110389 has long-since
		 *	been fixed.
		 */
		args.flags = NFSMNT_INT | NFSMNT_TIMEO | NFSMNT_RETRANS |
		    NFSMNT_HOSTNAME | NFSMNT_NOAC;
		args.addr = &xprt->xp_ltaddr;

		if (stat(nconf->nc_device, &sb) < 0) {
			fatal(gettext("Couldn't stat \"%s\"; %m\n"),
			    nconf->nc_device);
			/*NOTREACHED*/
		}
		knconf.knc_semantics = nconf->nc_semantics;
		knconf.knc_protofmly = nconf->nc_protofmly;
		knconf.knc_proto = nconf->nc_proto;
		knconf.knc_rdev = sb.st_rdev;
		args.flags |= NFSMNT_KNCONF;
		args.knconf = &knconf;

		args.timeo = (mount_timeout + 5) * 10;
		args.retrans = 5;
		args.hostname = strdup("for volume management (/vol)");
		args.netname = strdup("");
#ifdef notdef
		/* XXX: why are these here, and why are they not needed?? */
		args.acregmin = 1;
		args.acregmax = 1;
		args.acdirmin = 1;
		args.acdirmax = 1;
#endif
		/* ensure the root node is set up before using it */
		ASSERT(root != NULL);
		args.fh = (caddr_t)&root->vn_fh;

		/*
		 * Check to see mount point is there...
		 */
		if (stat(vold_root, &sb) < 0) {
			if (errno == ENOENT) {
				info(gettext("%s did not exist: creating\n"),
				    vold_root);
				if (makepath(vold_root, 0755) < 0) {
					fatal(gettext(
					"can't make directory \"%s\"; %m\n"),
					    vold_root);
				}
			} else {
				fatal("can't stat \"%s\"; %m\n", vold_root);
				/*NOTREACHED*/
			}
		} else if (!(sb.st_mode & S_IFDIR)) {
			/* ...and that it's a directory. */
			fatal(gettext("\"%s\" is not a directory\n"),
			    vold_root);
			/*NOTREACHED*/
		}

		args.flags |= NFSMNT_NEWARGS;
		secdata.secmod = AUTH_LOOPBACK;
		secdata.rpcflavor = AUTH_LOOPBACK;
		secdata.flags = 0;
		secdata.data = NULL;

		args.nfs_args_ext = NFS_ARGS_EXTB;
		args.nfs_ext_u.nfs_extB.secdata = &secdata;
		args.nfs_ext_u.nfs_extB.next = NULL;

		/*
		 * it's not really mounted until /etc/mnttab says so
		 */
		(void) sprintf(buf, "%s:vold(pid%ld)", self, getppid());

		/*
		 * mount "/vol" -- this will block until our parent
		 * actually services this request
		 */
		strcpy(mntopts, MNTOPT_IGNORE);
		if (mount(buf, vold_root, MS_DATA|MS_OPTIONSTR, MNTTYPE_NFS,
		    &args, sizeof (args), mntopts, MNTMAXSTR) < 0) {
			if (errno == EBUSY) {
				warning(gettext("vold restarted\n"));
			} else {
				warning(gettext("Can't mount \"%s\"; %m\n"),
				    vold_root);
			}
			exit(1);
			/*NOTREACHED*/
		}

		exit(0);
		/*NOTREACHED*/
	}

	/* parent */

	(void) setsid();

	/* set up our signal handlers */
	act.sa_handler = catch_n_return;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGCHLD, &act, NULL);

	act.sa_handler = catch;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGTERM, &act, NULL);

	act.sa_handler = reread_config;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGHUP, &act, NULL);

	act.sa_handler = catch;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGINT, &act, NULL);

	act.sa_handler = catch_n_exit;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGUSR1, &act, NULL);

#ifdef	DEBUG
	act.sa_handler = catch_n_exit;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART|SA_SIGINFO;
	(void) sigaction(SIGSEGV, &act, NULL);
#endif

	act.sa_handler = catch_n_return;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;	/* no restart!! */
	(void) sigaction(SIGUSR2, &act, NULL);

	/*
	 * tell vol driver about where our root is
	 */

	volstr.data = vold_root;
	volstr.data_len = strlen(vold_root);

	if (ioctl(vol_fd, VOLIOCDROOT, &volstr) != 0) {
		fatal(gettext("can't set vol root to \"%s\"; %m\n"),
		    vold_root);
		/*NOTREACHED*/
	}

	/* do the real work */
	vold_run();
	fatal(gettext("vold_run returned!\n"));
	/*NOTREACHED*/
}

/*
 * Get a netconfig entry for loopback transport
 */
static struct netconfig *
trans_loopback(void)
{
	struct netconfig	*nconf;
	NCONF_HANDLE		*nc;


	nc = setnetconfig();
	if (nc == NULL)
		return (NULL);

	while (nconf = getnetconfig(nc)) {
		if (nconf->nc_flag & NC_VISIBLE &&
		    nconf->nc_semantics == NC_TPI_CLTS &&
		    strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			nconf = getnetconfigent(nconf->nc_netid);
			break;
		}
	}

	endnetconfig(nc);
	return (nconf);
}

static void
trans_netbuf(struct netconfig *nconf, struct netbuf *np)
{
	struct nd_hostserv	nd_hostserv;
	struct nd_addrlist	*nas;

	nd_hostserv.h_host = self;
	nd_hostserv.h_serv = DEFAULT_SERVICE;

	if (!netdir_getbyname(nconf, &nd_hostserv, &nas)) {
		np->len = nas->n_addrs->len;
		(void) memcpy(np->buf, nas->n_addrs->buf,
		    (int)nas->n_addrs->len);
		netdir_free((char *)nas, ND_ADDRLIST);
	} else {
		fatal(gettext("No service found for %s on transport %s\n"),
			DEFAULT_SERVICE, nconf->nc_netid);
		/*NOTREACHED*/
	}

}


/*
 * main loop for the volume daemon.
 */

/*
 * egad... what a clever... well...
 * The problem is that it's impossible to write a fully MT program
 * at this time because several of the libraries that I depend on
 * (e.g. the rpc library) are not MT safe.  So, we suffer from a
 * very partial MT job.  The main significance here is that poll(2)
 * relies on SIGCHLD to kick us out of a system call.  It's much more
 * efficient (in terms of wall clock time) to do this than just
 * poll for some number of seconds.  The trouble is that the child can
 * end and it's signal be generated and caught before we make it back
 * to poll.  So, we have this flag that gets set to zero anytime
 * a sigcld is recieved.  It's set to one just before the reaping is
 * done.  The only window left :-( is between the if(do_main_poll) and
 * the entry into the system call.  It's a small number of instructions...
 * To cover this possibility, I have a 30 second timeout for the poll :-(.
 * Once in a blue moon, someone may run into a 30 second delay.
 * The trade-off is performance of the overall system (dumb daemon
 * wakes up all the time) vs waiting a bit every once in a rare
 * while.
 */

static void
vold_run(void)
{
	extern void	svc_getreq_common(const int);
	extern void	vol_readevents(void);
	extern void	vol_async(void);
	extern bool_t	config_read(void);
	extern	int	vol_fd;
	static int	n;
	static int	i;
	static int	rpc_fd;
	static size_t	npollfd = 0;
	static struct	pollfd	poll_fds[MAXPOLLFD];

	for (i = 0; i < svc_max_pollfd; i++) {
		if (svc_pollfd[i].fd >= 0) {
			rpc_fd = i;
			break;
		}
	}
	info(gettext("vold: running\n"));

	/* let the threads GO */
	if (vold_running == 0) {
		(void) mutex_lock(&running_mutex);
		vold_running = 1;
		(void) cond_broadcast(&running_cv);
		(void) mutex_unlock(&running_mutex);
	}

	poll_fds[npollfd].fd = vol_fd;
	poll_fds[npollfd].events = POLLRDNORM;
	npollfd++;
	poll_fds[npollfd].fd = rpc_fd;
	poll_fds[npollfd].events = POLLIN|POLLRDNORM|POLLRDBAND;
	npollfd++;

	/* init our polling mutex */
	(void) mutex_init(&polling_mutex, USYNC_THREAD, 0);

	/* handle events forever */
	for (;;) {

		/* wait until something happens */
		if (do_main_poll) {
#ifdef	DEBUG
			debug(12, "vold_run: about to poll()\n");
#endif
			n = poll(poll_fds, npollfd, vold_polltime);
#ifdef	DEBUG
			debug(12, "vold_run: poll() returned %d (errno %d)\n",
			    n, errno);
#endif
		}

		(void) mutex_lock(&polling_mutex);

		/* update idea of the "now" */
		(void) gettimeofday(&current_time, NULL);

		/*
		 * Is there work to do?
		 */
		if (n > 0) {

			/* there is work to do -- look at each possible fd */
			for (i = 0; n && i < npollfd; i++) {

				/* does this fd have a read event ? */
				if (poll_fds[i].revents == 0) {
					continue; /* no event for this fd */
				}

				if (poll_fds[i].fd == rpc_fd) {

					/* this is an NFS event */
					svc_getreq_common(rpc_fd);

				} else if (poll_fds[i].fd == vol_fd) {

					/* this is a volctl event */
					vol_readevents();

				}

			}

		} else if (n < 0) {

			/* poll() had an error */

			if (errno == EINTR) {
				debug(10, "vold_run: poll interrupted\n");
			} else {
				debug(10,
				    "vold_run: poll failed (errno %d)\n",
				    errno);
			}
		}

		if (reread_config_file) {
			(void) config_read();	/* check for changes */
			reread_config_file = 0;
		}

		if (!mount_complete) {
			int	stat;

			/*
			 * the child we forked to do the mount() may not be
			 * done yet, so wait until it is
			 */
			if (waitpid(mount_pid, &stat, WNOHANG) == mount_pid) {
				if (WIFEXITED(stat) &&
				    (WEXITSTATUS(stat) == 0)) {
					mount_complete = TRUE;
				} else {
					fatal(gettext(
					    "mounting of \"%s\" failed\n"),
					    vold_root);
					/*NOTREACHED*/
				}
			}
		}

		do_main_poll = 1;

		if (mount_complete) {
			/*
			 * don't want to process async tasks (such as
			 * media insertion) until the NFS server is ready
			 * to handle request
			 */
			vol_async(); 	/* do any async tasks */
		}

		(void) mutex_unlock(&polling_mutex);
	}
	/*NOTREACHED*/
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    gettext("usage: %s\n"), prog_name);
	(void) fprintf(stderr,
	    gettext("\t[-v]\t\tverbose status information\n"));
	(void) fprintf(stderr,
	    gettext("\t[-t]\t\tunfs server trace information\n"));
	(void) fprintf(stderr,
	    gettext("\t[-f pathname]\talternate vold.conf file\n"));
	(void) fprintf(stderr,
	    gettext("\t[-d directory]\talternate /vol directory\n"));
	(void) fprintf(stderr,
	    gettext("\t[-l logfile]\tplace to put log messages\n"));
	(void) fprintf(stderr,
	    gettext("\t[-L loglevel]\tlevel of debug information\n"));
#ifdef	DEBUG_MALLOC
	(void) fprintf(stderr,
	    gettext("\t[-m mlevel]\tlevel of malloc debug info\n"));
#endif
	(void) fprintf(stderr, "\n");
	exit(1);
}


void
catch(void)
{
	extern int	umount_all(char *);
	pid_t		parentpid = getpid();
	pid_t		fork_ret;
	int		err;


	fork_ret = fork1();
	if (fork_ret != 0) {
		if (fork_ret < 0) {
			warning(gettext("Can't fork; %m\n"));
		}
#ifdef	DEBUG
		else {
			debug(1,
			    "catch(): pid %d created pid %d\n", getpid(),
			    fork_ret);
		}
#endif
		return;
	}

	/* in child now */

	if ((err = umount_all(vold_root)) != 0) {
		syslog(LOG_ERR,
		    gettext("problem unmounting %s; %m\n"), vold_root);
	} else {
		/* nail vold with a -9 */
#ifdef DEBUG
		(void) fprintf(stderr, "Killing pid %d\n", parentpid);
#endif
		(void) kill(parentpid, SIGKILL);
	}
	exit(err);
}


/*
 * Exit from the thread.
 */
void
catch_n_exit(void)
{
	extern void	flushlog(void);


	flushlog();
	if (thr_self() > 1) {
		debug(1, "thread %d exiting\n", thr_self());
		thr_exit(NULL);
	}
	warning(gettext("volume management exiting\n"));
	exit(0);
}

/*
 * don't do anything but set this flag...
 */
/*ARGSUSED*/
void
catch_n_return(int sig, siginfo_t *si, ucontext_t *uc)
{
	do_main_poll = 0;
}

/*
 * Signal to reread the configuration file
 */
void
reread_config(void)
{
	reread_config_file = 1;
}
