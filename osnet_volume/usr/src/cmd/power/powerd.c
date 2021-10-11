/*
 * Copyright (c) 1994 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)powerd.c	1.30	99/10/20 SMI"

#include <stdio.h>			/* Standard */
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <procfs.h>
#include <dirent.h>
#include <thread.h>
#include <limits.h>
#include <sys/todio.h>			/* Time-Of-Day chip */
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>			/* IPC functions */
#include <sys/shm.h>
#include <signal.h>			/* signal handling */
#include <syslog.h>
#include <unistd.h>
#include <libdevinfo.h>
#include <poll.h>
#include <sys/pm.h>			/* power management driver */
#include <sys/uadmin.h>
#include <sys/openpromio.h>		/* for prom access */
#include <sys/sysmacros.h>		/* for MIN & MAX macros */
#include <sys/resource.h>		/* for "getrlimit" */
#include <sys/modctl.h>
#include <sys/stropts.h>		/* for INFTIM */
#include <sys/pbio.h>
#include <sys/cpr.h>

#include "powerd.h"

/* External Functions */
extern struct tm *localtime_r(const time_t *, struct tm *);
extern void sysstat_init(void);
extern int check_tty(hrtime_t *, int);
extern int check_disks(hrtime_t *, int);
extern int check_load_ave(hrtime_t *, float);
extern int check_nfs(hrtime_t *, int);
extern int last_disk_activity(hrtime_t *, int);
extern int last_tty_activity(hrtime_t *, int);
extern int last_load_ave_activity(hrtime_t *);
extern int last_nfs_activity(hrtime_t *, int);
extern int modctl(int, ...);

#define	PM		"/dev/pm"
#define	TOD		"/dev/tod"
#define	PROM		"/dev/openprom"
#define	PB		"/dev/power_button"
#define	LOGFILE		"./powerd.log"

#define	PBM_THREAD	0
#define	ATTACH_THREAD	1
#define	NUM_THREADS	2

#define	CHECK_INTERVAL	5
#define	IDLECHK_INTERVAL	15
#define	MINS_TO_SECS	60
#define	HOURS_TO_SECS	(60 * 60)
#define	DAYS_TO_SECS	(24 * 60 * 60)
#define	HOURS_TO_MINS	60
#define	DAYS_TO_MINS	(24 * 60)

#define	LIFETIME_SECS			(7 * 365 * DAYS_TO_SECS)
#define	DEFAULT_POWER_CYCLE_LIMIT	10000
#define	DEFAULT_SYSTEM_BOARD_DATE	804582000	/* July 1, 1995 */

#define	LOGERROR(m)	if (broadcast) {				\
				syslog(LOG_ERR, (m));			\
			}

typedef	enum {root, options} prom_node_t;

/* State Variables */
static struct cprconfig	asinfo;
static time_t		shutdown_time;	/* Time for next shutdown check */
static time_t		checkidle_time;	/* Time for next idleness check */
static time_t		last_resume;
pwr_info_t		*info;		/* private as config data buffer */
pid_t			*saved_pid;
static int		pb_fd;		/* power button driver */
static int		shmid;		/* Shared memory id */
static int		broadcast;	/* Enables syslog messages */
static int		start_calc;
static int		autoshutdown_en;
static int		do_idlecheck;
static int		got_sighup;
static int		estar_v2_prop;
static int		estar_v3_prop;
static int		log_power_cycles_error = 0;
static int		log_system_board_date_error = 0;
static int		log_no_autoshutdown_warning = 0;
static mutex_t		poweroff_mutex;
static thread_t		tid[NUM_THREADS];

#define	NUMARGS		5
char	*autoshutdown_cmd[NUMARGS] = {
	"/usr/openwin/bin/sys-suspend",
	"-n",
	"-d",
	":0",
	NULL
};
char	*power_button_cmd[NUMARGS] = {
	"/usr/openwin/bin/sys-suspend",
	"-h",
	"-d",
	":0",
	NULL
};

static int		devices_attached = 0;

/* Local Functions */
static void alarm_handler(int);
static void thaw_handler(int);
static void kill_handler(int);
static void work_handler(int);
static void check_shutdown(time_t *, hrtime_t *);
static void check_idleness(time_t *, hrtime_t *);
static int last_system_activity(hrtime_t *);
static int run_idlecheck(void);
static void set_alarm(time_t);
static int poweroff(char *, char **);
static int is_ok2shutdown(time_t *);
static int get_prom(int, prom_node_t, char *, char *);
#ifdef SETPROM
static int set_prom(int, char *, char *);
#endif
static void power_button_monitor(void *);
static void *attach_devices(void *);
static int process_path(char **);

int
main(int argc, char *argv[])
{
	pid_t		pid;
	key_t		key;
	int		pm_fd;
	struct sigaction act;
	sigset_t	sigmask;
	int		c;
	char		errmsg[PATH_MAX + 64];
	int		fd;
	struct rlimit	rl;
	psinfo_t	ps_info;
	int		ps_fd;
	char		ps_path[128];
	struct shmid_ds	shm_stat;

	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s: Must be root\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if ((key = ftok(PM, 'P')) < 0) {
		(void) fprintf(stderr, "%s: Unable to access %s\n",
		    argv[0], PM);
		exit(EXIT_FAILURE);
	}

	/*
	 * Check for left over IPC state
	 */
	shmid = shmget(key, sizeof (pid_t), SHM_RDONLY);
	if (shmid >= 0) {
		saved_pid = (pid_t *)shmat(shmid, NULL, SHM_RDONLY);
		if (saved_pid != (pid_t *)-1) {
			/*
			 * If shm is corrupted, fix it
			 */
			if (shmctl(shmid, IPC_STAT, &shm_stat) < 0) {
				fprintf(stderr, "%s: Can't do IPC_STAT\n",
				    argv[0]);
				perror(NULL);
				(void) shmdt((void *)saved_pid);
				exit(EXIT_FAILURE);
			}

			if (shm_stat.shm_perm.cuid != 0) {
				(void) shmdt((void *)saved_pid);
				if (shmctl(shmid, IPC_RMID, NULL) < 0) {
					(void) fprintf(stderr,
		"%s: Unable to remove old shared memory\n", argv[0]);
					exit(EXIT_FAILURE);
				}
			} else {
				/*
				 * exit if already a 'powerd' is running
				 */
				sprintf(ps_path, "/proc/%d/psinfo", *saved_pid);
				if (((ps_fd =
				    open(ps_path, O_RDONLY | O_NDELAY)) > 0) &&
				    (read(ps_fd, &ps_info, sizeof (ps_info))
					== sizeof (ps_info)) &&
				    (ps_info.pr_nlwp != 0)) {
				    close(ps_fd);
				    (void) fprintf(stderr,
					"%s: Another daemon is running\n",
					argv[0]);
				    exit(EXIT_FAILURE);
				}
				/*
				 * in case previous 'powerd' became <defunct>
				 */
				if (ps_fd > 0)
					close(ps_fd);

				(void) shmdt((void *)saved_pid);
				if (shmctl(shmid, IPC_RMID, NULL) < 0) {
					(void) fprintf(stderr,
					"%s: Unable to remove shared memory\n",
					argv[0]);
				}
			}
		}
	}

	/*
	 * Process options
	 */
	broadcast = 1;
	while ((c = getopt(argc, argv, "n")) != EOF) {
		switch (c) {
		case 'n':
			broadcast = 0;
			break;
		case '?':
			(void) fprintf(stderr, "Usage: %s [-n]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	pm_fd = open(PM, O_RDWR);
	if (pm_fd == -1) {
		(void) sprintf(errmsg, "%s: %s", argv[0], PM);
		perror(errmsg);
		exit(EXIT_FAILURE);
	}
	(void) close(pm_fd);

	/*
	 * Initialize mutex lock used to insure only one command to
	 * run at a time.
	 */
	if (mutex_init(&poweroff_mutex, USYNC_THREAD, NULL) != 0) {
		(void) fprintf(stderr,
			"%s: Unable to initialize mutex lock\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/*
	 * Initialise shared memory
	 */
	shmid = shmget(key, sizeof (pid_t), IPC_CREAT | IPC_EXCL | 0644);
	if (shmid < 0) {
		if (errno != EEXIST) {
			(void) sprintf(errmsg, "%s: shmget", argv[0]);
			perror(errmsg);
		} else
			(void) fprintf(stderr,
			    "%s: Another daemon is running\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	saved_pid = (pid_t *)shmat(shmid, NULL, 0644);
	if (saved_pid == (pid_t *)-1) {
		(void) sprintf(errmsg, "%s: shmat", argv[0]);
		perror(errmsg);
		(void) shmctl(shmid, IPC_RMID, NULL);
		exit(EXIT_FAILURE);
	}

	if ((info = (pwr_info_t *)malloc(sizeof (pwr_info_t))) == NULL) {
		(void) sprintf(errmsg, "%s: malloc", argv[0]);
		perror(errmsg);
		(void) shmdt((void *)saved_pid);
		(void) shmctl(shmid, IPC_RMID, NULL);
		exit(EXIT_FAILURE);
	}

	/*
	 * Daemon is set to go...
	 */
	if ((pid = fork()) < 0)
		exit(EXIT_FAILURE);
	else if (pid != 0)
		exit(EXIT_SUCCESS);

	/*
	 * Close all the parent's file descriptors (Bug 1225843).
	 */
	(void) getrlimit(RLIMIT_NOFILE, &rl);
	for (fd = 0; fd < rl.rlim_cur; fd++)
		(void) close(fd);

	pid = getpid();
	(void) setsid();
	(void) chdir("/");
	(void) umask(0);
	openlog(argv[0], 0, LOG_DAEMON);
#ifdef DEBUG
	/*
	 * Connect stdout to the console.
	 */
	if (dup2(open("/dev/console", O_WRONLY|O_NOCTTY), 1) == -1) {
		LOGERROR("Unable to connect to the console.");
	}
#endif
	*saved_pid = pid;

	info->pd_flags = PD_AC;
	info->pd_idle_time = -1;
	info->pd_start_time = 0;
	info->pd_finish_time = 0;

	/*
	 * If "power_button" device node can be opened, create a new
	 * thread to monitor the power button.
	 */
	if ((pb_fd = open(PB, O_RDONLY)) != -1) {
		if (thr_create(NULL, NULL,
		    (void *(*)(void *))power_button_monitor, NULL,
		    THR_DAEMON, &tid[PBM_THREAD]) != 0) {
			LOGERROR("Unable to monitor system's power button.");
		}
	}

	/*
	 * Setup for gathering system's statistic.
	 */
	sysstat_init();

	act.sa_handler = kill_handler;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void) sigaction(SIGQUIT, &act, NULL);
	(void) sigaction(SIGINT, &act, NULL);
	(void) sigaction(SIGTERM, &act, NULL);

	act.sa_handler = alarm_handler;
	(void) sigaction(SIGALRM, &act, NULL);

	act.sa_handler = work_handler;
	(void) sigaction(SIGHUP, &act, NULL);

	act.sa_handler = thaw_handler;
	(void) sigaction(SIGTHAW, &act, NULL);

	work_handler(SIGHUP);

	/*
	 * Wait for signal to read file
	 */
	(void) sigfillset(&sigmask);
	(void) sigdelset(&sigmask, SIGQUIT);
	(void) sigdelset(&sigmask, SIGINT);
	(void) sigdelset(&sigmask, SIGHUP);
	(void) sigdelset(&sigmask, SIGTERM);
	(void) sigdelset(&sigmask, SIGALRM);
	(void) sigdelset(&sigmask, SIGTHAW);
	(void) thr_sigsetmask(SIG_SETMASK, &sigmask, NULL);
	do {
		(void) sigsuspend(&sigmask);
	} while (errno == EINTR);
	return (1);
}

/*ARGSUSED*/
static void
thaw_handler(int sig)
{
	start_calc  = 0;
	last_resume = time(NULL);
}

/*ARGSUSED*/
static void
kill_handler(int sig)
{
	int ret_code = EXIT_SUCCESS;
	int pm_fd;

	pm_fd = open(PM, O_RDWR);

	/*
	 * Remove all the power-managed devices and that brings
	 * them back to the normal power mode.
	 */
	if (ioctl(pm_fd, PM_REM_DEVICES, NULL) == -1) {
		LOGERROR("Unable to remove power-managed devices.");
		ret_code = EXIT_FAILURE;
	}

	/*
	 * Free resources
	 */
	(void) shmdt((void *)saved_pid);
	(void) shmctl(shmid, IPC_RMID, NULL);
	free(info);
	(void) close(pm_fd);
	if (pb_fd != -1) {
		(void) thr_kill(tid[PBM_THREAD], SIGKILL);
		(void) close(pb_fd);
	}
	if (!thr_kill(tid[ATTACH_THREAD], 0))
		(void) thr_kill(tid[ATTACH_THREAD], SIGKILL);
	(void) mutex_destroy(&poweroff_mutex);
	closelog();
	exit(ret_code);
}

/*ARGSUSED*/
static void
alarm_handler(int sig)
{
	time_t		now;
	hrtime_t	hr_now;

	now = time(NULL);
	hr_now = gethrtime();
	if (checkidle_time <= now && checkidle_time != 0)
		check_idleness(&now, &hr_now);
	if (shutdown_time <= now && shutdown_time != 0)
		check_shutdown(&now, &hr_now);

	set_alarm(now);
}

/*ARGSUSED*/
static void
work_handler(int sig)
{
	time_t		now;
	hrtime_t	hr_now;
	char		inbuf[MAXPATHLEN];
	int		asfd;
	struct stat	stat_buf;

	do_idlecheck = 0;
	info->pd_flags = PD_AC;

	/*
	 * Parse the config file for autoshutdown and idleness entries.
	 */
	if ((asfd = open(CPR_CONFIG, O_RDONLY)) < 0) {
		return;
	}
	if (read(asfd, (void *)&asinfo, sizeof (asinfo)) != sizeof (asinfo)) {
		close(asfd);
		return;
	}
	(void) close(asfd);

	/*
	 * Since Oct. 1, 1995, any new system shipped had root
	 * property "energystar-v2" defined in its prom.  Systems
	 * shipped after July 1, 1999, will have "energystar-v3"
	 * property.
	 */
	estar_v2_prop = asinfo.is_cpr_default;
	estar_v3_prop = asinfo.is_autopm_default;

	info->pd_flags |= asinfo.is_autowakeup_capable;


	if (strlen(asinfo.idlecheck_path) > 0) {
		if (stat(asinfo.idlecheck_path, &stat_buf) != 0) {
			(void) sprintf(inbuf, "unable to access "
				"idlecheck program \"%s\".",
				asinfo.idlecheck_path);
			LOGERROR(inbuf)
		} else if (!(stat_buf.st_mode & S_IXUSR)) {
			(void) sprintf(inbuf, "idlecheck program "
				"\"%s\" is not executable.",
				asinfo.idlecheck_path);
			LOGERROR(inbuf)
		} else {
			do_idlecheck = 1;
		}
	}

	if (strlen(asinfo.as_behavior) == 0 ||
	    strcmp(asinfo.as_behavior, "noshutdown") == 0 ||
	    strcmp(asinfo.as_behavior, "unconfigured") == 0) {
		info->pd_autoshutdown = 0;
	} else if (strcmp(asinfo.as_behavior, "default") == 0) {
		info->pd_autoshutdown = estar_v2_prop;
	} else if (strcmp(asinfo.as_behavior, "shutdown") == 0 ||
		strcmp(asinfo.as_behavior, "autowakeup") == 0) {
		info->pd_autoshutdown = asinfo.is_cpr_capable;
	} else {
		sprintf(inbuf, "autoshutdown behavior \"%s\" unrecognized.",
			asinfo.as_behavior);
		LOGERROR(inbuf);
		info->pd_autoshutdown = 0;
	}

	if (info->pd_autoshutdown) {
		info->pd_idle_time = asinfo.as_idle;
		info->pd_start_time =
		    (asinfo.as_sh * 60 + asinfo.as_sm) % DAYS_TO_MINS;
		info->pd_finish_time =
		    (asinfo.as_fh * 60 + asinfo.as_fm) % DAYS_TO_MINS;
		info->pd_autoresume =
		    (strcmp(asinfo.as_behavior, "autowakeup") == 0) ? 1 : 0;
	}
	autoshutdown_en = (asinfo.as_idle >= 0 && info->pd_autoshutdown)
		? 1 : 0;

#ifdef sparc
	if (!devices_attached &&
	    ((strcmp(asinfo.apm_behavior, "enable") == 0) ||
	    (estar_v3_prop && strcmp(asinfo.apm_behavior, "default") == 0))) {
		if (thr_create(NULL, NULL, attach_devices, NULL,
		    THR_DAEMON, &tid[ATTACH_THREAD]) != 0) {
			LOGERROR("Unable to create thread to attach devices.");
		}
		devices_attached = 1;
	}
#endif

#ifdef DEBUG
	(void) fprintf(stderr, "autoshutdown_en = %d, as_idle = %d, "
			"pd_autoresume = %d\n",
		autoshutdown_en, asinfo.as_idle, info->pd_autoresume);
	(void) fprintf(stderr, " pd_start_time=%d, pd_finish_time=%d\n",
		info->pd_start_time, info->pd_finish_time);
#endif

	got_sighup = 1;
	now = last_resume = time(NULL);
	hr_now = gethrtime();
	check_idleness(&now, &hr_now);
	check_shutdown(&now, &hr_now);
	set_alarm(now);
}

static void
check_shutdown(time_t *now, hrtime_t *hr_now)
{
	int		tod_fd = -1;
	int		pm_fd;
	pm_req_t	pmreq;
	int		kbd, mouse, system, least_idle, idlecheck_time;
	int		next_time;
	int		s, f;
	struct tm	tmp_time;
	time_t		start_of_day, time_since_last_resume;
	time_t		wakeup_time;
	char		errmsg[PATH_MAX + 64];

	if (!autoshutdown_en) {
		shutdown_time = 0;
		return;
	}

	(void) localtime_r(now, &tmp_time);
	tmp_time.tm_sec = 0;
	tmp_time.tm_min = 0;
	tmp_time.tm_hour = 0;
	start_of_day = mktime(&tmp_time);
	s = start_of_day + info->pd_start_time * 60;
	f = start_of_day + info->pd_finish_time * 60;
	if ((s < f && *now >= s && *now < f) ||
	    (s >= f && (*now < f || *now >= s))) {

		if ((pm_fd = open(PM, O_RDONLY)) == -1) {
			LOGERROR("powerd: Open /dev/pm failed.");
			return;
		}
		pmreq.component = 0;
		pmreq.physpath = malloc(strlen("/dev/mouse") + 1);
		(void) strcpy(pmreq.physpath, "/dev/mouse");
		if (process_path(&pmreq.physpath))
			return;
		else if ((mouse =
			ioctl(pm_fd, PM_GET_TIME_IDLE, &pmreq)) == -1) {
			sprintf(errmsg, "powerd: PM_GET_TIME_IDLE on %s "
				"failed.", pmreq.physpath);
			LOGERROR(errmsg);
			close(pm_fd);
			return;
		}

		pmreq.component = 0;
		(void) strcpy(pmreq.physpath, "/dev/kbd");
		if (process_path(&pmreq.physpath))
			return;
		else if ((kbd = ioctl(pm_fd, PM_GET_TIME_IDLE, &pmreq)) == -1) {
			sprintf(errmsg, "powerd: PM_GET_TIME_IDLE on %s "
				"failed.", pmreq.physpath);
			LOGERROR(errmsg);
			close(pm_fd);
			return;
		}
		close(pm_fd);

		system = last_system_activity(hr_now);
		/* who is the last to go idle */
		least_idle = MIN(system, MIN(kbd, mouse));

#ifdef DEBUG
		(void) fprintf(stderr, "conskbd = %s\n", pmreq.physpath);
		(void) fprintf(stderr, "Idle(kbd, mouse, system) = "
			"(%d, %d, %d)\n", kbd, mouse, system);
#endif

		/*
		 * Calculate time_since_last_resume and the next_time
		 * to auto suspend.
		 */
		start_calc = 1;
		time_since_last_resume = time(NULL) - last_resume;
		next_time = info->pd_idle_time * 60 -
				MIN(least_idle, time_since_last_resume);

#ifdef DEBUG
		fprintf(stderr, " check_shutdown: next_time=%d\n",
			next_time);
#endif

		/*
		 * If we have get the SIGTHAW signal at this point - our
		 * calculation of time_since_last_resume is wrong  so
		 * - we need to recalculate.
		 */
		while (start_calc == 0) {
			/* need to redo calculation */
			start_calc = 1;
			time_since_last_resume = time(NULL) - last_resume;
			next_time = info->pd_idle_time * 60 -
				MIN(least_idle, time_since_last_resume);
		}

		/*
		 * Only when everything else is idle, run the user's idlecheck
		 * script.
		 */
		if (next_time <= 0 && do_idlecheck) {
			got_sighup = 0;
			idlecheck_time = run_idlecheck();
			next_time = info->pd_idle_time * 60 -
				MIN(idlecheck_time, MIN(least_idle,
				time_since_last_resume));
			/*
			 * If we have caught SIGTHAW or SIGHUP, need to
			 * recalculate.
			 */
			while (start_calc == 0 || got_sighup == 1) {
				start_calc = 1;
				got_sighup = 0;
				idlecheck_time = run_idlecheck();
				time_since_last_resume = time(NULL) -
					last_resume;
				next_time = info->pd_idle_time * 60 -
					MIN(idlecheck_time, MIN(least_idle,
					time_since_last_resume));
			}
		}

		if (next_time <= 0) {
			if (is_ok2shutdown(now)) {
				/*
				 * Setup the autowakeup alarm.  Clear it
				 * right after poweroff, just in case if
				 * shutdown doesn't go through.
				 */
				if (info->pd_autoresume)
					tod_fd = open(TOD, O_RDWR);
				if (info->pd_autoresume && tod_fd != -1) {
					wakeup_time = (*now < f) ? f :
							(f + DAYS_TO_SECS);
					/*
					 * A software fix for hardware
					 * bug 1217415.
					 */
					if ((wakeup_time - *now) < 180) {
						LOGERROR(
		"Since autowakeup time is less than 3 minutes away, "
		"autoshutdown will not occur.");
						shutdown_time = *now + 180;
						close(tod_fd);
						return;
					}
					if (ioctl(tod_fd, TOD_SET_ALARM,
							&wakeup_time) == -1) {
						LOGERROR("Unable to program "
							"TOD alarm for "
							"autowakeup.");
						close(tod_fd);
						return;
					}
				}

				(void) poweroff("Autoshutdown",
				    autoshutdown_cmd);

				if (info->pd_autoresume && tod_fd != -1) {
					if (ioctl(tod_fd, TOD_CLEAR_ALARM,
							NULL) == -1)
						LOGERROR("Unable to clear "
							"alarm in TOD device.");
					close(tod_fd);
				}

				(void) time(now);
				/* wait at least 5 mins */
				shutdown_time = *now +
					((info->pd_idle_time * 60) > 300 ?
					(info->pd_idle_time * 60) : 300);
			} else {
				/* wait 5 mins */
				shutdown_time = *now + 300;
			}
		} else
			shutdown_time = *now + next_time;
	} else if (s < f && *now >= f) {
		shutdown_time = s + DAYS_TO_SECS;
	} else
		shutdown_time = s;
}

static int
is_ok2shutdown(time_t *now)
{
	int	prom_fd = -1;
	char	power_cycles_st[80];
	char	power_cycle_limit_st[80];
	char	system_board_date_st[80];
	int	power_cycles, power_cycle_limit, free_cycles, scaled_cycles;
	time_t	life_began, life_passed;
	int	no_power_cycles = 0;
	int	no_system_board_date = 0;
	int	ret = 1;

	if (estar_v2_prop == 0) {
		return (1);
	}

	/* CONSTCOND */
	while (1) {
		if ((prom_fd = open(PROM, O_RDWR)) == -1 &&
			(errno == EAGAIN))
				continue;
		break;
	}

	if (get_prom(prom_fd, root, "power-cycle-limit", power_cycle_limit_st)
		== 0) {
		power_cycle_limit = DEFAULT_POWER_CYCLE_LIMIT;
	} else {
		power_cycle_limit = atoi(power_cycle_limit_st);
	}

	/*
	 * Allow 10% of power_cycle_limit as free cycles.
	 */
	free_cycles = power_cycle_limit * 0.1;

	if (get_prom(prom_fd, options, "#power-cycles", power_cycles_st) == 0) {
		no_power_cycles++;
	} else {
		power_cycles = atoi(power_cycles_st);
		if (power_cycles < 0) {
			no_power_cycles++;
		} else if (power_cycles <= free_cycles) {
			goto ckdone;
		}
	}
	if (no_power_cycles && log_power_cycles_error == 0) {
		LOGERROR("No or invalid PROM property \"#power-cycles\" "
				"was found.");
		log_power_cycles_error++;
	}

	if (get_prom(prom_fd, options, "system-board-date",
					system_board_date_st) == 0) {
		no_system_board_date++;
	} else {
		life_began = strtol(system_board_date_st, (char **)NULL, 16);
		if (life_began > *now) {
			no_system_board_date++;
		}
	}
	if (no_system_board_date) {
		if (log_system_board_date_error == 0) {
			LOGERROR("No or invalid PROM property \"system-board-"
					"date\" was found.");
			log_system_board_date_error++;
		}
		life_began = DEFAULT_SYSTEM_BOARD_DATE;
#ifdef SETPROM
		(void) sprintf(system_board_date_st, "%lx", life_began);
		(void) set_prom(prom_fd, "system-board-date",
			system_board_date_st);
#endif
	}

	life_passed = *now - life_began;

	/*
	 * Since we don't keep the date that last free_cycle is ended, we
	 * need to spread (power_cycle_limit - free_cycles) over the entire
	 * 7-year life span instead of (lifetime - date free_cycles ended).
	 */
	scaled_cycles = ((float)life_passed / (float)LIFETIME_SECS) *
				(power_cycle_limit - free_cycles);

	if (no_power_cycles) {
#ifdef SETPROM
		(void) sprintf(power_cycles_st, "%d", scaled_cycles);
		(void) set_prom(prom_fd, "#power-cycles", power_cycles_st);
#endif
		goto ckdone;
	}
#ifdef DEBUG
	(void) fprintf(stderr, "Actual power_cycles = %d\t"
				"Scaled power_cycles = %d\n",
				power_cycles, scaled_cycles);
#endif
	if (power_cycles > scaled_cycles) {
		if (log_no_autoshutdown_warning == 0) {
			LOGERROR("Automatic shutdown has been temporarily "
				"suspended in order to preserve the reliability"
				" of this system.");
			log_no_autoshutdown_warning++;
		}
		ret = 0;
		goto ckdone;
	}

ckdone:
	if (prom_fd != -1)
		close(prom_fd);
	return (ret);
}

static void
check_idleness(time_t *now, hrtime_t *hr_now)
{

	/*
	 * Check idleness only when autoshutdown is enabled.
	 */
	if (!autoshutdown_en) {
		checkidle_time = 0;
		return;
	}

	info->pd_ttychars_idle = check_tty(hr_now, asinfo.ttychars_thold);
	info->pd_loadaverage_idle =
	    check_load_ave(hr_now, asinfo.loadaverage_thold);
	info->pd_diskreads_idle = check_disks(hr_now, asinfo.diskreads_thold);
	info->pd_nfsreqs_idle = check_nfs(hr_now, asinfo.nfsreqs_thold);

#ifdef DEBUG
	(void) fprintf(stderr, "Idle ttychars for %d secs.\n",
			info->pd_ttychars_idle);
	(void) fprintf(stderr, "Idle loadaverage for %d secs.\n",
			info->pd_loadaverage_idle);
	(void) fprintf(stderr, "Idle diskreads for %d secs.\n",
			info->pd_diskreads_idle);
	(void) fprintf(stderr, "Idle nfsreqs for %d secs.\n",
			info->pd_nfsreqs_idle);
#endif

	checkidle_time = *now + IDLECHK_INTERVAL;
}

static int
last_system_activity(hrtime_t *hr_now)
{
	int	act_idle, latest;

	latest = info->pd_idle_time * 60;
	act_idle = last_tty_activity(hr_now, asinfo.ttychars_thold);
	latest = MIN(latest, act_idle);
	act_idle = last_load_ave_activity(hr_now);
	latest = MIN(latest, act_idle);
	act_idle = last_disk_activity(hr_now, asinfo.diskreads_thold);
	latest = MIN(latest, act_idle);
	act_idle = last_nfs_activity(hr_now, asinfo.nfsreqs_thold);
	latest = MIN(latest, act_idle);

	return (latest);
}

static int
run_idlecheck()
{
	char		pm_variable[80], script[80];
	char		*cp;
	int		status;
	pid_t		child;

	/*
	 * Reap any child process which has been left over.
	 */
	while (waitpid((pid_t)-1, &status, WNOHANG) > 0);

	/*
	 * Execute the user's idlecheck script and set variable PM_IDLETIME.
	 * Returned exit value is the idle time in minutes.
	 */
	if ((child = fork1()) == 0) {
		(void) sprintf(pm_variable, "PM_IDLETIME=%d",
			info->pd_idle_time);
		(void) putenv(pm_variable);
		cp = strrchr(asinfo.idlecheck_path, '/');
		(void *) strcpy(script, ++cp);
		(void) execl(asinfo.idlecheck_path, script, NULL);
		exit(-1);
	} else if (child == -1) {
		return (info->pd_idle_time * 60);
	}

	/*
	 * Wait until the idlecheck program completes.
	 */
	if (waitpid(child, &status, 0) != child) {
		/*
		 * We get here if the calling process gets a signal.
		 */
		return (info->pd_idle_time * 60);
	}

	if (WEXITSTATUS(status) < 0) {
		return (info->pd_idle_time * 60);
	} else {
		return (WEXITSTATUS(status) * 60);
	}
}

static void
set_alarm(time_t now)
{
	time_t	itime, stime, next_time, max_time;
	int	next_alarm;

	max_time = MAX(checkidle_time, shutdown_time);
	if (max_time == 0) {
		(void) alarm(0);
		return;
	}
	itime = (checkidle_time == 0) ? max_time : checkidle_time;
	stime = (shutdown_time == 0) ? max_time : shutdown_time;
	next_time = MIN(itime, stime);
	next_alarm = (next_time <= now) ? 1 : (next_time - now);
	(void) alarm(next_alarm);

#ifdef DEBUG
	(void) fprintf(stderr, "Currently @ %s", ctime(&now));
	(void) fprintf(stderr, "Checkidle in %d secs\n", checkidle_time - now);
	(void) fprintf(stderr, "Shutdown  in %d secs\n", shutdown_time - now);
	(void) fprintf(stderr, "Next alarm goes off in %d secs\n", next_alarm);
	(void) fprintf(stderr, "************************************\n");
#endif
}

static int
poweroff(char *msg, char **cmd_argv)
{
	struct stat	statbuf;
	pid_t		pid, child;
	struct passwd	*pwd;
	char		home[256] = "HOME=";
	char		user[256] = "LOGNAME=";
	int		status;

	if (mutex_trylock(&poweroff_mutex) != 0)
		return (0);

	if (stat("/dev/console", &statbuf) == -1 ||
	    (pwd = getpwuid(statbuf.st_uid)) == NULL) {
		mutex_unlock(&poweroff_mutex);
		return (1);
	}

	if (broadcast && msg != NULL)
		(void) syslog(LOG_ERR, msg);

	if (*cmd_argv == NULL) {
		LOGERROR("No command to run.");
		mutex_unlock(&poweroff_mutex);
		return (1);
	}

	/*
	 * Need to simulate the user enviroment, minimaly set HOME, and USER.
	 */
	if ((child = fork1()) == 0) {
		(void) strcat(home, pwd->pw_dir);
		(void) putenv(home);
		(void) strcat(user, pwd->pw_name);
		(void) putenv(user);
		(void) setgid(pwd->pw_gid);
		(void) setuid(pwd->pw_uid);
		(void) execv(cmd_argv[0], cmd_argv);
		exit(EXIT_FAILURE);
	} else if (child == -1) {
		mutex_unlock(&poweroff_mutex);
		return (1);
	}
	pid = 0;
	while (pid != child)
		pid = wait(&status);
	if (WEXITSTATUS(status) == EXIT_FAILURE && broadcast) {
		(void) syslog(LOG_ERR, "Failed to exec \"%s\".", cmd_argv[0]);
		mutex_unlock(&poweroff_mutex);
		return (1);
	}

	mutex_unlock(&poweroff_mutex);
	return (0);
}

#define	PBUFSIZE	256

/*
 * Gets the value of a prom property at either root or options node.  It
 * returns 1 if it is successful, otherwise it returns 0 .
 */
static int
get_prom(int prom_fd, prom_node_t node_name,
	char *property_name, char *property_value)
{
	union {
		char buf[PBUFSIZE + sizeof (uint_t)];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	int	got_it = 0;

	if (prom_fd == -1) {
		return (0);
	}

	switch (node_name) {
	case root:
		(void *) memset(oppbuf.buf, 0, PBUFSIZE);
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMNEXT, opp) < 0) {
			return (0);
		}

		/*
		 * Passing null string will give us the first property.
		 */
		(void *) memset(oppbuf.buf, 0, PBUFSIZE);
		do {
			opp->oprom_size = PBUFSIZE;
			if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0) {
				return (0);
			}
			if (strcmp(opp->oprom_array, property_name) == 0) {
				got_it++;
				break;
			}
		} while (opp->oprom_size > 0);

		if (!got_it) {
			return (0);
		}
		if (got_it && property_value == NULL) {
			return (1);
		}
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMGETPROP, opp) < 0) {
			return (0);
		}
		if (opp->oprom_size == 0) {
			*property_value = '\0';
		} else {
			(void *) strcpy(property_value, opp->oprom_array);
		}
		break;
	case options:
		(void) strcpy(opp->oprom_array, property_name);
		opp->oprom_size = PBUFSIZE;
		if (ioctl(prom_fd, OPROMGETOPT, opp) < 0) {
			return (0);
		}
		if (opp->oprom_size == 0) {
			return (0);
		}
		if (property_value != NULL) {
			(void *) strcpy(property_value, opp->oprom_array);
		}
		break;
	default:
		LOGERROR("Only root node and options node are supported.\n");
		return (0);
	}

	return (1);
}

/*
 *  Sets the given prom property at the options node.  Returns 1 if it is
 *  successful, otherwise it returns 0.
 */
#ifdef SETPROM
static int
set_prom(int prom_fd, char *property_name, char *property_value)
{
	union {
		char buf[PBUFSIZE];
		struct openpromio opp;
	} oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	int name_length;

	if (prom_fd == -1) {
		return (0);
	}

	name_length = strlen(property_name) + 1;
	(void) strcpy(opp->oprom_array, property_name);
	(void) strcpy(opp->oprom_array + name_length, property_value);
	opp->oprom_size = name_length + strlen(property_value);
	if (ioctl(prom_fd, OPROMSETOPT, opp) < 0) {
		return (0);
	}

	/*
	 * Get back the property value in order to verify the set operation.
	 */
	opp->oprom_size = PBUFSIZE;
	if (ioctl(prom_fd, OPROMGETOPT, opp) < 0) {
		return (0);
	}
	if (opp->oprom_size == 0) {
		return (0);
	}
	if (strcmp(opp->oprom_array, property_value) != 0) {
		return (0);
	}

	return (1);
}
#endif  /* SETPROM */

#define	isspace(ch)	((ch) == ' ' || (ch) == '\t')
#define	iseol(ch)	((ch) == '\n' || (ch) == '\r' || (ch) == '\f')

/*ARGSUSED*/
static void
power_button_monitor(void *arg)
{
	sigset_t sigmask;
	struct pollfd pfd;
	int events;

	(void) sigfillset(&sigmask);
	(void) thr_sigsetmask(SIG_SETMASK, &sigmask, NULL);

	if (ioctl(pb_fd, PB_BEGIN_MONITOR, NULL) == -1) {
		LOGERROR("Failed to monitor the power button.");
		thr_exit((void *) 0);
	}

	pfd.fd = pb_fd;
	pfd.events = POLLIN;

	/*CONSTCOND*/
	while (1) {
		if (poll(&pfd, 1, INFTIM) == -1) {
			LOGERROR("Failed to poll for power button events.");
			thr_exit((void *) 0);
		}

		if (!(pfd.revents & POLLIN))
			continue;

		if (ioctl(pfd.fd, PB_GET_EVENTS, &events) == -1) {
			LOGERROR("Failed to get power button events.");
			thr_exit((void *) 0);
		}

		if ((events & PB_BUTTON_PRESS) &&
		    (poweroff(NULL, power_button_cmd) != 0)) {
			LOGERROR("Power button is pressed, powering "
			    "down the system!");

			/*
			 * Send SIGPWR signal to the init process to
			 * shut down the system.
			 */
			if (kill(1, SIGPWR) == -1)
				(void) uadmin(A_SHUTDOWN, AD_POWEROFF, 0);
		}

		/*
		 * Clear any power button event that has happened
		 * meanwhile we were busy processing the last one.
		 */
		if (ioctl(pfd.fd, PB_GET_EVENTS, &events) == -1) {
			LOGERROR("Failed to get power button events.");
			thr_exit((void *) 0);
		}
	}
}

/*ARGSUSED*/
static void *
attach_devices(void *arg)
{
	sigset_t sigmask;
	di_node_t root_node;

	sleep(60);	/* let booting finish first */

	(void) sigfillset(&sigmask);
	(void) thr_sigsetmask(SIG_SETMASK, &sigmask, NULL);

	if ((root_node = di_init("/", DINFOFORCE)) == DI_NODE_NIL) {
		LOGERROR("Failed to attach devices.");
		return (NULL);
	}
	di_fini(root_node);

	/*
	 * Unload all the modules.
	 */
	(void) modctl(MODUNLOAD, 0);

	return (NULL);
}


/*
 * Convert a symbolic link into /devices into the appropriate physical path,
 * and trim leading /devices and trailing minor string from resolved path.
 */
static int
process_path(char **pp)
{
	char pathbuf[PATH_MAX+1];
	char *rp, *cp;
	struct stat st;
	char	errmsg[PATH_MAX + 64];

	st.st_rdev = 0;
	/* if we have a link to a device file */
	if (stat(*pp, &st) == 0 && st.st_rdev != 0) {
		if (realpath(*pp, pathbuf) == NULL) {
			/* this sequence can't happen */
			sprintf(errmsg, "Can't convert %s to realpath.\n", *pp);
			LOGERROR(errmsg);
			return (1);
		}
		if (strncmp("/devices", pathbuf, strlen("/devices")) == 0) {
			cp = pathbuf + strlen("/devices");
			if ((rp = strchr(cp, ':')) != NULL)
				*rp = '\0';
			*pp = realloc(*pp, strlen(cp) + 1);
			if (*pp == NULL) {
				LOGERROR("Out of memory.\n");
				return (1);
			}
			(void) strcpy(*pp, cp);
			return (0);
		}
	}
	sprintf(errmsg, "Can not convert %s to real path.\n", *pp);
	LOGERROR(errmsg);
	return (1);
}
