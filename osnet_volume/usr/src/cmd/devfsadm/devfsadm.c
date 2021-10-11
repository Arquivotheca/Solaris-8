/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devfsadm.c	1.12	99/12/06 SMI"

/*
 * Devfsadm replaces drvconfig, audlinks, disks, tapes, ports, devlinks
 * as a general purpose device administrative utility.	It creates
 * devices special files in /devices and logical links in /dev, and
 * coordinates updates to /etc/path_to_instance with the kernel.  It
 * operates in both command line mode to handle user or script invoked
 * reconfiguration updates, and operates in daemon mode to handle dynamic
 * reconfiguration for hotplugging support.
 */

#include "devfsadm_impl.h"

/* globals */

/* create or remove nodes or links. unset with -n */
static int file_mods = TRUE;

/* cleanup mode.  Set with -C */
static int cleanup = FALSE;

/* devlinks -d compatability */
static int devlinks_debug = FALSE;

/* load a single driver only.  set with -i */
static int single_drv = FALSE;
static char *driver = NULL;

/* attempt to load drivers or defer attach nodes */
static int load_attach_drv = TRUE;

/* set if invoked via /usr/lib/devfsadm/devfsadmd */
static daemon_mode = FALSE;

/* output directed to syslog during daemon mode if set */
static int logflag = FALSE;

/* build links in /dev.  -x to turn off */
static int build_dev = TRUE;

/* build links in /devices.  -y to turn off */
static int build_devices = TRUE;

/* -z to turn off */
static int flush_path_to_inst_enable = TRUE;

/* variables used for path_to_inst flushing */
static int inst_count = 0;
static mutex_t count_lock;
static cond_t cv;

/* variables for minor_fini calling system */
static int minor_fini_timeout = MINOR_FINI_TIMEOUT_DEFAULT;
static mutex_t minor_fini_mutex;
static int minor_fini_thread_created = FALSE;
static int minor_fini_delay_restart = FALSE;

/* prevents calling minor_fini when add_minor_pathname is active */
static sema_t sema_minor;

/* the program we were invoked as; ie argv[0] */
static char *prog;

/* pointers to create/remove link lists */
static create_list_t *create_head = NULL;
static remove_list_t *remove_head = NULL;

/*  supports the class -c option */
static char **classes = NULL;
static int num_classes = 0;

/* used with verbose option -v or -V */
static int num_verbose = 0;
static char **verbose = NULL;

static struct mperm *minor_perms = NULL;
static driver_alias_t *driver_aliases = NULL;

/* set if -r alternate root given */
static char *root_dir = "";

/* /devices or <rootdir>/devices */
static char *devices_dir  = DEVICES;

/* /dev or <rootdir>/dev */
static char *dev_dir = DEV;

/* /etc/path_to_inst unless -p used */
static char *inst_file = INSTANCE_FILE;

/* /usr/lib/devfsadm/linkmods unless -l used */
static char *module_dirs = MODULE_DIRS;

/* default uid/gid used if /etc/minor_perm entry not found */
static uid_t root_uid;
static gid_t sys_gid;

/* root node for device tree snapshow */
static di_node_t root_node = NULL;

/* /etc/devlink.tab unless devlinks -t used */
static char *devlinktab_file = NULL;

/* set if /devices node is new. speeds up rm_stale_links */
static int physpathnew = FALSE;
static int linknew = TRUE;

/* variables for devlink.tab compat processing */
static devlinktab_list_t *devlinktab_list = NULL;
static unsigned int devlinktab_line = 0;

/* cache head for devfsadm_enumerate*() functions */
static numeral_set_t *head_numeral_set = NULL;

/* list list of devfsadm modules */
static module_t *module_head = NULL;

/* name_to_major list used in utility function */
static n2m_t *n2m_list = NULL;

/* cache of some links used for performance */
static linkhead_t *headlinkhead = NULL;

/* locking variables to prevent multiples writes to /dev */
static int hold_dev_lock = FALSE;
static int hold_daemon_lock = FALSE;
static int dev_lock_fd;
static int daemon_lock_fd;
static char dev_lockfile[PATH_MAX + 1];
static char daemon_lockfile[PATH_MAX + 1];

/* last devinfo node/minor processed. used for performance */
static di_node_t lnode;
static di_minor_t lminor;
static char lphy_path[PATH_MAX + 1] = {""};

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	struct group *gp;
	pid_t pid;
	struct sigaction act;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if ((prog = strrchr(argv[0], '/')) == NULL) {
		prog = argv[0];
	} else {
		prog++;
	}

	if (getuid() != 0) {
		err_print(MUST_BE_ROOT);
		devfsadm_exit(1);
	}

	if ((pw = getpwnam(DEFAULT_USER)) != NULL) {
		root_uid = pw->pw_uid;
	} else {
		err_print(CANT_FIND_USER, DEFAULT_USER);
		root_uid = (uid_t)0;	/* assume 0 is root */
	}

	/* the default group is sys */

	if ((gp = getgrnam(DEFAULT_GROUP)) != NULL) {
		sys_gid = gp->gr_gid;
	} else {
		err_print(CANT_FIND_GROUP, DEFAULT_GROUP);
		sys_gid = (gid_t)3;	/* assume 3 is sys */
	}

	(void) umask(0);

	read_minor_perm_file();
	read_driver_aliases_file();

	parse_args(argc, argv);

	/* set up our signal handlers */
	act.sa_handler = catch_sigs;
	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGHUP, &act, NULL) == -1) {
		err_print(SIGACTION_FAILED, strerror(errno));
	}

	(void) sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &act, NULL) == -1) {
		err_print(SIGACTION_FAILED, strerror(errno));
	}

	(void) sema_init(&sema_minor, 1, USYNC_THREAD, NULL);

	if (build_dev == TRUE) {
		/*
		 * devlinktab_file will be NULL if running under any
		 * link generator compat mode other than devlinks.
		 */
		read_devlinktab_file();
		load_modules();
		if (module_head == NULL) {
			if (strcmp(prog, DEVLINKS) == 0) {
				if (devlinktab_list == NULL) {
					err_print(NO_LINKTAB, devlinktab_file);
					err_print(NO_MODULES, module_dirs);
					err_print(ABORTING);
					devfsadm_exit(2);
				}
			} else {
				err_print(NO_MODULES, module_dirs);
				if (strcmp(prog, DEVFSADM) == 0) {
					err_print(MODIFY_PATH);
				}
			}
		}

	}

	if (daemon_mode == TRUE) {
		/*
		 * If reconfig booting, build /dev and /devices before
		 * daemonizing.
		 */
		if (getenv(RECONFIG_BOOT) != NULL) {
			vprint(INFO_MID, CONFIGURING);
			process_devinfo_tree();
		}

		unload_modules();

		/* call di_fini here, so that minor_fini() can use root_node */
		if (root_node != NULL) {
			di_fini(root_node);
			root_node = NULL;
		}

		/*
		 * fork before detaching from tty in order to print error
		 * message if unable to acquire file lock.  locks not preserved
		 * across forks.  Even under debug we want to fork so that
		 * when executed at boot we don't hang.
		 */
		if (fork() != 0) {
			devfsadm_exit(0);
		}

		/* set directory to / so it coredumps there */
		if (chdir("/") == -1) {
			err_print(CHROOT_FAILED, strerror(errno));
		}

		/* only one daemon can run at a time */
		if ((pid = enter_daemon_lock()) == getpid()) {
			thread_t thread;
			detachfromtty();
			(void) cond_init(&cv, USYNC_THREAD, 0);
			(void) mutex_init(&count_lock, USYNC_THREAD, 0);
			if (thr_create(NULL, NULL,
				(void *(*)(void *))instance_flush_thread,
				    NULL,
				    THR_DETACHED,
				    &thread) != 0) {
				err_print(CANT_CREATE_THREAD, "daemon",
					strerror(errno));
				devfsadm_exit(3);
			}
			daemon_update();
		} else {
			err_print(DAEMON_RUNNING, pid);
			devfsadm_exit(4);
		}
		exit_daemon_lock();

	} else {
		/* not a daemon, so just build /dev and /devices */

		process_devinfo_tree();
		unload_modules();

		/* call di_fini here so that minor_fini can use root_node */
		if (root_node != NULL) {
			di_fini(root_node);
			root_node = NULL;
		}
	}
	return (0);
}

/*
 * Parse arguments for all 6 programs handled from devfsadm.
 */
static void
parse_args(int argc, char *argv[])
{
	char opt;
	char get_linkcompat_opts = FALSE;
	char *compat_class;
	int num_aliases = 0;
	int len;
	int retval;
	int add_bind = FALSE;
	struct aliases *ap = NULL;
	struct aliases *a_head = NULL;
	struct aliases *a_tail = NULL;
	struct modconfig mc;

	if (strcmp(prog, DISKS) == 0) {
		compat_class = "disk";
		get_linkcompat_opts = TRUE;

	} else if (strcmp(prog, TAPES) == 0) {
		compat_class = "tape";
		get_linkcompat_opts = TRUE;

	} else if (strcmp(prog, PORTS) == 0) {
		compat_class = "port";
		get_linkcompat_opts = TRUE;

	} else if (strcmp(prog, AUDLINKS) == 0) {
		compat_class = "audio";
		get_linkcompat_opts = TRUE;

	} else if (strcmp(prog, DEVLINKS) == 0) {
		devlinktab_file = DEVLINKTAB_FILE;

		build_devices = FALSE;
		load_attach_drv = FALSE;

		while ((opt = getopt(argc, argv, "dnr:st:vV:")) != EOF) {
			switch (opt) {
			case 'd':
				file_mods = FALSE;
				flush_path_to_inst_enable = FALSE;
				devlinks_debug = TRUE;
				break;
			case 'n':
				/* prevent driver loading and deferred attach */
				load_attach_drv = FALSE;
				break;
			case 'r':
				set_root_devices_dev_dir(optarg);
				break;
			case 's':
				/*
				 * suppress.  don't create/remove links/nodes
				 * useful with -v or -V
				 */
				file_mods = FALSE;
				flush_path_to_inst_enable = FALSE;
				break;
			case 't':
				/* supply a non-default table file */
				devlinktab_file = optarg;
				break;
			case 'v':
				/* documented verbose flag */
				add_verbose_id(VERBOSE_MID);
				break;
			case 'V':
				/* undocumented for extra verbose levels */
				add_verbose_id(optarg);
				break;
			default:
				usage();
				break;
			}
		}

		if (optind < argc) {
			usage();
		}

	} else if (strcmp(prog, DRVCONFIG) == 0) {
		build_dev = FALSE;

		while ((opt =
			getopt(argc, argv, "a:bdc:i:m:np:R:r:svV:")) != EOF) {
			switch (opt) {
			case 'a':
				ap = calloc(sizeof (struct aliases), 1);
				ap->a_name = dequote(optarg);
				len = strlen(ap->a_name) + 1;
				if (len > 256) {
					err_print(ALIAS_TOO_LONG, ap->a_name);
					devfsadm_exit(5);
				}
				ap->a_len = len;
				if (a_tail == NULL) {
					a_head = ap;
				} else {
					a_tail->a_next = ap;
				}
				a_tail = ap;
				num_aliases++;
				add_bind = TRUE;
				break;
			case 'b':
				add_bind = TRUE;
				break;
			case 'c':
				(void) strcpy(mc.drvclass, optarg);
				break;
			case 'd':
				/*
				 * need to keep for compability, but
				 * do nothing.
				 */
				break;
			case 'i':
				single_drv = TRUE;
				(void) strcpy(mc.drvname, optarg);
				driver = s_strdup(optarg);
				break;
			case 'm':
				mc.major = atoi(optarg);
				break;
			case 'n':
				/* prevent driver loading and deferred attach */
				load_attach_drv = FALSE;
				break;
			case 'p':
				/* specify alternate path_to_inst file */
				inst_file = s_strdup(optarg);
				break;
			case 'R':
				/*
				 * Private flag for suninstall to populate
				 * device information on the installed root.
				 */
				root_dir = s_malloc(strlen(optarg) + 1);
				(void) strcpy(root_dir, optarg);
				devfsadm_exit(devfsadm_copy());
				break;
			case 'r':
				devices_dir = s_malloc(strlen(optarg) + 1);
				(void) strcpy(devices_dir, optarg);
				break;
			case 's':
				/*
				 * suppress.  don't create nodes
				 * useful with -v or -V
				 */
				file_mods = FALSE;
				flush_path_to_inst_enable = FALSE;
				break;
			case 'v':
				/* documented verbose flag */
				add_verbose_id(VERBOSE_MID);
				break;
			case 'V':
				/* undocumented for extra verbose levels */
				add_verbose_id(optarg);
				break;
			default:
				usage();
			}
		}

		if (optind < argc) {
			usage();
		}

		if ((add_bind == TRUE) && (mc.major == -1 ||
		    mc.drvname[0] == NULL)) {
			err_print(MAJOR_AND_B_FLAG);
			devfsadm_exit(6);
		}
		if (add_bind == TRUE) {
			mc.num_aliases = num_aliases;
			mc.ap = a_head;
			retval =  modctl(MODADDMAJBIND, NULL, (caddr_t)&mc);
			if (retval < 0) {
				err_print(MODCTL_ADDMAJBIND);
			}
			devfsadm_exit(retval);
		}

	} else if ((strcmp(prog, DEVFSADM) == 0) ||
	    (strcmp(prog, DEVFSADMD) == 0)) {

		if (strcmp(prog, DEVFSADMD) == 0) {
			daemon_mode = TRUE;
		}

		devlinktab_file = DEVLINKTAB_FILE;

		while ((opt = getopt(argc, argv, "Cc:i:l:np:R:r:st:vV:xyz")) !=
		    EOF) {
			switch (opt) {
			case 'C':
				cleanup = TRUE;
				break;
			case 'c':
				num_classes++;
				classes = s_realloc(classes, num_classes *
						    sizeof (char *));
				classes[num_classes - 1] = optarg;
				break;
			case 'i':
				single_drv = TRUE;
				driver = s_strdup(optarg);
				break;
			case 'l':
				/* specify an alternate module load path */
				module_dirs = s_strdup(optarg);
				break;
			case 'n':
				/* prevent driver loading and deferred attach */
				load_attach_drv = FALSE;
				break;
			case 'p':
				/* specify alternate path_to_inst file */
				inst_file = s_strdup(optarg);
				break;
			case 'R':
				/*
				 * Private flag for suninstall to populate
				 * device information on the installed root.
				 */
				root_dir = s_malloc(strlen(optarg) + 1);
				(void) strcpy(root_dir, optarg);
				devfsadm_exit(devfsadm_copy());
				break;
			case 'r':
				set_root_devices_dev_dir(optarg);
				break;
			case 's':
				/*
				 * suppress. don't create/remove links/nodes
				 * useful with -v or -V
				 */
				file_mods = FALSE;
				flush_path_to_inst_enable = FALSE;
				break;
			case 't':
				devlinktab_file = optarg;
				break;
			case 'v':
				/* documented verbose flag */
				add_verbose_id(VERBOSE_MID);
				break;
			case 'V':
				/* undocumented: specify verbose lvl */
				add_verbose_id(optarg);
				break;
			case 'x':
				/* don't build /dev */
				build_dev = FALSE;
				break;
			case 'y':
				/* don't build /devices */
				build_devices = FALSE;
				break;
			case 'z':
				/* don't flush path_to_inst */
				flush_path_to_inst_enable = FALSE;
				break;
			default:
				usage();
				break;
			}

		}
		if (optind < argc) {
			usage();
		}
	}

	if (get_linkcompat_opts == TRUE) {

		build_devices = FALSE;
		load_attach_drv = FALSE;
		num_classes++;
		classes = s_realloc(classes, num_classes *
				    sizeof (char *));
		classes[num_classes - 1] = compat_class;

		while ((opt = getopt(argc, argv, "Cnr:svV:")) != EOF) {
			switch (opt) {
			case 'C':
				cleanup = TRUE;
				break;
			case 'n':
				/* prevent driver loading or deferred attach */
				load_attach_drv = FALSE;
				break;
			case 'r':
				set_root_devices_dev_dir(optarg);
				break;
			case 's':
				/* suppress.  don't create/remove links/nodes */
				/* useful with -v or -V */
				file_mods = FALSE;
				flush_path_to_inst_enable = FALSE;
				break;
			case 'v':
				/* documented verbose flag */
				add_verbose_id(VERBOSE_MID);
				break;
			case 'V':
				/* undocumented for extra verbose levels */
				add_verbose_id(optarg);
				break;
			default:
				usage();
			}
		}
		if (optind < argc) {
			usage();
		}
	}
}

void
usage(void)
{
	if (strcmp(prog, DEVLINKS) == 0) {

		err_print(DEVLINKS_USAGE);
		devfsadm_exit(7);

	} else if (strcmp(prog, DRVCONFIG) == 0) {

		err_print(DRVCONFIG_USAGE);
		devfsadm_exit(8);

	} else if ((strcmp(prog, DEVFSADM) == 0) ||
			(strcmp(prog, DEVFSADMD) == 0)) {
		err_print(DEVFSADM_USAGE);
		devfsadm_exit(9);

	} else {
		err_print(COMPAT_LINK_USAGE);
		devfsadm_exit(10);
	}
}

/*
 * Called in non-daemon mode to take a snap shot of the devinfo tree.
 * Then it calls the appropriate functions to build /devices and /dev.
 * It also flushes path_to_inst.
 */
void
process_devinfo_tree()
{
	char *fcn = "process_devinfo_tree: ";
	uint_t flags = DINFOCPYALL, walk_flags = 0;
	defer_list_t defer_list;
	defer_minor_t *dmp, *odmp;

	vprint(CHATTY_MID, "%senter\n", fcn);

	if (build_dev == TRUE) {
		(void) enter_dev_lock();
	}

	if (load_attach_drv == TRUE) {

		if (single_drv == TRUE) {
			walk_flags |= DI_CHECK_ALIAS;
			vprint(CHATTY_MID, "%sattaching driver (%s)\n",
					fcn, driver);
		} else {
			flags |= DINFOFORCE;
			vprint(CHATTY_MID, "%sattaching all drivers\n", fcn);
		}
	}

	if (single_drv == TRUE) {
		/*
		 * loads a single driver, but returns entire devinfo tree
		 */
		if ((root_node = di_init_driver(driver, flags)) ==
		    DI_NODE_NIL) {
			err_print(DRIVER_FAILED_ATTACH, driver);
			devfsadm_exit(11);
		}
	} else {
		/* Possibly load drvrs, then ret snapshot of entire tree. */
		if ((root_node = di_init("/", flags)) ==
		    DI_NODE_NIL) {
			err_print(DI_INIT_FAILED, "/", strerror(errno));
			devfsadm_exit(12);
		}
	}

	if (((load_attach_drv == TRUE) || (single_drv == TRUE)) &&
	    (build_devices == TRUE)) {
		flush_path_to_inst();
	}

	/* handle pre-cleanup operations desired by the modules. */
	if (build_dev == TRUE) {
		vprint(CHATTY_MID, "attempting pre-cleanup\n");
		pre_and_post_cleanup(RM_PRE);
	}

	/*
	 * Process every minor node except DDM_ALIAS and DDM_INTERNAL_PATH
	 * nodes. However, if minors are being processed for a specific driver,
	 * process DDM_ALIAS minors as well.
	 */
	vprint(CHATTY_MID, "%swalking device tree and creating: %s  %s\n", fcn,
		build_dev == TRUE ? "/dev" : "", build_devices == TRUE ?
		"/devices" : "");

	defer_list.head = defer_list.tail = NULL;
	di_walk_minor(root_node, NULL, walk_flags, &defer_list,
	    check_minor_type);

	/*
	 * Now create the deferred links
	 */
	vprint(CHATTY_MID, "%s creating deferred links\n", fcn);

	assert((defer_list.head == NULL) ^ (defer_list.tail != NULL));
	for (dmp = defer_list.head; dmp != NULL; ) {
		(void) check_minor_type(dmp->node, dmp->minor, NULL);
		odmp = dmp;
		dmp = dmp->next;
		assert(dmp != NULL || odmp == defer_list.tail);
		free(odmp);
	}

	/* handle post-cleanup operations desired by the modules. */
	if (build_dev == TRUE) {
		vprint(CHATTY_MID, "%sattempting post-cleanup\n", fcn);
		pre_and_post_cleanup(RM_POST);
	}

	if (build_dev == TRUE) {
		exit_dev_lock();
	}
}

/*ARGSUSED*/
static void
print_cache_signal(int signo)
{

	if (signal(SIGUSR1, print_cache_signal) == SIG_ERR) {
		err_print("signal SIGUSR1 failed: %s\n", strerror(errno));
		devfsadm_exit(1);
	}
}

/*
 * Register with eventd for messages.  Then go into infinite loop
 * waiting for events and call call_event_handler() for each new event.
 */
static void
daemon_update(void)
{
	char *fcn = "daemon_update: ";
	time_t tstamp;
	event_filter_t *event_handle;
	log_event_request_t req;
	char event_buf[LOGEVENT_BUFSIZE];
	int error;
	int timeout = 1;
	int reconnect;

	vprint(CHATTY_MID, "%senter\n", fcn);

	if (signal(SIGUSR1, print_cache_signal) == SIG_ERR) {
		err_print("signal SIGUSR1 failed: %s\n", strerror(errno));
		devfsadm_exit(1);
	}

	req.event_class = EC_DEVFS;
	req.event_type = NULL;
	req.next = NULL;

	for (;;) {
		vprint(EVENT_MID, "%srequest_log_event\n", fcn);
		while ((event_handle =
		    request_log_event(NULL, &req)) == NULL) {
			error = errno;

			vprint(EVENT_MID, "%srequest_log_event() failed: %s"
				" (%d)\n", fcn, strerror(errno), errno);

			(void) sleep(timeout);

			if (timeout < MAX_SLEEP) {
				timeout *= 2;
			}

			switch (error) {
				case EAGAIN:
				case EBADF:
					err_print(IS_EVENTD_RUNNING);
					break;
				default:
					devfsadm_exit(13);
					break;
			}
		}

		for (reconnect = FALSE; reconnect == FALSE; ) {
			vprint(EVENT_MID, "%sget_log_event\n", fcn);
			if (DEVFSADM_DEBUG_ON == TRUE) {
				/* helpful when  redirecting to file */
				(void) fflush(stdout);
				(void) fflush(stderr);
			}
			switch (get_log_event(event_handle,
			    event_buf, &tstamp)) {

			case 0:		/* message obtained successfully */
				call_event_handler(event_buf);
				break;

			case -1:	/* error */
			default:
				vprint(EVENT_MID,
					"%sget_log_event returns non 0\n", fcn);
				if (errno == ESRCH) {
					vprint(EVENT_MID,
					    "%sdaemon_update: error = %d\n",
						fcn, errno);
					reconnect = TRUE;
				}
				(void) sleep(1);
				break;
			}
		}
		cancel_log_event_request(event_handle);
	}
}

/*
 *  Called by the daemon when it receives an event.  Vectors the event to
 *  the correct handler.
 */
static void
call_event_handler(char *sp)
{
	char *log_event_str[NUM_EV_STR];
	log_event_tuple_t tuple;
	int i;

	vprint(EVENT_MID, "call_event_handler: %s\n", sp);

	/* reset string pointers */
	for (i = 0; i < NUM_EV_STR; i++) {
		log_event_str[i] = NULL;
	}

	/*
	 * Parse message tuples
	 */
	while (get_log_event_tuple(&tuple, &sp) == 0) {
		if (strcmp(tuple.attr, LOGEVENT_TYPE) == 0) {
			log_event_str[EV_TYPE] = tuple.val;

		} else if (strcmp(tuple.attr, LOGEVENT_CLASS) == 0) {
			log_event_str[EV_CLASS] = tuple.val;

		} else if (strcmp(tuple.attr, DEVFS_PATHNAME) == 0) {
			log_event_str[EV_PATH_NAME] = tuple.val;

		} else if (strcmp(tuple.attr, DEVFS_MINOR_NAME) == 0) {
			log_event_str[EV_MINOR_NAME] = tuple.val;

		}
	}

	if (strcmp(log_event_str[EV_CLASS], EC_DEVFS) != 0) {
		return;
	} else if (strcmp(log_event_str[EV_TYPE], ET_DEVFS_INSTANCE_MOD) == 0) {
		devfs_instance_mod();
		return;
	}

	/* provide protection against competing threads */
	while (sema_wait(&sema_minor) != 0);

	/*
	 * If this daemon wasn't the last devfsadm process to run,
	 * clear caches
	 */
	if (enter_dev_lock() != getpid()) {
		invalidate_enumerate_cache();
		rm_all_links_from_cache();
	}

	/*
	 * If modules were unloaded, reload them.  Also use module status
	 * as an indication that we should check to see if other binding
	 * files need to be reloaded.
	 */
	if (module_head == NULL) {
		load_modules();
		read_minor_perm_file();
		read_driver_aliases_file();
		read_devlinktab_file();
	}

	if (strcmp(log_event_str[EV_TYPE], ET_DEVFS_DEVI_ADD) == 0) {
		add_minor_pathname(log_event_str[EV_PATH_NAME], NULL);

	} else if (strcmp(log_event_str[EV_TYPE], ET_DEVFS_MINOR_CREATE) == 0) {
		add_minor_pathname(log_event_str[EV_PATH_NAME],
				    log_event_str[EV_MINOR_NAME]);

	} else if (strcmp(log_event_str[EV_TYPE], ET_DEVFS_MINOR_REMOVE) == 0) {
		hot_cleanup(log_event_str[EV_PATH_NAME],
				    log_event_str[EV_MINOR_NAME]);

	} else if (strcmp(log_event_str[EV_TYPE], ET_DEVFS_DEVI_REMOVE) == 0) {
		hot_cleanup(log_event_str[EV_PATH_NAME], NULL);

	} else {
		err_print(UNKNOWN_EVENT, log_event_str[EV_TYPE]);
	}

	exit_dev_lock();

	(void) sema_post(&sema_minor);

	deferred_call_minor_fini();
}

/*
 *  Kernel logs a message when a devinfo node is attached.  Try to create
 *  /dev and /devices for each minor node.  minorname can be NULL.
 */
void
add_minor_pathname(char *node_path, char *minorname)
{
	di_node_t hp_node;
	di_minor_t next_minor;
	defer_list_t defer_list;
	defer_minor_t *dmp, *odmp;
	char *fcn = "add_minor_pathname: ";

	vprint(CHATTY_MID, "%snode_path = %s minorname = %s\n",
				fcn, node_path, (minorname == NULL) ?
				"NULL" : minorname);

	if ((hp_node = di_init(node_path, DINFOPROP|DINFOMINOR)) ==
	    DI_NODE_NIL) {
		if (errno != ENXIO) {
			err_print(DI_INIT_FAILED, node_path, strerror(errno));
		}
		return;
	}

	next_minor = di_minor_next(hp_node, DI_MINOR_NIL);

	defer_list.head = defer_list.tail = NULL;
	/*
	 * if minorname is NULL, call check_minor_type() for all
	 * minor nodes on this devinfo node.
	 */
	while (next_minor != DI_MINOR_NIL) {
		if ((minorname == NULL) ||
		    (strcmp(minorname, di_minor_name(next_minor)) == 0)) {
			(void) check_minor_type(hp_node, next_minor,
			    &defer_list);
		}
		next_minor = di_minor_next(hp_node, next_minor);
	}

	/*
	 * Now do deferred link creation.
	 */
	vprint(CHATTY_MID, "%s processing deferred minors\n", fcn);

	assert((defer_list.head == NULL) ^ (defer_list.tail != NULL));
	for (dmp = defer_list.head; dmp != NULL; ) {
		(void) check_minor_type(dmp->node, dmp->minor, NULL);
		odmp = dmp;
		dmp = dmp->next;
		assert(dmp != NULL || odmp == defer_list.tail);
		free(odmp);
	}

	di_fini(hp_node);

	/*
	 * call di_fini here, so that minor_fini() can use root_node
	 * root_node could be non-NULL if dangling() was called by
	 * any fini.
	 */
	if (root_node != NULL) {
		di_fini(root_node);
		root_node = NULL;
	}
}

/*
 * Checks the minor type.  If it is an alias node, then lookup
 * the real node/minor first, then call minor_process() to
 * do the real work.
 */
static int
check_minor_type(di_node_t node, di_minor_t minor, void *arg)
{
	di_minor_t clone_minor;
	ddi_minor_type minor_type;
	di_node_t clone_node;
	char *minor_name;
	int clone_made = FALSE;
	defer_list_t *deferp = (defer_list_t *)arg;

	/*
	 * We match driver names here instead of in minor_process
	 * as we want the actual driver name. This check is
	 * unnecessary during deferred processing.
	 */
	if (deferp != NULL && single_drv == TRUE) {
		char *dn = di_driver_name(node);
		if ((dn == NULL) || (strcmp(driver, dn) != 0))
			return (DI_WALK_CONTINUE);
	}

	minor_type = di_minor_type(minor);

	if (minor_type == DDM_MINOR) {
		minor_process(node, minor, deferp);

	} else if (minor_type == DDM_ALIAS) {
		/*
		 * This particular clone_node snapshot will not exist
		 * during deferred processing. So store alias node rather than
		 * real node/minor in the defer list.
		 */
		defer_list_t clone_defer, *cldp;
		defer_minor_t *dmp, *odmp;

		minor_name = di_minor_name(minor);
		if (DI_NODE_NIL == (clone_node =
				    di_init("/pseudo/clone@0", DINFOMINOR))) {
			err_print(DI_INIT_FAILED, "clone", strerror(errno));
			return (DI_WALK_CONTINUE);
		}

		cldp = NULL;
		if (deferp != NULL) {
			cldp = &clone_defer;
			cldp->head = cldp->tail = NULL;
		} else {
			vprint(CHATTY_MID, "check_minor_type: processing"
			    " deferred clone minor: %s\n",
			    minor_name == NULL ? "(null)" : minor_name);
		}

		clone_minor = di_minor_next(clone_node, DI_MINOR_NIL);
		while (clone_minor != DI_MINOR_NIL) {
			if (strcmp(di_minor_name(clone_minor),
				    minor_name) == 0) {

				minor_process(clone_node,
						    clone_minor, cldp);
				clone_made = TRUE;
				break;
			}
			clone_minor = di_minor_next(clone_node, clone_minor);
		}
		if (clone_made == FALSE) {
			err_print(CLONE_NOT_FOUND, minor_name);
		}

		/*
		 * cache alias minor node
		 */
		if (cldp != NULL && cldp->head != NULL) {
			assert(cldp->tail != NULL);
			cache_deferred_minor(deferp, node, minor);
			for (dmp = cldp->head; dmp != NULL; ) {
				odmp = dmp;
				dmp = dmp->next;
				assert(dmp != NULL || odmp == cldp->tail);
				free(odmp);
			}
		}
		di_fini(clone_node);
	}

	return (DI_WALK_CONTINUE);
}


/*
 *  This is the entry point for each minor node, whether walking
 *  the entire tree via di_walk_minor() or processing a hotplug event
 *  for a single devinfo node (via hotplug ndi_devi_online()).
 */
/*ARGSUSED*/
static void
minor_process(di_node_t node, di_minor_t minor, defer_list_t *deferp)
{
	create_list_t *create;
	int defer_minor;

	vprint(CHATTY_MID, "minor_process: node=%s, minor=%s\n",
		di_node_name(node), di_minor_name(minor));

	if (deferp != NULL) {

		/* create /devices node */
		if (build_devices == TRUE) {
			create_devices_node(node, minor);
		}

		if (build_dev == FALSE) {
			return;
		}

		/*
		 * This function will create any nodes for /etc/devlink.tab.
		 * If devlink.tab handles link creation, we don't call any
		 * devfsadm modules since that could cause duplicate caching
		 * in the enumerate functions if different re strings are
		 * passed that are logically identical.  I'm still not
		 * convinced this would cause any harm, but better to be safe.
		 *
		 * Deferred processing is available only for devlinks
		 * created through devfsadm modules.
		 */
		if (process_devlink_compat(minor, node) == TRUE) {
			return;
		}
	} else {
		vprint(CHATTY_MID, "minor_process: deferred processing\n");
	}

	/*
	 * look for relevant link create rules in the modules, and
	 * invoke the link create callback function to build a link
	 * if there is a match.
	 */
	defer_minor = 0;
	for (create = create_head; create != NULL; create = create->next) {
		if ((minor_matches_rule(node, minor, create) == TRUE) &&
		    class_ok(create->create->device_class) ==
		    DEVFSADM_SUCCESS) {
			if (call_minor_init(create->modptr) ==
			    DEVFSADM_FAILURE) {
				continue;
			}

			/*
			 * If NOT doing the deferred creates (i.e. 1st pass) and
			 * rule requests deferred processing cache the minor
			 * data.
			 *
			 * If deferred processing (2nd pass), create links
			 * ONLY if rule requests deferred processing.
			 */
			if (deferp != NULL &&
			    ((create->create->flags & CREATE_MASK) ==
			    CREATE_DEFER)) {
				defer_minor = 1;
				continue;
			} else if (deferp == NULL &&
			    ((create->create->flags & CREATE_MASK) !=
			    CREATE_DEFER)) {
				continue;
			}

			if ((*(create->create->callback_fcn))
			    (minor, node) == DEVFSADM_TERMINATE) {
				break;
			}
		}
	}

	if (defer_minor == 1) {
		cache_deferred_minor(deferp, node, minor);
	}

}


/*
 * Cache node and minor in defer list.
 */
static void
cache_deferred_minor(
	defer_list_t *deferp,
	di_node_t node,
	di_minor_t minor)
{
	defer_minor_t *dmp;
	char *node_name = di_node_name(node);
	char *minor_name = di_minor_name(minor);
	const char *fcn = "cache_deferred_minor:";

	vprint(CHATTY_MID, "%s node=%s, minor=%s\n", fcn,
	    (node_name == NULL ? "(null)": node_name),
	    (minor_name == NULL ? "(null)": minor_name));

	if (deferp == NULL) {
		vprint(CHATTY_MID, "%s: cannot cache during "
		    "deferred processing. Ignoring minor\n", fcn);
		return;
	}

	dmp = (defer_minor_t *)s_zalloc(sizeof (defer_minor_t));
	dmp->node = node;
	dmp->minor = minor;
	dmp->next = NULL;

	assert(deferp->head == NULL || deferp->tail != NULL);
	if (deferp->head == NULL) {
		deferp->head = dmp;
	} else {
		deferp->tail->next = dmp;
	}
	deferp->tail = dmp;
}

/*
 *  Check to see if "create" link creation rule matches this node/minor.
 *  If it does, return TRUE.
 */
static int
minor_matches_rule(di_node_t node, di_minor_t minor, create_list_t *create)
{
	char *m_nodetype, *m_drvname;

	if (create->create->node_type != NULL) {
		/* some pseudo drivers don't specify nodetype */

		m_nodetype = di_minor_nodetype(minor);
		if (m_nodetype == NULL) {
			m_nodetype = "ddi_pseudo";
		}

		switch (create->create->flags & TYPE_MASK) {
		case TYPE_EXACT:
			if (strcmp(create->create->node_type, m_nodetype) !=
			    0) {
				return (FALSE);
			}
			break;
		case TYPE_PARTIAL:
			if (strncmp(create->create->node_type, m_nodetype,
			    strlen(create->create->node_type)) != 0) {
				return (FALSE);
			}
			break;
		case TYPE_RE:
			if (regexec(&(create->node_type_comp), m_nodetype,
			    0, NULL, 0) != 0) {
				return (FALSE);
			}
			break;
		}
	}

	if (create->create->drv_name != NULL) {
		m_drvname = di_driver_name(node);
		switch (create->create->flags & DRV_MASK) {
		case DRV_EXACT:
			if (strcmp(create->create->drv_name, m_drvname) != 0) {
				return (FALSE);
			}
			break;
		case DRV_RE:
			if (regexec(&(create->drv_name_comp), m_drvname,
			    0, NULL, 0) != 0) {
				return (FALSE);
			}
			break;
		}
	}

	return (TRUE);
}

/*
 * If no classes were given on the command line, then return DEVFSADM_SUCCESS.
 * Otherwise, return DEVFSADM_SUCCESS if the device "class" from the module
 * matches one of the device classes given on the command line,
 * otherwise, return DEVFSADM_FAILURE.
 */
static int
class_ok(char *class)
{
	int i;

	if (num_classes == 0) {
		return (DEVFSADM_SUCCESS);
	}

	for (i = 0; i < num_classes; i++) {
		if (strcmp(class, classes[i]) == 0) {
			return (DEVFSADM_SUCCESS);
		}
	}
	return (DEVFSADM_FAILURE);
}

/*
 * call minor_fini on active modules, then unload ALL modules
 */
static void
unload_modules(void)
{
	module_t *module_free;
	create_list_t *create_free;
	remove_list_t *remove_free;

	/*
	 * If this daemon wasn't the last devfsadm process to run,
	 * clear caches
	 */
	if ((build_dev == TRUE) && (enter_dev_lock() != getpid())) {
		invalidate_enumerate_cache();
		rm_all_links_from_cache();
	}

	while (create_head != NULL) {
		create_free = create_head;
		create_head = create_head->next;

		if ((create_free->create->flags & TYPE_RE) == TYPE_RE) {
			regfree(&(create_free->node_type_comp));
		}
		if ((create_free->create->flags & DRV_RE) == DRV_RE) {
			regfree(&(create_free->drv_name_comp));
		}
		free(create_free);
	}

	while (remove_head != NULL) {
		remove_free = remove_head;
		remove_head = remove_head->next;
		free(remove_free);
	}

	while (module_head != NULL) {

		if ((module_head->minor_fini != NULL) &&
		    ((module_head->flags & MODULE_ACTIVE) == MODULE_ACTIVE)) {
			(void) (*(module_head->minor_fini))();
		}

		vprint(MODLOAD_MID, "unloading module %s\n", module_head->name);
		free(module_head->name);
		(void) dlclose(module_head->dlhandle);

		module_free = module_head;
		module_head = module_head->next;
		free(module_free);
	}

	exit_dev_lock();
}

/*
 * Load devfsadm logical link processing modules.
 */
static void
load_modules(void)
{
	DIR *mod_dir;
	struct dirent *entp = NULL;
	struct dirent *retp;
	char cdir[PATH_MAX + 1];
	char *last;
	char *mdir = module_dirs;
	char *fcn = "load_modules: ";

	while (*mdir != '\0') {

		while (*mdir == ':') {
			mdir++;
		}

		if (*mdir == '\0') {
			continue;
		}

		last = strchr(mdir, ':');

		if (last == NULL) {
			last = mdir + strlen(mdir);
		}

		(void) strncpy(cdir, mdir, last - mdir);
		cdir[last - mdir] = '\0';
		mdir += strlen(cdir);

		if ((mod_dir = opendir(cdir)) == NULL) {
			vprint(MODLOAD_MID, "%sopendir(%s): %s\n",
				fcn, cdir, strerror(errno));
			continue;
		}

		entp = s_malloc(PATH_MAX + 1 + sizeof (struct  dirent));

		while (readdir_r(mod_dir, entp, &retp) == 0) {

			if (retp == NULL) {
				break;
			}

			if ((strcmp(entp->d_name, ".") == 0) ||
			    (strcmp(entp->d_name, "..") == 0)) {
				continue;
			}

			load_module(entp->d_name, cdir);
		}
		s_closedir(mod_dir);
		free(entp);
	}
}

static void
load_module(char *mname, char *cdir)
{
	_devfsadm_create_reg_t *create_reg;
	_devfsadm_remove_reg_t *remove_reg;
	create_list_t *create_list_element;
	create_list_t **create_list_next;
	remove_list_t *remove_list_element;
	remove_list_t **remove_list_next;
	char epath[PATH_MAX + 1], *end;
	char *fcn = "load_module: ";
	void *dlhandle;
	module_t *module;
	int n;
	int i;

	/* ignore any file which does not end in '.so' */
	if ((end = strstr(mname, MODULE_SUFFIX)) != NULL) {
		if (end[strlen(MODULE_SUFFIX)] != '\0') {
			return;
		}
	} else {
		return;
	}

	(void) sprintf(epath, "%s/%s", cdir, mname);

	if ((dlhandle = dlopen(epath, RTLD_LAZY)) == NULL) {
		err_print(DLOPEN_FAILED, epath, dlerror());
		return;
	}

	/* dlsym the _devfsadm_create_reg structure */
	if (NULL == (create_reg = (_devfsadm_create_reg_t *)
		    dlsym(dlhandle, _DEVFSADM_CREATE_REG))) {
		vprint(MODLOAD_MID, "dlsym(%s, %s): symbol not found\n", epath,
			_DEVFSADM_CREATE_REG);
	} else {
		vprint(MODLOAD_MID, "%sdlsym(%s, %s) succeeded\n",
			    fcn, epath, _DEVFSADM_CREATE_REG);
	}

	/* dlsym the _devfsadm_remove_reg structure */
	if (NULL == (remove_reg = (_devfsadm_remove_reg_t *)
	    dlsym(dlhandle, _DEVFSADM_REMOVE_REG))) {
		vprint(MODLOAD_MID, "dlsym(%s,\n\t%s): symbol not found\n",
			epath, _DEVFSADM_REMOVE_REG);
	} else {
		vprint(MODLOAD_MID, "dlsym(%s, %s): succeeded\n",
			    epath, _DEVFSADM_REMOVE_REG);
	}

	vprint(MODLOAD_MID, "module %s loaded\n", epath);

	module = (module_t *)s_malloc(sizeof (module_t));
	module->name = s_strdup(epath);
	module->dlhandle = dlhandle;

	/* dlsym other module functions, to be called later */
	module->minor_fini = (int (*)())dlsym(dlhandle, MINOR_FINI);
	module->minor_init = (int (*)())dlsym(dlhandle, MINOR_INIT);
	module->flags = 0;

	/*
	 *  put a ptr to each struct devfsadm_create on "create_head"
	 *  list sorted in interpose_lvl.
	 */
	if (create_reg != NULL) {
		for (i = 0; i < create_reg->count; i++) {
			int flags = create_reg->tblp[i].flags;

			create_list_element = (create_list_t *)
				s_malloc(sizeof (create_list_t));

			create_list_element->create = &(create_reg->tblp[i]);
			create_list_element->modptr = module;

			if (((flags & CREATE_MASK) != 0) &&
			    ((flags & CREATE_MASK) != CREATE_DEFER)) {
				free(create_list_element);
				err_print("illegal flag combination in "
						" module create\n");
				err_print(IGNORING_ENTRY, i, epath);
				continue;
			}

			if (((flags & TYPE_MASK) == 0) ^
			    (create_reg->tblp[i].node_type == NULL)) {
				free(create_list_element);
				err_print("flags value incompatible with "
					"node_type value in module create\n");
				err_print(IGNORING_ENTRY, i, epath);
				continue;
			}

			if (((flags & TYPE_MASK) != 0) &&
			    ((flags & TYPE_MASK) != TYPE_EXACT) &&
			    ((flags & TYPE_MASK) != TYPE_RE) &&
			    ((flags & TYPE_MASK) != TYPE_PARTIAL)) {
				free(create_list_element);
				err_print("illegal TYPE_* flag combination in "
						" module create\n");
				err_print(IGNORING_ENTRY, i, epath);
				continue;
			}

			/* precompile regular expression for efficiency */
			if ((flags & TYPE_RE) == TYPE_RE) {
				if ((n = regcomp(&(create_list_element->
				    node_type_comp),
				    create_reg->tblp[i].node_type,
				    REG_EXTENDED)) != 0) {
					free(create_list_element);
					err_print(REGCOMP_FAILED,
						create_reg->tblp[i].node_type,
						n);
					err_print(IGNORING_ENTRY, i, epath);
					continue;
				}
			}

			if (((flags & DRV_MASK) == 0) ^
			    (create_reg->tblp[i].drv_name == NULL)) {
				if ((flags & TYPE_RE) == TYPE_RE) {
					regfree(&(create_list_element->
					    node_type_comp));
				}
				free(create_list_element);
				err_print("flags value incompatible with "
					"drv_name value in module create\n");
				err_print(IGNORING_ENTRY, i, epath);
				continue;
			}

			if (((flags & DRV_MASK) != 0) &&
			    ((flags & DRV_MASK) != DRV_EXACT) &&
			    ((flags & DRV_MASK) !=  DRV_RE)) {
				if ((flags & TYPE_RE) == TYPE_RE) {
					regfree(&(create_list_element->
					    node_type_comp));
				}
				free(create_list_element);
				err_print("illegal DRV_* flag combination in "
					"module create\n");
				err_print(IGNORING_ENTRY, i, epath);
				continue;
			}

			/* precompile regular expression for efficiency */
			if ((create_reg->tblp[i].flags & DRV_RE) == DRV_RE) {
				if ((n = regcomp(&(create_list_element->
				    drv_name_comp),
				    create_reg->tblp[i].drv_name,
				    REG_EXTENDED)) != 0) {
					if ((flags & TYPE_RE) == TYPE_RE) {
						regfree(&(create_list_element->
						    node_type_comp));
					}
					free(create_list_element);
					err_print(REGCOMP_FAILED,
						create_reg->tblp[i].drv_name,
						n);
					err_print(IGNORING_ENTRY, i, epath);
					continue;
				}
			}


			/* add to list sorted by interpose level */
			for (create_list_next = &(create_head);
				(*create_list_next != NULL) &&
				(*create_list_next)->create->interpose_lvl >=
				create_list_element->create->interpose_lvl;
				create_list_next =
					&((*create_list_next)->next));
			create_list_element->next = *create_list_next;
			*create_list_next = create_list_element;
		}
	}

	/*
	 *  put a ptr to each struct devfsadm_remove on "remove_head"
	 *  list sorted by interpose_lvl.
	 */
	if (remove_reg != NULL) {
		for (i = 0; i < remove_reg->count; i++) {

			remove_list_element = (remove_list_t *)
				s_malloc(sizeof (remove_list_t));

			remove_list_element->remove = &(remove_reg->tblp[i]);
			remove_list_element->modptr = module;

			for (remove_list_next = &(remove_head);
				(*remove_list_next != NULL) &&
				(*remove_list_next)->remove->interpose_lvl >=
				remove_list_element->remove->interpose_lvl;
				remove_list_next =
					&((*remove_list_next)->next));
			remove_list_element->next = *remove_list_next;
			*remove_list_next = remove_list_element;
		}
	}

	module->next = module_head;
	module_head = module;
}

/*
 * Create a thread to call minor_fini after some delay
 */
static void
deferred_call_minor_fini()
{
	vprint(INITFINI_MID, "deferred_call_minor_fini\n");

	(void) mutex_lock(&minor_fini_mutex);

	minor_fini_delay_restart = TRUE;

	if (minor_fini_thread_created == FALSE) {

		if (thr_create(NULL, NULL,
		    (void *(*)(void *))call_minor_fini_thread, NULL,
		    THR_DETACHED, NULL)) {
			err_print(CANT_CREATE_THREAD, "minor_fini",
					strerror(errno));

			(void) mutex_unlock(&minor_fini_mutex);

			/* provide protection against competing threads */
			while (sema_wait(&sema_minor) != 0);

			/*
			 * just call minor fini here rather than
			 * giving up
			 */
			unload_modules();

			(void) sema_post(&sema_minor);

			return;
		}
		minor_fini_thread_created = TRUE;
	} else {
		vprint(INITFINI_MID, "restarting delay\n");
	}

	(void) mutex_unlock(&minor_fini_mutex);
}

/*
 * after not receiving an event for minor_fini_timeout secs, we need
 * to call the minor_fini routines
 */
/*ARGSUSED*/
static void
call_minor_fini_thread(void *arg)
{
	int count = 0;

	(void) mutex_lock(&minor_fini_mutex);

	vprint(INITFINI_MID, "call_minor_fini_thread starting\n");

	do {
		minor_fini_delay_restart = FALSE;

		(void) mutex_unlock(&minor_fini_mutex);
		(void) sleep(minor_fini_timeout);
		(void) mutex_lock(&minor_fini_mutex);

		/*
		 * if minor_fini_delay_restart is still false then
		 * we can call minor fini routines.
		 * ensure that at least periodically minor_fini gets
		 * called satisfying link generators depending on fini
		 * being eventually called
		 */
		if ((count++ >= FORCE_CALL_MINOR_FINI) ||
		    (minor_fini_delay_restart == FALSE)) {
			vprint(INITFINI_MID,
			    "call_minor_fini starting (%d)\n", count);
			(void) mutex_unlock(&minor_fini_mutex);

			/* provide protection against competing threads */
			while (sema_wait(&sema_minor) != 0);

			unload_modules();

			(void) sema_post(&sema_minor);

			vprint(INITFINI_MID, "call_minor_fini done\n");

			/*
			 * hang around before exiting just in case
			 * minor_fini_delay_restart is set again
			 */
			(void) sleep(1);

			count = 0;

			(void) mutex_lock(&minor_fini_mutex);
		}
	} while (minor_fini_delay_restart);

	minor_fini_thread_created = FALSE;
	(void) mutex_unlock(&minor_fini_mutex);
	vprint(INITFINI_MID, "call_minor_fini_thread exiting\n");
}

/*
 * Attempt to initialize module, if a minor_init routine exists.  Set
 * the active flag if the routine exists and succeeds.	If it doesn't
 * exist, just set the active flag.
 */
static int
call_minor_init(module_t *module)
{
	char *fcn = "call_minor_init: ";

	if ((module->flags & MODULE_ACTIVE) == MODULE_ACTIVE) {
		return (DEVFSADM_SUCCESS);
	}

	vprint(INITFINI_MID, "%smodule %s.  current state: inactive\n",
		fcn, module->name);

	if (module->minor_init == NULL) {
		module->flags |= MODULE_ACTIVE;
		vprint(INITFINI_MID, "minor_init not defined\n");
		return (DEVFSADM_SUCCESS);
	}

	if ((*(module->minor_init))() == DEVFSADM_FAILURE) {
		err_print(FAILED_FOR_MODULE, MINOR_INIT, module->name);
		return (DEVFSADM_FAILURE);
	}

	vprint(INITFINI_MID, "minor_init() returns DEVFSADM_SUCCESS. "
		"new state: active\n");

	module->flags |= MODULE_ACTIVE;
	return (DEVFSADM_SUCCESS);
}

/*
 * Creates a symlink 'link' to the physical path of node:minor.
 * Construct link contents, then call create_link_common().
 */
/*ARGSUSED*/
int
devfsadm_mklink(char *link, di_node_t node, di_minor_t minor, int flags)
{
	char rcontents[PATH_MAX + 1];
	char devlink[PATH_MAX + 1];
	char phy_path[PATH_MAX + 1];
	char *acontents;
	int numslashes;
	int rv;
	int i;
	int last_was_slash = FALSE;

	/*
	 * try to use devices path that was created in
	 * create_devices_node()
	 */
	if ((node == lnode) && (minor == lminor)) {
		acontents = lphy_path;
	} else {
		(void) strcpy(phy_path, s_di_devfs_path(node, minor));
		(void) strcat(phy_path, ":");
		(void) strcat(phy_path, di_minor_name(minor));
		acontents = phy_path;
	}

	/* prepend link with dev_dir contents */
	(void) strcpy(devlink, dev_dir);
	(void) strcat(devlink, "/");
	(void) strcat(devlink, link);

	/*
	 * Calculate # of ../ to add.  Account for double '//' in path.
	 * Ignore all leading slashes.
	 */
	for (i = 0; link[i] == '/'; i++)
		;
	for (numslashes = 0; link[i] != '\0'; i++) {
		if (link[i] == '/') {
			if (last_was_slash == FALSE) {
				numslashes++;
				last_was_slash = TRUE;
			}
		} else {
			last_was_slash = FALSE;
		}
	}
	/* Don't count any trailing '/' */
	if (link[i-1] == '/') {
		numslashes--;
	}

	rcontents[0] = '\0';
	do {
		(void) strcat(rcontents, "../");
	} while (numslashes-- != 0);

	(void) strcat(rcontents, "devices");
	(void) strcat(rcontents, acontents);

	if (devlinks_debug == TRUE) {
		vprint(INFO_MID, "adding link %s ==> %s\n",
				devlink, rcontents);
	}

	if ((rv = create_link_common(devlink, rcontents)) == DEVFSADM_SUCCESS) {
		linknew = TRUE;
		add_link_to_cache(link, acontents);
	} else {
		linknew = FALSE;
	}

	return (rv);
}

/*
 * Creates a symlink link to primary_link.  Calculates relative
 * directory offsets, then calls link_common().
 */
/*ARGSUSED*/
int
devfsadm_secondary_link(char *link, char *primary_link, int flags)
{
	char contents[PATH_MAX + 1];
	char devlink[PATH_MAX + 1];
	int rv;
	char *fpath;
	char *tpath;
	char *op;

	/* prepend link with dev_dir contents */
	(void) strcpy(devlink, dev_dir);
	(void) strcat(devlink, "/");
	(void) strcat(devlink, link);
	/*
	 * building extra link, so use first link as link contents, but first
	 * make it relative.
	 */
	fpath = link;
	tpath = primary_link;
	op = contents;

	while (*fpath == *tpath && *fpath != '\0') {
		fpath++, tpath++;
	}

	/* Count directories to go up, if any, and add "../" */
	while (*fpath != '\0') {
		if (*fpath == '/') {
			(void) strcpy(op, "../");
			op += 3;
		}
		fpath++;
	}

	/*
	 * Back up to the start of the current path component, in
	 * case in the middle
	 */
	while (tpath != primary_link && *(tpath-1) != '/') {
		tpath--;
	}
	(void) strcpy(op, tpath);

	if (devlinks_debug == TRUE) {
		vprint(INFO_MID, "adding extra link %s ==> %s\n",
				devlink, contents);
	}

	if ((rv = create_link_common(devlink, contents)) == DEVFSADM_SUCCESS) {
		/*
		 * we need to save the ultimate /devices contents, and not the
		 * secondary link, since hotcleanup only looks at /devices path.
		 * Since we don't have devices path here, we can try to get it
		 * by readlink'ing the secondary link.  This assumes the primary
		 * link was created first.  Somewhat safer would be to just get
		 * it from create_devices_node()... but  a tiny but unsafe also.
		 */
		add_link_to_cache(link, lphy_path);
		linknew = TRUE;
	} else {
		linknew = FALSE;
	}

	return (rv);
}

/*
 * Does the actual link creation.  VERBOSE_MID only used if there is
 * a change.  CHATTY_MID used otherwise.
 */
static int
create_link_common(char *devlink, char *contents)
{
	int try;
	int linksize;
	int max_tries = 0;
	static int prev_link_existed = TRUE;
	char checkcontents[PATH_MAX + 1];
	char *hide;

	if (file_mods == FALSE) {
		linksize = readlink(devlink, checkcontents, PATH_MAX);
		if (linksize > 0) {
			checkcontents[linksize] = '\0';
			if (strcmp(checkcontents, contents) != 0) {
				vprint(CHATTY_MID, REMOVING_LINK,
						devlink, checkcontents);
				return (DEVFSADM_SUCCESS);
			} else {
				vprint(CHATTY_MID, "link exists and is correct:"
					" %s -> %s\n", devlink, contents);
				/* failure only in that the link existed */
				return (DEVFSADM_FAILURE);
			}
		} else {
			vprint(VERBOSE_MID, CREATING_LINK, devlink, contents);
			return (DEVFSADM_SUCCESS);
		}
	}

	/*
	 * systems calls are expensive, so predict whether to readlink
	 * or symlink first, based on previous attempt
	 */
	if (prev_link_existed == FALSE) {
		try = CREATE_LINK;
	} else {
		try = READ_LINK;
	}

	while (++max_tries <= 3) {

		switch (try) {
		case  CREATE_LINK:

			if (symlink(contents, devlink) == 0) {
				vprint(VERBOSE_MID, CREATING_LINK, devlink,
						contents);
				prev_link_existed = FALSE;
				/* link successfully created */
				return (DEVFSADM_SUCCESS);
			} else {
				switch (errno) {

				case ENOENT:
					/* dirpath to node doesn't exist */
					hide = strrchr(devlink, '/');
					*hide = '\0';
					s_mkdirp(devlink, S_IRWXU|S_IRGRP|
						S_IXGRP|S_IROTH|S_IXOTH);
					*hide = '/';
					break;
				case EEXIST:
					try = READ_LINK;
					break;
				default:
					err_print(SYMLINK_FAILED, devlink,
						contents, strerror(errno));
					return (DEVFSADM_FAILURE);
				}
			}
			break;

		case READ_LINK:

			linksize = readlink(devlink, checkcontents, PATH_MAX);
			if (linksize >= 0) {
				checkcontents[linksize] = '\0';
				if (strcmp(checkcontents, contents) != 0) {
					s_unlink(devlink);
					vprint(VERBOSE_MID, REMOVING_LINK,
						devlink, checkcontents);
					try = CREATE_LINK;
				} else {
					prev_link_existed = TRUE;
					vprint(CHATTY_MID,
						"link exists and is corrrect:"
						" %s -> %s\n", devlink,
						contents);
					/* failure in that the link existed */
					return (DEVFSADM_FAILURE);
				}
			} else {
				switch (errno) {
				case EINVAL:
					/* not a symlink, remove and create */
					s_unlink(devlink);
				default:
					/* maybe it didn't exist at all */
					try = CREATE_LINK;
					break;
				}
			}
			break;
		}
	}
	err_print(MAX_ATTEMPTS, devlink, contents);
	return (DEVFSADM_FAILURE);
}

/*
 * Create a /devices node if it doesn't exist, with appropriate
 * permissions and ownership by calling getattr().  If the node already
 * exists and is a device special file, do nothing.
 */
static void
create_devices_node(di_node_t node, di_minor_t minor)
{
	static int prev_node_existed = TRUE;
	int max_tries = 0;
	int try;
	int spectype;
	char phy_path[PATH_MAX + 1];
	char *slash;
	mode_t mode;
	dev_t dev;
	uid_t uid;
	gid_t gid;
	gid_t saved_gid;
	struct stat sb;

	/* used by disk_link.c for rm_stale_link() optimization */
	physpathnew = FALSE;

	/* lphy_path starts with / */
	(void) strcpy(lphy_path, s_di_devfs_path(node, minor));
	(void) strcat(lphy_path, ":");
	(void) strcat(lphy_path, di_minor_name(minor));

	(void) strcpy(phy_path, devices_dir);
	(void) strcat(phy_path, lphy_path);

	lnode = node;
	lminor = minor;

	vprint(CHATTY_MID, "create_devices_node: phy_path=%s lphy_path=%s\n",
			phy_path, lphy_path);

	dev = di_minor_devt(minor);
	spectype = di_minor_spectype(minor); /* block or char */

	getattr(phy_path, spectype, dev, &mode, &uid, &gid);

	if (file_mods == FALSE) {
		/*
		 * DON'T do the actual work of checking or creating the node.
		 * This just prints out what we'd do if we were actually
		 * modifying the file system.
		 */
		if (stat(phy_path, &sb) == -1) {
			vprint(VERBOSE_MID, MKNOD_MSG, phy_path,
					uid, gid, mode);
		} else {
			/*
			 * node already exists.  As long as it is a device
			 * special file, return, otherwise, recreate node.
			 */
			if (sb.st_rdev == dev) {
				vprint(CHATTY_MID, "minor node exists and "
					"correct: %s\n", phy_path);
				return;
			}
			vprint(VERBOSE_MID, MKNOD_MSG, phy_path, uid,
				gid, mode);

		}
		return;
	}

	/*
	 * systems calls are expensive, so predict whether
	 * to readlink or symlink first, based on previous attempt
	 */
	if (prev_node_existed == FALSE) {
		try = CREATE_NODE;
	} else {
		try = READ_NODE;
	}

	while (++max_tries <= 3) {

		switch (try) {
		case  CREATE_NODE:

			vprint(CHATTY_MID, "create_node: phy_path=%s "
				"spectype=%d dev=%d last=%d\n", phy_path,
				spectype, dev, prev_node_existed);

			if (mknod(phy_path, mode, dev) == 0) {
				prev_node_existed = FALSE;
				physpathnew = TRUE;
				if (chown(phy_path, uid, gid) == -1) {
					err_print(CHOWN_FAILED, phy_path,
							strerror(errno));
				}
				vprint(VERBOSE_MID, MKNOD_MSG, phy_path,
						uid, gid, mode);
				return;
			} else {
				switch (errno) {
				case ENOENT:
					prev_node_existed = FALSE;
					/* dir path up to node doesn't exist. */
					slash = strrchr(phy_path, '/');
					*slash = '\0';

					/*
					 * drvconfig sets gid for duration of
					 * program.  Can't do that since we want
					 * different perms for links and dev
					 * directories.  This maintains exact
					 * compatability with drvconfig.
					 */
					saved_gid = getegid();
					(void) setegid(sys_gid);
					s_mkdirp(phy_path, S_IRWXU|S_IRGRP|
						S_IXGRP|S_IROTH|S_IXOTH);
					(void) setegid(saved_gid);
					*slash = '/';
					break;
				case EEXIST:
					try = READ_NODE;
					break;
				default:
					return;
				}
			}
			break;
		case READ_NODE:

			vprint(CHATTY_MID, "read_node: phy_path=%s spectype=%d"
				" dev=%d last=%d\n", phy_path, spectype, dev,
				prev_node_existed);

			if (stat(phy_path, &sb) == -1) {
				try = CREATE_NODE;
			} else {
				/*
				 * node already exists.  As long as it is a
				 * device special file, return, otherwise,
				 * recreate node.
				 */
				if (sb.st_rdev == dev) {
					prev_node_existed = TRUE;
					vprint(CHATTY_MID, "minor node exists "
						"and correct: %s\n", phy_path);
					return;
				}
				vprint(VERBOSE_MID, RM_INVALID_MINOR_NODE,
						phy_path);
				s_unlink(phy_path);
				try = CREATE_NODE;
			}
			break;
		}
	}
	err_print(MAX_ATTEMPTS_MKNOD, phy_path);
}

/*
 * Removes logical link and the minor node it refers to.  If file is a
 * link, we recurse and try to remove the minor node (or link if path is
 * a double link) that file's link contents refer to.
 */
static void
devfsadm_rm_work(char *file, int recurse, int file_type)
{
	char *fcn = "devfsadm_rm_work: ";
	int linksize;
	char contents[PATH_MAX + 1];
	char nextfile[PATH_MAX + 1];
	char newfile[PATH_MAX + 1];
	char *ptr;

	vprint(REMOVE_MID, "%s%s\n", fcn, file);

	/* TYPE_LINK split into multiple if's due to excessive indentations */
	if (file_type == TYPE_LINK) {
		(void) strcpy(newfile, dev_dir);
		(void) strcat(newfile, "/");
		(void) strcat(newfile, file);
	}

	if ((file_type == TYPE_LINK) && (recurse == TRUE) &&
	    ((linksize = readlink(newfile, contents, PATH_MAX)) > 0)) {
		contents[linksize] = '\0';

		if (is_minor_node(contents, &ptr) == DEVFSADM_TRUE) {
			devfsadm_rm_work(++ptr, FALSE, TYPE_DEVICES);
		} else {
			if (strncmp(contents, DEV "/", strlen(DEV) + 1) == 0) {
				devfsadm_rm_work(&contents[strlen(DEV) + 1],
							TRUE, TYPE_LINK);
			} else {
				if ((ptr = strrchr(file, '/')) != NULL) {
					*ptr = '\0';
					(void) strcpy(nextfile, file);
					*ptr = '/';
					(void) strcat(nextfile, "/");
				} else {
					(void) strcpy(nextfile, "");
				}
				(void) strcat(nextfile, contents);
				devfsadm_rm_work(nextfile, TRUE, TYPE_LINK);
			}
		}
	}

	if (file_type == TYPE_LINK) {
		vprint(VERBOSE_MID, DEVFSADM_UNLINK, newfile);
		if (file_mods == TRUE) {
			rm_link_from_cache(file);
			s_unlink(newfile);
			rm_parent_dir_if_empty(newfile);
			invalidate_enumerate_cache();
		}
	}

	if (file_type == TYPE_DEVICES) {
		(void) strcpy(newfile, devices_dir);
		(void) strcat(newfile, "/");
		(void) strcat(newfile, file);
		vprint(VERBOSE_MID, DEVFSADM_UNLINK, newfile);
		if (file_mods == TRUE) {
			s_unlink(newfile);
			rm_parent_dir_if_empty(newfile);
		}
	}
}

void
devfsadm_rm_link(char *file)
{
	devfsadm_rm_work(file, FALSE, TYPE_LINK);
}

void
devfsadm_rm_all(char *file)
{
	devfsadm_rm_work(file, TRUE, TYPE_LINK);
}

/*
 * Try to remove any empty directories up the tree.  It is assumed that
 * pathname is a file that was removed, so start with its parent, and
 * work up the tree.
 */
static void
rm_parent_dir_if_empty(char *pathname)
{
	char *ptr, path[PATH_MAX + 1];
	char *fcn = "rm_parent_dir_if_empty: ";
	struct dirent *entp;
	struct dirent *retp;
	DIR *dp;

	vprint(REMOVE_MID, "%schecking %s if empty\n", fcn, pathname);

	(void) strcpy(path, pathname);

	entp = (struct dirent *)s_malloc(PATH_MAX + 1 +
					    sizeof (struct dirent));

	/*
	 * ascend up the dir tree, deleting all empty dirs.
	 * Return immediately if a dir is not empty.
	 */
	for (;;) {

		if ((ptr = strrchr(path, '/')) == NULL) {
			return;
		}

		*ptr = '\0';

		if ((dp = opendir(path)) == NULL) {
			err_print(OPENDIR_FAILED, path, strerror(errno));
			free(entp);
			return;
		}

		while (readdir_r(dp, entp, &retp) == 0) {

			if (retp == NULL) {
				vprint(REMOVE_MID, "%sremoving empty dir %s\n",
						fcn, path);
				(void) rmdir(path);
				break;
			}

			if (strcmp(entp->d_name, ".") == 0 ||
			    strcmp(entp->d_name, "..") == 0) {
				continue;
			}

			/* some other file is here, so return */
			vprint(REMOVE_MID, "%sdir not empty: %s\n", fcn, path);
			free(entp);
			s_closedir(dp);
			return;

		}
		s_closedir(dp);
	}
}

/*
 * This function and all the functions it calls below were added to
 * handle the unique problem with world wide names (WWN).  The problem is
 * that if a WWN device is moved to another address on the same controller
 * its logical link will change, while the physical node remains the same.
 * The result is that two logical links will point to the same physical path
 * in /devices, the valid link and a stale link. This function will
 * find all the stale nodes, though at a significant performance cost.

 * Caching is used to increase performance.
 * A cache will be built from disk if the cache tag doesn't already exist.
 * The cache tag is a regular expression "dir_re", which selects a
 * subset of disks to search from typically something like
 * "dev/cXt[0-9]+d[0-9]+s[0-9]+".  After the cache is built, consistency must
 * be maintained, so entries are added as new links are created, and removed
 * as old links are deleted.  The whole cache is flushed if we are a daemon,
 * and another devfsadm process ran in between.
 *
 * Once the cache is built, this function finds the cache which matches
 * dir_re, and then it searches all links in that cache looking for
 * any link whose contents match "valid_link_contents" with a corresponding link
 * which does not match "valid_link".  Any such matches are stale and removed.
 */
void
devfsadm_rm_stale_links(char *dir_re, char *valid_link, di_node_t node,
			di_minor_t minor)
{
	link_t *link, *next;
	char phy_path[PATH_MAX + 1];
	char *valid_link_contents;

	/*
	 * try to use devices path that was created in create_devices_node()
	 */
	if ((node == lnode) && (minor == lminor)) {
		valid_link_contents = lphy_path;
	} else {
		(void) strcpy(phy_path, s_di_devfs_path(node, minor));
		(void) strcat(phy_path, ":");
		(void) strcat(phy_path, di_minor_name(minor));
		valid_link_contents = phy_path;
	}

	/*
	 * As an optimization, we don't need to check for stale links if the
	 * physical path was just created, as that should imply there are no
	 * old links to search for.  Another optimization checks to make sure
	 * the corresponding devlink was just created before continuing.
	 */

	if ((physpathnew == TRUE) || (linknew == FALSE)) {
		return;
	}

	link = get_cached_links(dir_re);

	for (; link != NULL; link = next) {
		next = link->next;
		if ((strcmp(link->contents, valid_link_contents) == 0) &&
		    (strcmp(link->devlink, valid_link) != 0)) {
			vprint(CHATTY_MID, "removing %s -> %s\n"
				"valid link is: %s -> %s\n",
				link->devlink, link->contents,
				valid_link, valid_link_contents);
			devfsadm_rm_link(link->devlink);
		}
	}
}

/*
 * Return previously created cache, or create cache.
 */
static link_t *
get_cached_links(char *dir_re)
{
	recurse_dev_t rd;
	linkhead_t *linkhead;
	int n;

	vprint(BUILDCACHE_MID, "get_cached_links: %s\n", dir_re);

	for (linkhead = headlinkhead; linkhead != NULL;
		linkhead = linkhead->next) {
		if (strcmp(linkhead->dir_re, dir_re) == 0) {
			return (linkhead->link);
		}
	}

	/*
	 * This tag is not in cache, so add it, along with all its
	 * matching /dev entries.  This is the only time we go to disk.
	 */
	linkhead = s_malloc(sizeof (linkhead_t));
	linkhead->next = headlinkhead;
	headlinkhead = linkhead;
	linkhead->dir_re = s_strdup(dir_re);

	if ((n = regcomp(&(linkhead->dir_re_compiled), dir_re,
				REG_EXTENDED)) != 0) {
		err_print(REGCOMP_FAILED,  dir_re, n);
	}

	linkhead->link = NULL;

	rd.fcn = build_devlink_list;
	rd.data = (void *)linkhead;

	vprint(BUILDCACHE_MID, "get_cached_links: calling recurse_dev_re\n");

	/* call build_devlink_list for each directory in the dir_re RE */
	if (dir_re[0] == '/') {
		recurse_dev_re("/", &dir_re[1], &rd);
	} else {
		recurse_dev_re(dev_dir, dir_re, &rd);
	}

	return (linkhead->link);
}

static void
build_devlink_list(char *devlink, void *data)
{
	char *fcn = "build_devlink_list: ";
	char *ptr;
	char *r_contents;
	char *r_devlink;
	char contents[PATH_MAX + 1];
	char newlink[PATH_MAX + 1];
	char stage_link[PATH_MAX + 1];
	int linksize;
	linkhead_t *linkhead = (linkhead_t *)data;
	link_t *link;
	int i = 0;

	vprint(BUILDCACHE_MID, "%scheck_link: %s\n", fcn, devlink);

	(void) strcpy(newlink, devlink);

	do {
		linksize = readlink(newlink, contents, PATH_MAX);
		if (linksize <= 0) {
			/*
			 * The first pass through the do loop we may readlink()
			 * non-symlink files(EINVAL) from false regexec matches.
			 * Suppress error messages in those cases or if the link
			 * content is the empty string.
			 */
			if (linksize < 0 && (i || errno != EINVAL))
				err_print(READLINK_FAILED, newlink,
				    strerror(errno));
			return;
		}
		contents[linksize] = '\0';
		i = 1;

		if (is_minor_node(contents, &r_contents) == DEVFSADM_FALSE) {
			/*
			 * assume that link contents is really a pointer to
			 * another link, so recurse and read its link contents.
			 *
			 * some link contents are absolute:
			 *	/dev/audio -> /dev/sound/0
			 */
			if (strncmp(contents, DEV "/",
				strlen(DEV) + strlen("/")) != 0) {

				if ((ptr = strrchr(newlink, '/')) == NULL) {
					vprint(REMOVE_MID, "%s%s -> %s invalid "
						" link. missing '/'\n", fcn,
						newlink, contents);
						return;
				}
				*ptr = '\0';
				(void) strcpy(stage_link, newlink);
				*ptr = '/';
				(void) strcat(stage_link, "/");
				(void) strcat(stage_link, contents);
				(void) strcpy(newlink, stage_link);
			} else {
				(void) strcpy(newlink, dev_dir);
				(void) strcat(newlink, "/");
				(void) strcat(newlink,
					&contents[strlen(DEV) + strlen("/")]);
			}

		} else {
			newlink[0] = '\0';
		}
	} while (newlink[0] != '\0');

	if ((r_devlink = strstr(devlink, DEV "/")) == NULL) {
		return;
	}

	link = s_malloc(sizeof (link_t));

	/* don't store the '/' after /dev */
	r_devlink += DEV_LEN + 1;
	link->devlink = s_strdup(r_devlink);

	link->contents = s_strdup(r_contents);

	link->next = linkhead->link;
	linkhead->link = link;
}

/*
 * to be consistent, devlink must not begin with / and must be
 * relative to /dev/, whereas physpath must contain / and be
 * relative to /devices.
 */
static void
add_link_to_cache(char *devlink, char *physpath)
{
	linkhead_t *linkhead;
	link_t *link;
	int added = 0;

	if (file_mods == FALSE) {
		return;
	}

	vprint(CACHE_MID, "add_link_to_cache: %s -> %s ",
				devlink, physpath);

	for (linkhead = headlinkhead; linkhead != NULL;
		linkhead = linkhead->next) {
		if (regexec(&(linkhead->dir_re_compiled), devlink, 0, NULL,
			0) == 0) {
			added++;
			link = s_malloc(sizeof (link_t));
			link->devlink = s_strdup(devlink);
			link->contents = s_strdup(physpath);
			link->next = linkhead->link;
			linkhead->link = link;
		}
	}

	vprint(CACHE_MID,
		" %d %s\n", added, added == 0 ? "NOT ADDED" : "ADDED");
}

/*
 * Remove devlink from cache.  Devlink must be relative to /dev/ and not start
 * with /.
 */
static void
rm_link_from_cache(char *devlink)
{
	linkhead_t *linkhead;
	link_t **linkp;
	link_t *save;

	vprint(CACHE_MID, "rm_link_from_cache enter: %s\n", devlink);

	for (linkhead = headlinkhead; linkhead != NULL;
	    linkhead = linkhead->next) {
		if (regexec(&(linkhead->dir_re_compiled), devlink, 0, NULL,
			0) == 0) {

			for (linkp = &(linkhead->link); *linkp != NULL; ) {
				if ((strcmp((*linkp)->devlink, devlink) == 0)) {
					save = *linkp;
					*linkp = (*linkp)->next;
					free(save->devlink);
					free(save->contents);
					free(save);
					vprint(CACHE_MID, " %s FREED FROM "
						"CACHE\n", devlink);
				} else {
					linkp = &((*linkp)->next);
				}
			}
		}
	}
}

static void
rm_all_links_from_cache()
{
	linkhead_t *linkhead;
	linkhead_t *nextlinkhead;
	link_t *link;
	link_t *nextlink;

	vprint(CACHE_MID, "rm_all_links_from_cache\n");

	for (linkhead = headlinkhead; linkhead != NULL;
		linkhead = nextlinkhead) {

		nextlinkhead = linkhead->next;
		for (link = linkhead->link; link != NULL; link = nextlink) {
			nextlink = link->next;
			free(link->devlink);
			free(link->contents);
			free(link);
		}
		regfree(&(linkhead->dir_re_compiled));
		free(linkhead->dir_re);
		free(linkhead);
	}
	headlinkhead = NULL;
}

/*
 * Called when the kernel has modified the incore path_to_inst data.  This
 * function will schedule a flush of the data to the filesystem.
 */
static void
devfs_instance_mod(void)
{
	char *fcn = "devfs_instance_mod: ";
	vprint(PATH2INST_MID, "%senter\n", fcn);

	/* signal instance thread */
	(void) mutex_lock(&count_lock);
	inst_count++;
	(void) cond_signal(&cv);
	(void) mutex_unlock(&count_lock);
}

static void
instance_flush_thread(void)
{
	int i;
	int idle;

	for (;;) {

		(void) mutex_lock(&count_lock);
		while (inst_count == 0) {
			(void) cond_wait(&cv, &count_lock);
		}
		inst_count = 0;

		vprint(PATH2INST_MID, "signaled to flush path_to_inst."
			" Enter delay loop\n");
		/*
		 * Wait MAX_IDLE_DELAY seconds after getting the last flush
		 * path_to_inst event before invoking a flush, but never wait
		 * more than MAX_DELAY seconds after getting the first event.
		 */
		for (idle = 0, i = 0; i < MAX_DELAY; i++) {

			(void) mutex_unlock(&count_lock);
			(void) sleep(1);
			(void) mutex_lock(&count_lock);

			/* shorten the delay if we are idle */
			if (inst_count == 0) {
				idle++;
				if (idle > MAX_IDLE_DELAY) {
					break;
				}
			} else {
				inst_count = idle = 0;
			}
		}

		(void) mutex_unlock(&count_lock);

		flush_path_to_inst();
	}
}

/*
 * Call the loadable syscall.  This probably errs on the
 * side of being over-robust ..
 */
static int
do_syscall(char *filename)
{
	register void (*sigsaved)(int);
	register int err;
	char *fcn = "do_syscall: ";

	sigsaved = sigset(SIGSYS, SIG_IGN);
	vprint(INSTSYNC_MID, "%sabout to flush %s\n", fcn, filename);
	if (inst_sync(filename, 0) == -1) {
		err = errno;
	} else {
		err = 0;
	}
	(void) sigset(SIGSYS, sigsaved);

	switch (err) {
	case ENOSYS:
		vprint(INFO_MID, CANT_LOAD_SYSCALL);
		break;
		/*NOTREACHED*/
	case EPERM:
		vprint(INFO_MID, SUPER_TO_SYNC);
		break;
	default:
		vprint(INFO_MID, INSTSYNC_FAILED, filename, strerror(err));
		break;
	case 0:
		/*
		 * Success!
		 */
		return (DEVFSADM_SUCCESS);
	}

	return (DEVFSADM_FAILURE);
}

/*
 * calls inst_sync system call which flushes path_to_inst to a temp
 * file.  Then we rename that file to /etc/path_to_inst if successful
 */
static void
flush_path_to_inst(void)
{
	char *new_inst_file = NULL;
	char *old_inst_file = NULL;
	char *old_inst_file_npid = NULL;
	FILE *inst_file_fp = NULL;
	FILE *old_inst_file_fp = NULL;
	struct stat sb;
	int err;
	int c;

	vprint(PATH2INST_MID, "flush_path_to_inst: %s\n",
		(flush_path_to_inst_enable == TRUE) ? "ENABLED" :
						"DISABLED");

	if (flush_path_to_inst_enable == FALSE) {
		return;
	}

	new_inst_file = s_malloc(strlen(inst_file) + 8);
	old_inst_file = s_malloc(strlen(inst_file) + 11);
	old_inst_file_npid = s_malloc(strlen(inst_file) + 1 + 4);

	(void) sprintf(new_inst_file, "%s.%ld", inst_file, getpid());

	if (stat(new_inst_file, &sb) == 0) {
		s_unlink(new_inst_file);
	}

	if ((err = do_syscall(new_inst_file)) != DEVFSADM_SUCCESS) {
		goto out;
		/*NOTREACHED*/
	}

	/*
	 * Now we deal with the somewhat tricky updating and renaming
	 * of this critical piece of kernel state.
	 */

	/*
	 * Create a temporary file to contain a backup copy
	 * of 'inst_file'.  Of course if 'inst_file' doesn't exist,
	 * there's much less for us to do .. tee hee.
	 */
	if ((inst_file_fp = fopen(inst_file, "r")) == NULL) {
		/*
		 * No such file.  Rename the new onto the old
		 */
		if ((err = rename(new_inst_file, inst_file)) != 0)
			err_print(RENAME_FAILED, inst_file, strerror(errno));
		goto out;
		/*NOTREACHED*/
	}

	(void) sprintf(old_inst_file, "%s.old.%ld", inst_file, getpid());

	if (stat(old_inst_file, &sb) == 0) {
		s_unlink(old_inst_file);
	}

	if ((old_inst_file_fp = fopen(old_inst_file, "w")) == NULL) {
		/*
		 * Can't open the 'old_inst_file' file for writing.
		 * This is somewhat strange given that the syscall
		 * just succeeded to write a file out.. hmm.. maybe
		 * the fs just filled up or something nasty.
		 *
		 * Anyway, abort what we've done so far.
		 */
		err_print(CANT_UPDATE, old_inst_file);
		err = DEVFSADM_FAILURE;
		goto out;
		/*NOTREACHED*/
	}

	/*
	 * Copy current instance file into the temporary file
	 */
	while ((c = getc(inst_file_fp)) != EOF) {
		if ((err = putc(c, old_inst_file_fp)) == EOF) {
			break;
		}
	}

	if (s_fclose(old_inst_file_fp) == EOF || err == EOF) {
		vprint(INFO_MID, CANT_UPDATE, old_inst_file);
		err = DEVFSADM_FAILURE;
		goto out;
		/* NOTREACHED */
	}

	/*
	 * Set permissions to be the same on the backup as
	 * /etc/path_to_inst.
	 */
	(void) chmod(old_inst_file, 0444);

	/*
	 * So far, everything we've done is more or less reversible.
	 * But now we're going to commit ourselves.
	 */

	(void) sprintf(old_inst_file_npid, "%s.old", inst_file);

	if ((err = rename(old_inst_file, old_inst_file_npid)) != 0) {
		vprint(INFO_MID, RENAME_FAILED, old_inst_file_npid,
				strerror(errno));
	} else if ((err = rename(new_inst_file, inst_file)) != 0) {
		vprint(INFO_MID, RENAME_FAILED, inst_file, strerror(errno));
	}

out:
	if (inst_file_fp != NULL) {
		if (s_fclose(inst_file_fp) == EOF) {
			err_print(FCLOSE_FAILED, inst_file, strerror(errno));
		}
	}

	if (new_inst_file != NULL) {
		if (stat(new_inst_file, &sb) == 0) {
			s_unlink(new_inst_file);
		}
		free(new_inst_file);
	}

	if (old_inst_file != NULL) {
		if (stat(old_inst_file, &sb) == 0) {
			s_unlink(old_inst_file);
		}
		free(old_inst_file);
	}

	if (old_inst_file_npid != NULL)
		free(old_inst_file_npid);

	if (err != 0) {
		err_print(FAILED_TO_UPDATE, inst_file);
	}
}

/*
 * detach from tty.  For daemon mode.
 */
void
detachfromtty()
{
	(void) setsid();
	if (DEVFSADM_DEBUG_ON == TRUE) {
		return;
	}

	(void) close(0);
	(void) close(1);
	(void) close(2);
	(void) open("/dev/null", O_RDWR, 0);
	(void) dup(0);
	(void) dup(0);
	openlog(DEVFSADMD, LOG_PID, LOG_DAEMON);
	(void) setlogmask(LOG_UPTO(LOG_INFO));
	logflag = TRUE;
}

/*
 * Use an advisory lock to synchronize updates to /dev.  If the lock is
 * held by another process, block in the fcntl() system call until that
 * process drops the lock or exits.  The lock file itself is
 * DEV_LOCK_FILE.  The process id of the current and last process owning
 * the lock is kept in the lock file.  After acquiring the lock, read the
 * process id and return it.  It is the process ID which last owned the
 * lock, and will be used to determine if caches need to be flushed.
 */
pid_t
enter_dev_lock()
{
	struct flock lock;
	int n;
	pid_t pid;
	pid_t last_owner_pid;

	if (file_mods == FALSE) {
		return (0);
	}

	s_mkdirp(dev_dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	(void) sprintf(dev_lockfile, "%s/%s", dev_dir, DEV_LOCK_FILE);

	vprint(LOCK_MID, "enter_dev_lock: lock file %s\n", dev_lockfile);

	dev_lock_fd = open(dev_lockfile, O_CREAT|O_RDWR, 0644);
	if (dev_lock_fd < 0) {
		err_print(OPEN_FAILED, dev_lockfile, strerror(errno));
		devfsadm_exit(14);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	/* try for the lock, but don't wait */
	if (fcntl(dev_lock_fd, F_SETLK, &lock) == -1) {
		if ((errno == EACCES) || (errno == EAGAIN)) {
			pid = 0;
			n = read(dev_lock_fd, &pid, sizeof (pid_t));
			vprint(LOCK_MID, "waiting for PID %d to complete\n",
				pid);
			if (lseek(dev_lock_fd, 0, SEEK_SET) == (off_t)-1) {
				err_print(LSEEK_FAILED, dev_lockfile,
						strerror(errno));
				devfsadm_exit(15);
			}
			/* wait for the lock */
			if (fcntl(dev_lock_fd, F_SETLKW, &lock) == -1) {
				err_print(LOCK_FAILED, dev_lockfile,
						strerror(errno));
				devfsadm_exit(16);
			}
		}
	}

	hold_dev_lock = TRUE;
	pid = 0;
	n = read(dev_lock_fd, &pid, sizeof (pid_t));
	if (n == sizeof (pid_t) && pid == getpid()) {
		return (pid);
	}

	last_owner_pid = pid;

	if (lseek(dev_lock_fd, 0, SEEK_SET) == (off_t)-1) {
		err_print(LSEEK_FAILED, dev_lockfile, strerror(errno));
		devfsadm_exit(17);
	}
	pid = getpid();
	n = write(dev_lock_fd, &pid, sizeof (pid_t));
	if (n != sizeof (pid_t)) {
		err_print(WRITE_FAILED, dev_lockfile, strerror(errno));
		devfsadm_exit(18);
	}

	return (last_owner_pid);
}

/*
 * Drop the advisory /dev lock, close lock file.  Close and re-open the
 * file every time so to ensure a resync if for some reason the lock file
 * gets removed.
 */
void
exit_dev_lock()
{
	struct flock unlock;

	if (hold_dev_lock == FALSE) {
		return;
	}

	vprint(LOCK_MID, "exit_dev_lock: lock file %s\n", dev_lockfile);

	unlock.l_type = F_UNLCK;
	unlock.l_whence = SEEK_SET;
	unlock.l_start = 0;
	unlock.l_len = 0;

	if (fcntl(dev_lock_fd, F_SETLK, &unlock) == -1) {
		err_print(UNLOCK_FAILED, dev_lockfile, strerror(errno));
	}

	hold_dev_lock = FALSE;

	if (close(dev_lock_fd) == -1) {
		err_print(CLOSE_FAILED, dev_lockfile, strerror(errno));
		devfsadm_exit(19);
	}
}

/*
 *
 * Use an advisory lock to ensure that only one daemon process is active
 * in the system at any point in time.	If the lock is held by another
 * process, do not block but return the pid owner of the lock to the
 * caller immediately.	The lock is cleared if the holding daemon process
 * exits for any reason even if the lock file remains, so the daemon can
 * be restarted if necessary.  The lock file is DAEMON_LOCK_FILE.
 */
pid_t
enter_daemon_lock(void)
{
	struct flock lock;

	s_mkdirp(dev_dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	(void) sprintf(daemon_lockfile, "%s/%s", dev_dir, DAEMON_LOCK_FILE);

	vprint(LOCK_MID, "enter_daemon_lock: lock file %s\n", daemon_lockfile);

	daemon_lock_fd = open(daemon_lockfile, O_CREAT|O_RDWR, 0644);
	if (daemon_lock_fd < 0) {
		err_print(OPEN_FAILED, daemon_lockfile, strerror(errno));
		devfsadm_exit(20);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == -1) {

		if (errno == EAGAIN || errno == EDEADLK) {
			if (fcntl(daemon_lock_fd, F_GETLK, &lock) == -1) {
				err_print(LOCK_FAILED, daemon_lockfile,
						strerror(errno));
				devfsadm_exit(21);
			}
			return (lock.l_pid);
		}
	}
	hold_daemon_lock = TRUE;
	return (getpid());
}

/*
 * Drop the advisory daemon lock, close lock file
 */
void
exit_daemon_lock(void)
{
	struct flock lock;

	if (hold_daemon_lock == FALSE) {
		return;
	}

	vprint(LOCK_MID, "exit_daemon_lock: lock file %s\n", daemon_lockfile);

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == -1) {
		err_print(UNLOCK_FAILED, daemon_lockfile, strerror(errno));
	}

	if (close(daemon_lock_fd) == -1) {
		err_print(CLOSE_FAILED, daemon_lockfile, strerror(errno));
		devfsadm_exit(22);
	}
}

/*
 * Called to removed danging nodes in two different modes: RM_PRE, RM_POST.
 * RM_PRE mode is called before processing the entire devinfo tree, and RM_POST
 * is called after processing the entire devinfo tree.
 */
static void
pre_and_post_cleanup(int flags)
{
	remove_list_t *rm;
	recurse_dev_t rd;
	cleanup_data_t cleanup_data;
	char *fcn = "pre_and_post_cleanup: ";

	vprint(REMOVE_MID, "%sflags = %d\n", fcn, flags);

	/*
	 * the generic function recurse_dev_re is shared amoung different
	 * functions, so set the method and data that it should use for
	 * matches.
	 */
	rd.fcn = matching_dev;
	rd.data = (void *)&cleanup_data;
	cleanup_data.flags = flags;

	for (rm = remove_head; rm != NULL; rm = rm->next) {
		if ((flags & rm->remove->flags) == flags) {
			cleanup_data.rm = rm;
			/*
			 * If reached this point, RM_PRE or RM_POST cleanup is
			 * desired.  clean_ok() decides whether to clean
			 * under the given circumstances.
			 */
			vprint(REMOVE_MID, "%scleanup: PRE or POST\n", fcn);
			if (clean_ok(rm->remove) == DEVFSADM_SUCCESS) {
				vprint(REMOVE_MID, "cleanup: cleanup OK\n");
				recurse_dev_re(dev_dir, rm->remove->
					dev_dirs_re, &rd);
			}
		}
	}
}

/*
 * clean_ok() determines whether cleanup should be done according
 * to the following matrix:
 *
 * command line arguments RM_PRE    RM_POST	  RM_PRE &&    RM_POST &&
 *						  RM_ALWAYS    RM_ALWAYS
 * ---------------------- ------     -----	  ---------    ----------
 *
 * <neither -c nor -C>	  -	    -		  pre-clean    post-clean
 *
 * -C			  pre-clean  post-clean   pre-clean    post-clean
 *
 * -C -c class		  pre-clean  post-clean   pre-clean    post-clean
 *			  if class  if class	  if class     if class
 *			  matches   matches	  matches      matches
 *
 * -c class		   -	       -	  pre-clean    post-clean
 *						  if class     if class
 *						  matches      matches
 *
 */
static int
clean_ok(devfsadm_remove_t *remove)
{
	int i;

	if (single_drv == TRUE) {
		/* no cleanup at all when using -i option */
		return (DEVFSADM_FAILURE);
	}

	/* if the cleanup flag was not specified, return false */
	if ((cleanup == FALSE) && ((remove->flags & RM_ALWAYS) == 0)) {
		return (DEVFSADM_FAILURE);
	}

	if (num_classes == 0) {
		return (DEVFSADM_SUCCESS);
	}

	/*
	 * if reached this point, check to see if the class in the given
	 * remove structure matches a class given on the command line
	 */

	for (i = 0; i < num_classes; i++) {
		if (strcmp(remove->device_class, classes[i]) == 0) {
			return (DEVFSADM_SUCCESS);
		}
	}

	return (DEVFSADM_FAILURE);
}

/*
 * Called to remove dangling nodes after receiving a hotplug event
 * containing the physical node pathname to be removed.
 */
void
hot_cleanup(char *node_path, char *minor_name)
{
	link_t *link, *next;
	remove_list_t *rm;
	char *fcn = "hot_cleanup: ";
	char path[PATH_MAX + 1];
	int path_len;

	(void) strcpy(path, node_path);
	(void) strcat(path, ":");
	(void) strcat(path, minor_name == NULL ? "" : minor_name);

	path_len = strlen(path);

	vprint(REMOVE_MID, "%spath=%s\n", fcn, path);

	for (rm = remove_head; rm != NULL; rm = rm->next) {
		if ((RM_HOT & rm->remove->flags) == RM_HOT) {
			for (link = get_cached_links(rm->remove->dev_dirs_re);
			    link != NULL; link = next) {
				/*
				 * save the next link, since this link might
				 * get removed.
				 */
				next = link->next;
				if (strncmp(link->contents, path,
					path_len) == 0) {
					(*(rm->remove->callback_fcn))
						(link->devlink);
					vprint(REMOVE_MID,
						"%sremoving %s -> %s\n", fcn,
						link->devlink, link->contents);
				}
			}
		}
	}
}

/*
 * Open the dir current_dir.  For every file which matches the first dir
 * component of path_re, recurse.  If there are no more *dir* path
 * components left in path_re (ie no more /), then call function rd->fcn.
 */
static void
recurse_dev_re(char *current_dir, char *path_re, recurse_dev_t *rd)
{
	regex_t re1;
	char *slash;
	char new_path[PATH_MAX + 1];
	char *anchored_path_re;
	struct dirent *entp;
	struct dirent *retp;
	DIR *dp;

	vprint(RECURSEDEV_MID, "recurse_dev_re: curr = %s path=%s\n",
		current_dir, path_re);

	if ((dp = opendir(current_dir)) == NULL) {
		return;
	}

	if ((slash = strchr(path_re, '/')) != NULL) {
		*slash = '\0';
	}

	entp = s_malloc(PATH_MAX + 1 + sizeof (struct  dirent));

	anchored_path_re = s_malloc(strlen(path_re) + 3);
	anchored_path_re[0] = '^';
	(void) strcpy(&(anchored_path_re[1]), path_re);
	(void) strcat(anchored_path_re, "$");

	if (regcomp(&re1, anchored_path_re, REG_EXTENDED) != 0) {
		free(anchored_path_re);
		goto out;
	}

	free(anchored_path_re);

	while (readdir_r(dp, entp, &retp) == 0) {

		/* See 4062296 to understand readdir_r semantics */
		if (retp == NULL) {
			break;
		}

		if (strcmp(entp->d_name, ".") == 0 ||
		    strcmp(entp->d_name, "..") == 0) {
			continue;
		}

		if (regexec(&re1, entp->d_name, 0, NULL, 0) == 0) {
			/* match */
			(void) strcpy(new_path, current_dir);
			(void) strcat(new_path, "/");
			(void) strcat(new_path, entp->d_name);

			vprint(RECURSEDEV_MID, "recurse_dev_re: match, new "
				"path = %s\n", new_path);

			if (slash != NULL) {
				recurse_dev_re(new_path, slash + 1, rd);
			} else {
				/* reached the leaf component of path_re */
				vprint(RECURSEDEV_MID,
					"recurse_dev_re: calling fcn\n");
				(*(rd->fcn))(new_path, rd->data);
			}
		}
	}

	regfree(&re1);

out:
	if (slash != NULL) {
		*slash = '/';
	}
	free(entp);
	s_closedir(dp);
}

/*
 *  Found a devpath which matches a RE in the remove structure.
 *  Now check to see if it is dangling.
 */
static void
matching_dev(char *devpath, void *data)
{
	cleanup_data_t *cleanup_data = data;
	char *fcn = "matching_dev: ";

	vprint(RECURSEDEV_MID, "%sexamining devpath = '%s'\n", fcn,
			devpath);

	if (dangling(devpath) == TRUE) {
		if (call_minor_init(cleanup_data->rm->modptr) ==
				DEVFSADM_FAILURE) {
			return;
		}

		devpath += strlen(dev_dir) + strlen("/");

		vprint(RECURSEDEV_MID, "%scalling"
			" callback %s\n", fcn, devpath);
		(*(cleanup_data->rm->remove->callback_fcn))(devpath);
	}
}

int
devfsadm_link_valid(char *link)
{
	struct stat sb;
	char devlink[PATH_MAX + 1];

	/* prepend link with dev_dir contents */
	(void) strcpy(devlink, dev_dir);
	(void) strcat(devlink, "/");
	(void) strcat(devlink, link);

	if (lstat(devlink, &sb) != 0) {
		return (DEVFSADM_FALSE);
	}

	if (dangling(devlink) == TRUE) {
		return (DEVFSADM_FALSE);
	} else {
		return (DEVFSADM_TRUE);
	}
}

static int
dangling(char *devpath)
{
	di_node_t result;
	di_minor_t minor;
	char contents[PATH_MAX + 1];
	char stage_link[PATH_MAX + 1];
	char *fcn = "dangling: ";
	char *ptr;
	char *slash;
	char *colon;
	char *busaddr;
	char *at;
	int linksize;
	int rv = TRUE;

	/*
	 * we may not have a full-snapshot of the tree, especially
	 * in hot plug case
	 */
	if (root_node == NULL) {
		/* take snapshot, don't load drivers */
		if ((root_node = di_init("/", DINFOCPYALL)) ==
		    DI_NODE_NIL) {
			err_print(DI_INIT_FAILED, "/", strerror(errno));
			devfsadm_exit(12);
		}
	}

	linksize = readlink(devpath, contents, PATH_MAX);

	if (linksize <= 0) {
		return (FALSE);
	} else {
		contents[linksize] = '\0';
	}
	vprint(REMOVE_MID, "%s %s -> %s\n", fcn, devpath, contents);

	/*
	 * Check to see if this is a link pointing to another link in /dev.  The
	 * cheap way to do this is to look for a lack of ../devices/.
	 */

	if (is_minor_node(contents, &ptr) == DEVFSADM_FALSE) {
		/*
		 * assume that linkcontents is really a pointer to another
		 * link, and if so recurse and read its link contents.
		 */
		if (strncmp(contents, DEV "/", strlen(DEV) + 1) == 0)  {
			(void) strcpy(stage_link, dev_dir);
			(void) strcat(stage_link, "/");
			(void) strcpy(stage_link,
					&contents[strlen(DEV) + strlen("/")]);
		} else {
			if ((ptr = strrchr(devpath, '/')) == NULL) {
				vprint(REMOVE_MID, "%s%s -> %s invalid link. "
					"missing '/'\n", fcn, devpath,
					contents);
				return (TRUE);
			}
			*ptr = '\0';
			(void) strcpy(stage_link, devpath);
			*ptr = '/';
			(void) strcat(stage_link, "/");
			(void) strcat(stage_link, contents);

		}
		return (dangling(stage_link));
	} else {
		ptr++; /* so that it points after the / following /devices */
	}

	result = root_node;

	while (*ptr != '\0') {

		if ((slash = strchr(ptr, '/')) != NULL) {
			*slash = '\0';
		}
		if ((colon = strrchr(ptr, ':')) != NULL) {
			*colon	= '\0';
		}
		if ((at = strchr(ptr, '@')) != NULL) {
			*at = '\0';
		}

		for (result = di_child_node(result); result != NULL;
		    result = di_sibling_node(result)) {

			busaddr = di_bus_addr(result);

			vprint(REMOVE_MID, "%sptr=%s, node_name=%s bus=%s\n",
				fcn, ptr, di_node_name(result),
				((busaddr == NULL) ? "<>" : busaddr));

			if ((strcmp(ptr, di_node_name(result)) == 0) &&
			    (((at == NULL) && (*busaddr == '\0')) ||
			    (busaddr && at && (strcmp(busaddr, at + 1) ==
			    0)))) {
				break;
			}
		}

		if (slash != NULL) {
			*slash = '/';
		}
		if (at != NULL) {
			*at = '@';
		}
		if (colon != NULL) {
			*colon = ':';
		}

		if (result == NULL)  {
			vprint(REMOVE_MID, "%snode not present\n", fcn);
			break;
		}

		if (slash == NULL) {
			break;	/* target devinfo found */
		} else {
			ptr = slash + 1;
		}
	}

	if (result != NULL) {
		vprint(REMOVE_MID, "checking for minor names \n");
		for (minor = di_minor_next(result, DI_MINOR_NIL);
		    minor != NULL; minor = di_minor_next(result, minor)) {

			vprint(REMOVE_MID, "%sminor_name=%s\n",
					fcn, di_minor_name(minor));

			if (strcmp(di_minor_name(minor), colon + 1) == 0) {
				rv = FALSE;
				break;
			}
		}
	}

	vprint(REMOVE_MID, "%slink=%s, returning %s\n", fcn,
			devpath, ((rv == TRUE) ? "TRUE" : "FALSE"));

	return (rv);
}

/*
 * Returns the substring of interest, given a path.
 */
static char *
alloc_cmp_str(const char *path, devfsadm_enumerate_t *dep)
{
	uint_t match;
	char *np, *ap, *mp;
	char *cmp_str = NULL;
	char *fcn = "alloc_cmp_str";

	np = ap = mp = NULL;

	/*
	 * extract match flags from the flags argument.
	 */
	match = (dep->flags & MATCH_MASK);

	vprint(ENUM_MID, "%s: enumeration match type: 0x%x"
	    " path: %s\n", fcn, match, path);

	/*
	 * MATCH_CALLBACK and MATCH_ALL are the only flags
	 * which may be used if "path" is a /dev path
	 */
	if (match == MATCH_CALLBACK) {
		if (dep->sel_fcn == NULL) {
			vprint(ENUM_MID, "%s: invalid enumerate"
			    " callback: path: %s\n", fcn, path);
			return (NULL);
		}
		cmp_str = dep->sel_fcn(path, dep->cb_arg);
		return (cmp_str);
	}

	cmp_str = s_strdup(path);

	if (match == MATCH_ALL) {
		return (cmp_str);
	}

	/*
	 * The remaining flags make sense only for /devices
	 * paths
	 */
	if ((mp = strrchr(cmp_str, ':')) == NULL) {
		vprint(ENUM_MID, "%s: invalid path: %s\n",
		    fcn, path);
		goto err;
	}

	if (match == MATCH_MINOR) {
		/* A NULL "match_arg" values implies entire minor */
		if (get_component(mp + 1, dep->match_arg) == NULL) {
			vprint(ENUM_MID, "%s: invalid minor component:"
			    " path: %s\n", fcn, path);
			goto err;
		}
		return (cmp_str);
	}

	if ((np = strrchr(cmp_str, '/')) == NULL) {
		vprint(ENUM_MID, "%s: invalid path: %s\n", fcn, path);
		goto err;
	}

	if (match == MATCH_PARENT) {
		if (strcmp(cmp_str, "/") == 0) {
			vprint(ENUM_MID, "%s: invalid path: %s\n",
			    fcn, path);
			goto err;
		}

		if (np == cmp_str) {
			*(np + 1) = '\0';
		} else {
			*np = '\0';
		}
		return (cmp_str);
	}

	if ((ap = strchr(np+1, '@')) == NULL) {
		vprint(ENUM_MID, "%s: invalid path: %s\n", fcn, path);
		goto err;
	}

	if (match == MATCH_NODE) {
		*ap = '\0';
		return (cmp_str);
	} else if (match == MATCH_ADDR) {
		*mp = '\0';
		if (get_component(ap + 1, dep->match_arg) == NULL) {
			vprint(ENUM_MID, "%s: invalid minor component:"
			    " path: %s\n", fcn, path);
			goto err;
		}
		return (cmp_str);
	}

	vprint(ENUM_MID, "%s: invalid enumeration flags: 0x%x"
		" path: %s\n", fcn, dep->flags, path);

	/*FALLTHRU*/
err:
	free(cmp_str);
	return (NULL);
}


/*
 * "str" is expected to be a string with components separated by ','
 * The terminating null char is considered a separator.
 * get_component() will remove the portion of the string beyond
 * the component indicated.
 * If comp_str is NULL, the entire "str" is returned.
 */
static char *
get_component(char *str, const char *comp_str)
{
	long comp;
	char *cp;

	if (str == NULL) {
		return (NULL);
	}

	if (comp_str == NULL) {
		return (str);
	}

	errno = 0;
	comp = strtol(comp_str, &cp, 10);
	if (errno != 0 || *cp != '\0' || comp < 0) {
		return (NULL);
	}

	if (comp == 0)
		return (str);

	for (cp = str; ; cp++) {
		if (*cp == ',' || *cp == '\0')
			comp--;
		if (*cp == '\0' || comp <= 0) {
			break;
		}
	}

	if (comp == 0) {
		*cp = '\0';
	} else {
		str = NULL;
	}

	return (str);
}


/*
 * Enumerate serves as a generic counter as well as a means to determine
 * logical unit/controller numbers for such items as disk and tape
 * drives.
 *
 * rules[] is an array of  devfsadm_enumerate_t structures which defines
 * the enumeration rules to be used for a specified set of links in /dev.
 * The set of links is specified through regular expressions (of the flavor
 * described in regex(5)). These regular expressions are used to determine
 * the set of links in /dev to examine. The last path component in these
 * regular expressions MUST contain a parethesized subexpression surrounding
 * the RE which is to be considered the enumerating component. The subexp
 * member in a rule is the subexpression number of the enumerating
 * component. Subexpressions in the last path component are numbered starting
 * from 1.
 *
 * A cache of current id assignments is built up from existing symlinks and
 * new assignments use the lowest unused id. Assignments are based on a
 * match of a specified substring of a symlink's contents. If the specified
 * component for the devfs_path argument matches the corresponding substring
 * for a existing symlink's contents, the cached id is returned. Else, a new
 * id is created and returned in *buf. *buf must be freed by the caller.
 *
 * An id assignment may be governed by a combination of rules, each rule
 * applicable to a different subset of links in /dev. For example, controller
 * numbers may be determined by a combination of disk symlinks in /dev/[r]dsk
 * and controller symlinks in /dev/cfg, with the two sets requiring different
 * rules to derive the "substring of interest". In such cases, the rules
 * array will have more than one element.
 */
int
devfsadm_enumerate_int(char *devfs_path, int index, char **buf,
			devfsadm_enumerate_t rules[], int nrules)
{
	return (find_enum_id(rules, nrules,
	    devfs_path, index, "0", INTEGER, buf));
}

/*
 * Same as above, but allows a starting value to be specified.
 * Private to devfsadm.... used by devlinks.
 */
static int
devfsadm_enumerate_int_start(char *devfs_path, int index, char **buf,
		devfsadm_enumerate_t rules[], int nrules, char *start)
{
	return (find_enum_id(rules, nrules,
	    devfs_path, index, start, INTEGER, buf));
}

/*
 *  devfsadm_enumerate_char serves as a generic counter returning
 *  a single letter.
 */
int
devfsadm_enumerate_char(char *devfs_path, int index, char **buf,
			devfsadm_enumerate_t rules[], int nrules)
{
	return (find_enum_id(rules, nrules,
	    devfs_path, index, "a", LETTER, buf));
}


/*
 * For a given numeral_set (see get_cached_set for desc of numeral_set),
 * search all cached entries looking for matches on a specified substring
 * of devfs_path. The substring is derived from devfs_path based on the
 * rule specified by "index". If a match is found on a cached entry,
 * return the enumerated id in buf. Otherwise, create a new id by calling
 * new_id, then cache and return that entry.
 */
static int
find_enum_id(devfsadm_enumerate_t rules[], int nrules,
	char *devfs_path, int index, char *min, int type, char **buf)
{
	numeral_t *matchnp;
	numeral_t *numeral;
	int matchcount = 0;
	char *cmp_str;
	char *fcn = "find_enum_id";
	numeral_set_t *set;

	if (devfs_path == NULL) {
		vprint(ENUM_MID, "%s: NULL path\n", fcn);
		return (DEVFSADM_FAILURE);
	}

	if (rules == NULL || nrules <= 0 || index < 0 ||
	    index >= nrules || buf == NULL) {
		vprint(ENUM_MID, "%s: invalid arguments. path: %s\n",
		    fcn, devfs_path);
		return (DEVFSADM_FAILURE);
	}

	*buf = NULL;

	cmp_str = alloc_cmp_str(devfs_path, &rules[index]);
	if (cmp_str == NULL) {
		return (DEVFSADM_FAILURE);
	}

	if ((set = get_enum_cache(rules, nrules)) == NULL) {
		free(cmp_str);
		return (DEVFSADM_FAILURE);
	}

	assert(nrules == set->re_count);

	/*
	 * Check and see if a matching entry is already cached.
	 */
	matchcount = lookup_enum_cache(set, cmp_str, rules, index,
	    &matchnp);

	if (matchcount < 0 || matchcount > 1) {
		free(cmp_str);
		return (DEVFSADM_FAILURE);
	}

	/* if matching entry already cached, return it */
	if (matchcount == 1) {
		*buf = s_strdup(matchnp->id);
		free(cmp_str);
		return (DEVFSADM_SUCCESS);
	}

	/*
	 * no cached entry, initialize a numeral struct
	 * by calling new_id() and cache onto the numeral_set
	 */
	numeral = s_malloc(sizeof (numeral_t));
	numeral->id = new_id(set->headnumeral, type, min);
	numeral->full_path = s_strdup(devfs_path);
	numeral->rule_index = index;
	numeral->cmp_str = cmp_str;
	cmp_str = NULL;

	/* insert to head of list for fast lookups */
	numeral->next = set->headnumeral;
	set->headnumeral = numeral;

	*buf = s_strdup(numeral->id);
	return (DEVFSADM_SUCCESS);
}


/*
 * Looks up the specified cache for a match with a specified string
 * Returns:
 *	-1	: on error.
 *	0/1/2	: Number of matches.
 * Returns the matching element only if there is a single match.
 * If the "uncached" flag is set, derives the "cmp_str" afresh
 * for the match instead of using cached values.
 */
static int
lookup_enum_cache(numeral_set_t *set, char *cmp_str,
	devfsadm_enumerate_t rules[], int index, numeral_t **matchnpp)
{
	int matchcount = 0, rv = -1;
	int uncached;
	numeral_t *np;
	char *fcn = "lookup_enum_cache";
	char *cp;

	*matchnpp = NULL;

	assert(index < set->re_count);

	if (cmp_str == NULL) {
		return (-1);
	}

	uncached = 0;
	if ((rules[index].flags & MATCH_UNCACHED) == MATCH_UNCACHED) {
		uncached = 1;
	}

	/*
	 * Check and see if a matching entry is already cached.
	 */
	for (np = set->headnumeral; np != NULL; np = np->next) {
		if (np->cmp_str == NULL) {
			vprint(ENUM_MID, "%s: invalid entry in enumerate"
			    " cache. path: %s\n", fcn, np->full_path);
			return (-1);
		}

		if (uncached) {
			vprint(CHATTY_MID, "%s: bypassing enumerate cache."
			    " path: %s\n", fcn, cmp_str);
			cp = alloc_cmp_str(np->full_path,
			    &rules[np->rule_index]);
			if (cp == NULL)
				return (-1);
			rv = strcmp(cmp_str, cp);
			free(cp);
		} else {
			rv = strcmp(cmp_str, np->cmp_str);
		}

		if (rv == 0) {
			if (matchcount++ != 0) {
				break; /* more than 1 match. */
			}
			*matchnpp = np;
		}
	}

	return (matchcount);
}

#ifdef	DEBUG
static void
dump_enum_cache(numeral_set_t *setp)
{
	int i;
	numeral_t *np;
	char *fcn = "dump_enum_cache";

	vprint(ENUM_MID, "%s: re_count = %d\n", fcn, setp->re_count);
	for (i = 0; i < setp->re_count; i++) {
		vprint(ENUM_MID, "%s: re[%d] = %s\n", fcn, i, setp->re[i]);
	}

	for (np = setp->headnumeral; np != NULL; np = np->next) {
		vprint(ENUM_MID, "%s: id: %s\n", fcn, np->id);
		vprint(ENUM_MID, "%s: full_path: %s\n", fcn, np->full_path);
		vprint(ENUM_MID, "%s: rule_index: %d\n", fcn, np->rule_index);
		vprint(ENUM_MID, "%s: cmp_str: %s\n", fcn, np->cmp_str);
	}
}
#endif

/*
 * For a given set of regular expressions in rules[], this function returns
 * either a previously cached struct numeral_set or it will create and
 * cache a new struct numeral_set.  There is only one struct numeral_set
 * for the combination of REs present in rules[].  Each numeral_set contains
 * the regular expressions in rules[] used for cache selection AND a linked
 * list of struct numerals, ONE FOR EACH *UNIQUE* numeral or character ID
 * selected by the grouping parenthesized subexpression found in the last
 * path component of each rules[].re.  For example, the RE: "rmt/([0-9]+)"
 * selects all the logical nodes of the correct form in dev/dsk and dev/rdsk.
 * Each rmt/X will store a *single* struct numeral... ie 0, 1, 2 each get a
 * single struct numeral. There is no need to store more than a single logical
 * node matching X since the information desired in the devfspath would be
 * identical for the portion of the devfspath of interest. (the part up to,
 * but not including the minor name in this example.)
 *
 * If the given numeral_set is not yet cached, call enumerate_recurse to
 * create it.
 */
static numeral_set_t *
get_enum_cache(devfsadm_enumerate_t rules[], int nrules)
{
	/* linked list of numeral sets */
	numeral_set_t *setp;
	int i;
	char *path_left;
	char *fcn = "get_enum_cache";

	/*
	 * See if we've already cached this numeral set.
	 */
	for (setp = head_numeral_set; setp != NULL; setp = setp->next) {
		/*
		 *  check all regexp's passed in function against
		 *  those in cached set.
		 */
		if (nrules != setp->re_count) {
			continue;
		}

		for (i = 0; i < nrules; i++) {
			if (strcmp(setp->re[i], rules[i].re) != 0) {
				break;
			}
		}

		if (i == nrules) {
			return (setp);
		}
	}

	/*
	 * If the MATCH_UNCACHED flag is set, we should not  be here.
	 */
	for (i = 0; i < nrules; i++) {
		if ((rules[i].flags & MATCH_UNCACHED) == MATCH_UNCACHED) {
			vprint(ENUM_MID, "%s: invalid enumeration flags: "
			    "0x%x\n", fcn, rules[i].flags);
			return (NULL);
		}
	}

	/*
	 *  Since we made it here, we have not yet cached the given set of
	 *  logical nodes matching the passed re.  Create a cached entry
	 *  struct numeral_set and populate it with a minimal set of
	 *  logical nodes from /dev.
	 */

	setp = s_malloc(sizeof (numeral_set_t));
	setp->re = s_malloc(sizeof (char *) * nrules);
	for (i = 0; i < nrules; i++) {
		setp->re[i] = s_strdup(rules[i].re);
	}
	setp->re_count = nrules;
	setp->headnumeral = NULL;

	/* put this new cached set on the cached set list */
	setp->next = head_numeral_set;
	head_numeral_set = setp;

	/*
	 * For each RE, search disk and cache any matches on the
	 * numeral list.
	 */
	for (i = 0; i < nrules; i++) {
		path_left = s_strdup(setp->re[i]);
		enumerate_recurse(dev_dir, path_left, setp, rules, i);
		free(path_left);
	}

#ifdef	DEBUG
	dump_enum_cache(setp);
#endif

	return (setp);
}


/*
 * This function stats the pathname namebuf.  If this is a directory
 * entry, we recurse down dname/fname until we find the first symbolic
 * link, and then stat and return it.  This is valid for the same reason
 * that we only need to read a single pathname for multiple matching
 * logical ID's... ie, all the logical nodes should contain identical
 * physical paths for the parts we are interested.
 */
int
get_stat_info(char *namebuf, struct stat *sb)
{
	struct dirent *entp;
	struct dirent *retp;
	DIR *dp;
	char *cp;

	if (lstat(namebuf, sb) < 0) {
		(void) err_print(LSTAT_FAILED, namebuf, strerror(errno));
		return (DEVFSADM_FAILURE);
	}

	if ((sb->st_mode & S_IFMT) == S_IFLNK) {
		return (DEVFSADM_SUCCESS);
	}

	/*
	 * If it is a dir, recurse down until we find a link and
	 * then use the link.
	 */
	if ((sb->st_mode & S_IFMT) == S_IFDIR) {

		if ((dp = opendir(namebuf)) == NULL) {
			return (DEVFSADM_FAILURE);
		}

		entp = s_malloc(PATH_MAX + 1 + sizeof (struct  dirent));

		/*
		 *  Search each dir entry looking for a symlink.  Return
		 *  the first symlink found in namebuf.  Recurse dirs.
		 */
		while (readdir_r(dp,  entp, &retp) == 0) {
			if (retp == NULL) {
				break;
			}
			if (strcmp(entp->d_name, ".") == 0 ||
			    strcmp(entp->d_name, "..") == 0) {
				continue;
			}

			cp = namebuf + strlen(namebuf);
			(void) strcat(namebuf, "/");
			(void) strcat(namebuf, entp->d_name);
			if (get_stat_info(namebuf, sb) == DEVFSADM_SUCCESS) {
				free(entp);
				s_closedir(dp);
				return (DEVFSADM_SUCCESS);
			}
			*cp = '\0';
		}
		free(entp);
		s_closedir(dp);
	}

	/* no symlink found, so return error */
	return (DEVFSADM_FAILURE);
}

/*
 * An existing matching ID was not found, so this function is called to
 * create the next lowest ID.  In the INTEGER case, return the next
 * lowest unused integer.  In the case of LETTER, return the next lowest
 * unused letter.  Return empty string if all 26 are used.
 * NOTE - The min argument is ignored for type == LETTER, because no
 * consumer needs it.
 */
char *
new_id(numeral_t *numeral, int type, char *min)
{
	int imin;
	temp_t *temp;
	temp_t *ptr;
	temp_t **previous;
	temp_t *head = NULL;
	char *retval;
	static char tempbuff[8];
	numeral_t *np;

	if (type == LETTER) {

		char letter[26], i;

		if (numeral == NULL) {
			return (s_strdup(min));
		}

		for (i = 0; i < 26; i++) {
			letter[i] = 0;
		}

		for (np = numeral; np != NULL; np = np->next) {
			letter[*np->id - 'a']++;
		}

		for (i = 0; i < 26; i++) {
			if (letter[i] == 0) {
				retval = s_malloc(2);
				retval[0] = 'a' + i;
				retval[1] = '\0';
				return (retval);
			}
		}

		return (s_strdup(""));
	}

	if (type == INTEGER) {

		if (numeral == NULL) {
			return (s_strdup(min));
		}

		imin = atoi(min);

		/* sort list */
		for (np = numeral; np != NULL; np = np->next) {
			temp = s_malloc(sizeof (temp_t));
			temp->integer = atoi(np->id);
			temp->next = NULL;

			previous = &head;
			for (ptr = head; ptr != NULL; ptr = ptr->next) {
				if (temp->integer < ptr->integer) {
					temp->next = ptr;
					*previous = temp;
					break;
				}
				previous = &(ptr->next);
			}
			if (ptr == NULL) {
				*previous = temp;
			}
		}

		/* now search sorted list for first hole >= imin */
		for (ptr = head; ptr != NULL; ptr = ptr->next) {
			if (imin == ptr->integer) {
				imin++;
			} else {
				if (imin < ptr->integer) {
					break;
				}
			}

		}

		/* free temp list */
		for (ptr = head; ptr != NULL; ) {
			temp = ptr;
			ptr = ptr->next;
			free(temp);
		}

		(void) sprintf(tempbuff, "%d", imin);
		return (s_strdup(tempbuff));
	}

	return (s_strdup(""));
}

/*
 * Search current_dir for all files which match the first path component
 * of path_left, which is an RE.  If a match is found, but there are more
 * components of path_left, then recurse, otherwise, if we have reached
 * the last component of path_left, call create_cached_numerals for each
 * file.   At some point, recurse_dev_re() should be rewritten so that this
 * function can be eliminated.
 */
static void
enumerate_recurse(char *current_dir, char *path_left, numeral_set_t *setp,
	    devfsadm_enumerate_t rules[], int index)
{
	char *slash;
	char *new_path;
	char *numeral_id;
	struct dirent *entp;
	struct dirent *retp;
	DIR *dp;

	if ((dp = opendir(current_dir)) == NULL) {
		return;
	}

	/* get rid of any extra '/' */
	while (*path_left == '/') {
		path_left++;
	}

	if (slash = strchr(path_left, '/')) {
		*slash = '\0';
	}

	entp = s_malloc(PATH_MAX + 1 + sizeof (struct  dirent));

	while (readdir_r(dp, entp, &retp) == 0) {

		if (retp == NULL) {
			break;
		}

		if (strcmp(entp->d_name, ".") == 0 ||
		    strcmp(entp->d_name, "..") == 0) {
			continue;
		}

		/*
		 *  Returns true if path_left matches entp->d_name
		 *  If it is the last path component, pass subexp
		 *  so that it will return the corresponding ID in
		 *  numeral_id.
		 */
		numeral_id = NULL;
		if (match_path_component(path_left, entp->d_name, &numeral_id,
				    slash ? 0 : rules[index].subexp)) {

			new_path = s_malloc(strlen(current_dir) +
					    strlen(entp->d_name) + 2);

			(void) strcpy(new_path, current_dir);
			(void) strcat(new_path, "/");
			(void) strcat(new_path, entp->d_name);

			if (slash != NULL) {
				enumerate_recurse(new_path, slash + 1,
				    setp, rules, index);
			} else {
				create_cached_numeral(new_path, setp,
				    numeral_id, rules, index);
				if (numeral_id != NULL) {
					free(numeral_id);
				}
			}
			free(new_path);
		}
	}

	if (slash != NULL) {
		*slash = '/';
	}
	free(entp);
	s_closedir(dp);
}


/*
 * Returns true if file matches file_re.  If subexp is non-zero, it means
 * we are searching the last path component and need to return the
 * parenthesized subexpression subexp in id.
 *
 */
static int
match_path_component(char *file_re,  char *file,  char **id, int subexp)
{
	regex_t re1;
	int match = 0;
	int nelements;
	regmatch_t *pmatch;

	if (subexp != 0) {
		nelements = subexp + 1;
		pmatch = (regmatch_t *)
			s_malloc(sizeof (regmatch_t) * nelements);
	} else {
		pmatch = NULL;
		nelements = 0;
	}

	if (regcomp(&re1, file_re, REG_EXTENDED) != 0) {
		if (pmatch != NULL) {
			free(pmatch);
		}
		return (0);
	}

	if (regexec(&re1, file, nelements, pmatch, 0) == 0) {
		match = 1;
	}

	if ((match != 0) && (subexp != 0)) {
		int size = pmatch[subexp].rm_eo - pmatch[subexp].rm_so;
		*id = s_malloc(size + 1);
		(void) strncpy(*id, &file[pmatch[subexp].rm_so], size);
		(*id)[size] = '\0';
	}

	if (pmatch != NULL) {
		free(pmatch);
	}
	regfree(&re1);
	return (match);
}

/*
 * This function is called for every file which matched the leaf
 * component of the RE.  If the "numeral_id" is not already on the
 * numeral set's numeral list, add it and its physical path.
 */
static void
create_cached_numeral(char *path, numeral_set_t *setp, char *numeral_id,
	devfsadm_enumerate_t rules[], int index)
{
	char linkbuf[PATH_MAX + 1];
	char lpath[PATH_MAX + 1];
	char *linkptr, *cmp_str;
	numeral_t *np;
	int linksize;
	struct stat sb;

	assert(index >= 0 && index < setp->re_count);
	assert(strcmp(rules[index].re, setp->re[index]) == 0);

	/*
	 *  We found a numeral_id from an entry in /dev which matched
	 *  the re passed in from devfsadm_enumerate.  We only need to make sure
	 *  ONE copy of numeral_id exists on the numeral list.  We only need
	 *  to store /dev/dsk/cNtod0s0 and no other entries hanging off
	 *  of controller N.
	 */
	for (np = setp->headnumeral; np != NULL; np = np->next) {
		if (strcmp(numeral_id, np->id) == 0) {
			return;
		}
	}

	/* NOT on list, so add it */

	(void) strcpy(lpath, path);
	/*
	 * If path is a dir, it is changed to the first symbolic link it find
	 * if it finds one.
	 */
	if (get_stat_info(lpath, &sb) == DEVFSADM_FAILURE) {
		return;
	}

	/* If we get here, we found a symlink */
	linksize = readlink(lpath, linkbuf, PATH_MAX);

	if (linksize <= 0) {
		err_print(READLINK_FAILED, lpath, strerror(errno));
		return;
	}

	linkbuf[linksize] = '\0';

	/*
	 * the following just points linkptr to the root of the /devices
	 * node if it is a minor node, otherwise, to the first char of
	 * linkbuf if it is a link.
	 */
	(void) is_minor_node(linkbuf, &linkptr);

	cmp_str = alloc_cmp_str(linkptr, &rules[index]);
	if (cmp_str == NULL) {
		return;
	}

	np = s_malloc(sizeof (numeral_t));

	np->id = s_strdup(numeral_id);
	np->full_path = s_strdup(linkptr);
	np->rule_index = index;
	np->cmp_str = cmp_str;

	np->next = setp->headnumeral;
	setp->headnumeral = np;
}


/*
 * This should be called either before or after granting access to a
 * command line version of devfsadm running, since it may have changed
 * the state of /dev.  It forces future enumerate calls to re-build
 * cached information from /dev.
 */
void
invalidate_enumerate_cache(void)
{
	numeral_set_t *setp;
	numeral_set_t *savedsetp;
	numeral_t *savednumset;
	numeral_t *numset;
	int i;

	for (setp = head_numeral_set; setp != NULL; ) {
		/*
		 *  check all regexp's passed in function against
		 *  those in cached set.
		 */

		savedsetp = setp;
		setp = setp->next;

		for (i = 0; i < savedsetp->re_count; i++) {
			free(savedsetp->re[i]);
		}
		free(savedsetp->re);

		for (numset = savedsetp->headnumeral; numset != NULL; ) {
			savednumset = numset;
			numset = numset->next;
			assert(savednumset->rule_index < savedsetp->re_count);
			free(savednumset->id);
			free(savednumset->full_path);
			free(savednumset->cmp_str);
			free(savednumset);
		}
		free(savedsetp);
	}
	head_numeral_set = NULL;
}

/*
 * Copies over links from /dev to <root>/dev and device special files in
 * /devices to <root>/devices, preserving the existing file modes.  If
 * the link or special file already exists on <root>, skip the copy.  (it
 * would exist only if a package hard coded it there, so assume package
 * knows best?).  Use /etc/name_to_major and <root>/etc/name_to_major to
 * make translations for major numbers on device special files.	No need to
 * make a translation on minor_perm since if the file was created in the
 * miniroot then it would presumably have the same minor_perm entry in
 *  <root>/etc/minor_perm.  To be used only by install.
 */
int
devfsadm_copy(void)
{
	char filename[PATH_MAX + 1];

	/* load the installed root's name_to_major for translations */
	(void) sprintf(filename, "%s%s", root_dir, NAME_TO_MAJOR);
	if (load_n2m_table(filename) == DEVFSADM_FAILURE) {
		return (DEVFSADM_FAILURE);
	}

	/* copy /dev and /devices */
	(void) nftw(DEV, devfsadm_copy_file, 20, FTW_PHYS);
	(void) nftw(DEVICES, devfsadm_copy_file, 20, FTW_PHYS);

	/* Let install handle copying over path_to_inst */

	return (DEVFSADM_SUCCESS);
}

/*
 * This function copies links, dirs, and device special files.
 * Note that it always returns DEVFSADM_SUCCESS, so that nftw doesn't
 * abort.
 */
/*ARGSUSED*/
static int
devfsadm_copy_file(const char *file, const struct stat *stat,
		    int flags, struct FTW *ftw)
{
	struct stat sp;
	dev_t newdev;
	char newfile[PATH_MAX + 1];
	char linkcontents[PATH_MAX + 1];
	int bytes;

	(void) strcpy(newfile, root_dir);
	(void) strcat(newfile, "/");
	(void) strcat(newfile, file);

	if (lstat(newfile, &sp) == 0) {
		/* newfile already exists, so no need to continue */
		return (DEVFSADM_SUCCESS);
	}

	if (((stat->st_mode & S_IFMT) == S_IFBLK) ||
	    ((stat->st_mode & S_IFMT) == S_IFCHR)) {
		if (translate_major(stat->st_rdev, &newdev) ==
		    DEVFSADM_FAILURE) {
			return (DEVFSADM_SUCCESS);
		}
		if (mknod(newfile, stat->st_mode, newdev) == -1) {
			err_print(MKNOD_FAILED, newfile, strerror(errno));
			return (DEVFSADM_SUCCESS);
		}
	} else if ((stat->st_mode & S_IFMT) == S_IFDIR) {
		if (mknod(newfile, stat->st_mode, 0) == -1) {
			err_print(MKNOD_FAILED, newfile, strerror(errno));
			return (DEVFSADM_SUCCESS);
		}
	} else if ((stat->st_mode & S_IFMT) == S_IFLNK)  {
		if ((bytes = readlink(file, linkcontents, PATH_MAX)) == -1)  {
			err_print(READLINK_FAILED, file, strerror(errno));
			return (DEVFSADM_SUCCESS);
		}
		linkcontents[bytes] = '\0';
		if (symlink(linkcontents, newfile) == -1) {
			err_print(SYMLINK_FAILED, newfile, newfile,
					strerror(errno));
			return (DEVFSADM_SUCCESS);
		}
	}

	(void) lchown(newfile, stat->st_uid, stat->st_gid);
	return (DEVFSADM_SUCCESS);
}

/*
 *  Given a dev_t from the running kernel, return the new_dev_t
 *  by translating to the major number found on the installed
 *  target's root name_to_major file.
 */
static int
translate_major(dev_t old_dev, dev_t *new_dev)
{
	major_t oldmajor;
	major_t newmajor;
	minor_t oldminor;
	minor_t newminor;
	char cdriver[FILENAME_MAX + 1];
	char driver[FILENAME_MAX + 1];
	char *fcn = "translate_major: ";

	oldmajor = major(old_dev);
	if (modctl(MODGETNAME, driver, sizeof (driver),
			    &oldmajor) != 0) {
		return (DEVFSADM_FAILURE);
	}

	if (strcmp(driver, "clone") != 0) {
		/* non-clone case */

		/* look up major number is target's name2major */
		if (get_major_no(driver, &newmajor) == DEVFSADM_FAILURE) {
			return (DEVFSADM_FAILURE);
		}

		*new_dev = makedev(newmajor, minor(old_dev));
		if (old_dev != *new_dev) {
			vprint(CHATTY_MID, "%sdriver: %s old: %lu,%lu "
				"new: %lu,%lu\n", fcn, driver, major(old_dev),
				minor(old_dev), major(*new_dev),
				minor(*new_dev));
		}
		return (DEVFSADM_SUCCESS);
	} else {
		/*
		 *  The clone is a special case.  Look at its minor
		 *  number since it is the major number of the real driver.
		 */
		if (get_major_no(driver, &newmajor) == DEVFSADM_FAILURE) {
			return (DEVFSADM_FAILURE);
		}

		oldminor = minor(old_dev);
		if (modctl(MODGETNAME, cdriver, sizeof (cdriver),
					&oldminor) != 0) {
			err_print(MODGETNAME_FAILED, oldminor);
			return (DEVFSADM_FAILURE);
		}

		if (get_major_no(cdriver, &newminor) == DEVFSADM_FAILURE) {
			return (DEVFSADM_FAILURE);
		}

		*new_dev = makedev(newmajor, newminor);
		if (old_dev != *new_dev) {
			vprint(CHATTY_MID, "%sdriver: %s old: "
				" %lu,%lu  new: %lu,%lu\n", fcn, driver,
				major(old_dev), minor(old_dev),
				major(*new_dev), minor(*new_dev));
		}
		return (DEVFSADM_SUCCESS);
	}
}

/*
 *
 * Find the major number for driver, searching the n2m_list that was
 * built in load_n2m_table().
 */
static int
get_major_no(char *driver, major_t *major)
{
	n2m_t *ptr;

	for (ptr = n2m_list; ptr != NULL; ptr = ptr->next) {
		if (strcmp(ptr->driver, driver) == 0) {
			*major = ptr->major;
			return (DEVFSADM_SUCCESS);
		}
	}
	err_print(FIND_MAJOR_FAILED, driver);
	return (DEVFSADM_FAILURE);
}

/*
 * Loads a name_to_major table into memory.  Used only for suninstall's
 * private -R option to devfsadm, to translate major numbers from the
 * running to the installed target disk.
 */
static int
load_n2m_table(char *file)
{
	FILE *fp;
	char line[1024];
	char driver[PATH_MAX + 1];
	major_t major;
	n2m_t *ptr;
	int ln = 0;

	if ((fp = fopen(file, "r")) == NULL) {
		err_print(FOPEN_FAILED, file, strerror(errno));
		return (DEVFSADM_FAILURE);
	}

	while (fgets(line, sizeof (line), fp) != NULL) {
		ln++;
		if (line[0] == '#') {
			continue;
		}
		if (sscanf(line, "%s%lu", driver, &major) != 2) {
			err_print(IGNORING_LINE_IN, ln, file);
			continue;
		}
		ptr = (n2m_t *)s_malloc(sizeof (n2m_t));
		ptr->major = major;
		ptr->driver = s_strdup(driver);
		ptr->next = n2m_list;
		n2m_list = ptr;
	}
	if (s_fclose(fp) == EOF) {
		err_print(FCLOSE_FAILED, file, strerror(errno));
	}
	return (DEVFSADM_SUCCESS);
}

/*
 * Called at devfsadm startup to read in the devlink.tab file.	Creates
 * a linked list of devlinktab_list structures which will be
 * searched for every minor node.
 */
static void
read_devlinktab_file(void)
{
	devlinktab_list_t *headp = NULL;
	devlinktab_list_t *entryp;
	devlinktab_list_t **previous;
	devlinktab_list_t *save;
	char line[MAX_DEVLINK_LINE];
	char *selector;
	char *p_link;
	char *s_link;
	FILE *fp;
	int i;
	static struct stat cached_sb;
	struct stat current_sb;
	static int cached = FALSE;

	if (devlinktab_file == NULL) {
		return;
	}

	(void) stat(devlinktab_file, &current_sb);

	/* if already cached, check to see if it is still valid */
	if (cached == TRUE) {

		if (current_sb.st_mtime == cached_sb.st_mtime) {
			vprint(FILES_MID, "%s cache valid\n", devlinktab_file);
			return;
		}

		vprint(FILES_MID, "invalidating %s cache\n", devlinktab_file);

		while (devlinktab_list != NULL) {
			free_link_list(devlinktab_list->p_link);
			free_link_list(devlinktab_list->s_link);
			free_selector_list(devlinktab_list->selector);
			free(devlinktab_list->selector_pattern);
			free(devlinktab_list->p_link_pattern);
			if (devlinktab_list->s_link_pattern != NULL) {
				free(devlinktab_list->s_link_pattern);
			}
			save = devlinktab_list;
			devlinktab_list = devlinktab_list->next;
			free(save);
		}
	} else {
		cached = TRUE;
	}

	(void) stat(devlinktab_file, &cached_sb);

	if ((fp = fopen(devlinktab_file, "r")) == NULL) {
		err_print(FOPEN_FAILED, devlinktab_file, strerror(errno));
		return;
	}

	previous = &headp;

	while (fgets(line, sizeof (line), fp) != NULL) {
		devlinktab_line++;
		i = strlen(line);
		if (line[i-1] == NEWLINE) {
			line[i-1] = '\0';
		} else if (i == sizeof (line-1)) {
			err_print(LINE_TOO_LONG, devlinktab_line,
			    devlinktab_file, sizeof (line)-1);
			while (((i = getc(fp)) != '\n') && (i != EOF));
			continue;
		}

		if ((line[0] == '#') || (line[0] == '\0')) {
			/* Ignore comments and blank lines */
			continue;
		}

		vprint(DEVLINK_MID, "table: %s line %d: '%s'\n",
			devlinktab_file, devlinktab_line, line);

		/* break each entry into fields.  s_link may be NULL */
		if (split_devlinktab_entry(line, &selector, &p_link,
		    &s_link) == DEVFSADM_FAILURE) {
			vprint(DEVLINK_MID, "split_entry returns failure\n");
			continue;
		} else {
			vprint(DEVLINK_MID, "split_entry selector='%s' "
				"p_link='%s' s_link='%s'\n\n", selector,
				p_link, (s_link == NULL) ? "" : s_link);
		}

		entryp = (devlinktab_list_t *)
			s_malloc(sizeof (devlinktab_list_t));

		entryp->line_number = devlinktab_line;

		if ((entryp->selector =
			create_selector_list(selector)) == NULL) {
			free(entryp);
			continue;
		}
		entryp->selector_pattern = s_strdup(selector);

		if ((entryp->p_link = create_link_list(p_link)) == NULL) {
			free_selector_list(entryp->selector);
			free(entryp->selector_pattern);
			free(entryp);
			continue;
		}

		entryp->p_link_pattern = s_strdup(p_link);

		if (s_link != NULL) {
			if ((entryp->s_link =
			    create_link_list(s_link)) == NULL) {
				free_selector_list(entryp->selector);
				free_link_list(entryp->p_link);
				free(entryp->selector_pattern);
				free(entryp->p_link_pattern);
				free(entryp);
				continue;
			}
			    entryp->s_link_pattern = s_strdup(s_link);
		} else {
			entryp->s_link = NULL;
			entryp->s_link_pattern = NULL;

		}

		/* append to end of list */

		entryp->next = NULL;
		*previous = entryp;
		previous = &(entryp->next);
	}
	if (s_fclose(fp) == EOF) {
		err_print(FCLOSE_FAILED, devlinktab_file, strerror(errno));
	}
	devlinktab_list = headp;
}

/*
 *
 * For a single line entry in devlink.tab, split the line into fields
 * selector, p_link, and an optionally s_link.	If s_link field is not
 * present, then return NULL in s_link (not NULL string).
 */
static int
split_devlinktab_entry(char *entry, char **selector, char **p_link,
			char **s_link)
{
	char *tab;

	*selector = entry;

	if ((tab = strchr(entry, TAB)) != NULL) {
		*tab = '\0';
		*p_link = ++tab;
	} else {
		err_print(MISSING_TAB, devlinktab_line, devlinktab_file);
		return (DEVFSADM_FAILURE);
	}

	if (*p_link == '\0') {
		err_print(MISSING_DEVNAME, devlinktab_line, devlinktab_file);
		return (DEVFSADM_FAILURE);
	}

	if ((tab = strchr(*p_link, TAB)) != NULL) {
		*tab = '\0';
		*s_link = ++tab;
		if (strchr(*s_link, TAB) != NULL) {
			err_print(TOO_MANY_FIELDS, devlinktab_line,
					devlinktab_file);
			return (DEVFSADM_FAILURE);
		}
	} else {
		*s_link = NULL;
	}

	return (DEVFSADM_SUCCESS);
}

/*
 * For a given devfs_spec field, for each element in the field, add it to
 * a linked list of devfs_spec structures.  Return the linked list in
 * devfs_spec_list.
 */
static selector_list_t *
create_selector_list(char *selector)
{
	    char *key;
	    char *val;
	    int error = FALSE;
	    selector_list_t *head_selector_list = NULL;
	    selector_list_t *selector_list;

	    /* parse_devfs_spec splits the next field into keyword & value */
	    while ((*selector != NULL) && (error == FALSE)) {
		    if (parse_selector(&selector, &key,
				&val) == DEVFSADM_FAILURE) {
			    error = TRUE;
			    break;
		    } else {
			    selector_list = (selector_list_t *)
				    s_malloc(sizeof (selector_list_t));
			    if (strcmp(NAME_S, key) == 0) {
				    selector_list->key = NAME;
			    } else if (strcmp(TYPE_S, key) == 0) {
				    selector_list->key = TYPE;
			    } else if (strncmp(ADDR_S, key, ADDR_S_LEN) == 0) {
				    selector_list->key = ADDR;
				    if (key[ADDR_S_LEN] == '\0') {
					    selector_list->arg = 0;
				    } else if (isdigit(key[ADDR_S_LEN]) !=
						FALSE) {
					    selector_list->arg =
							atoi(&key[ADDR_S_LEN]);
				    } else {
					    error = TRUE;
					    free(selector_list);
					    err_print(BADKEYWORD, key,
						devlinktab_line,
						devlinktab_file);
					    break;
				    }
			    } else if (strncmp(MINOR_S, key,
						MINOR_S_LEN) == 0) {
				    selector_list->key = MINOR;
				    if (key[MINOR_S_LEN] == '\0') {
					    selector_list->arg = 0;
				    } else if (isdigit(key[MINOR_S_LEN]) !=
						FALSE) {
					    selector_list->arg =
						atoi(&key[MINOR_S_LEN]);
				    } else {
					    error = TRUE;
					    free(selector_list);
					    err_print(BADKEYWORD, key,
						devlinktab_line,
						devlinktab_file);
					    break;
				    }
				    vprint(DEVLINK_MID, "MINOR = %s\n", val);
			    } else {
				    err_print(UNRECOGNIZED_KEY, key,
					devlinktab_line, devlinktab_file);
				    error = TRUE;
				    free(selector_list);
				    break;
			    }
			    selector_list->val = s_strdup(val);
			    selector_list->next = head_selector_list;
			    head_selector_list = selector_list;
			    vprint(DEVLINK_MID, "key='%s' val='%s' arg=%d\n",
					key, val, selector_list->arg);
		    }
	    }

	    if ((error == FALSE) && (head_selector_list != NULL)) {
		    return (head_selector_list);
	    } else {
		    /* parse failed.  Free any alocated structs */
		    free_selector_list(head_selector_list);
		    return (NULL);
	    }
}

/*
 * Takes a semicolon separated list of selector elements and breaks up
 * into a keyword-value pair.	semicolon and equal characters are
 * replaced with NULL's.  On success, selector is updated to point to the
 * terminating NULL character terminating the keyword-value pair, and the
 * function returns DEVFSADM_SUCCESS.	If there is a syntax error,
 * devfs_spec is not modified and function returns DEVFSADM_FAILURE.
 */
static int
parse_selector(char **selector, char **key, char **val)
{
	char *equal;
	char *semi_colon;

	*key = *selector;

	if ((equal = strchr(*key, '=')) != NULL) {
		*equal = '\0';
	} else {
		err_print(MISSING_EQUAL, devlinktab_line, devlinktab_file);
		return (DEVFSADM_FAILURE);
	}

	*val = ++equal;
	if ((semi_colon = strchr(equal, ';')) != NULL) {
		*semi_colon = '\0';
		*selector = semi_colon + 1;
	} else {
		*selector = equal + strlen(equal);
	}
	return (DEVFSADM_SUCCESS);
}

/*
 * link is either the second or third field of devlink.tab.  Parse link
 * into a linked list of devlink structures and return ptr to list.  Each
 * list element is either a constant string, or one of the following
 * escape sequences: \M, \A, \N, or \D.  The first three escape sequences
 * take a numerical argument.
 */
static link_list_t *
create_link_list(char *link)
{
	int x = 0;
	int error = FALSE;
	int counter_found = FALSE;
	link_list_t *head = NULL;
	link_list_t **ptr;
	link_list_t *link_list;
	char constant[MAX_DEVLINK_LINE];
	char *error_str;

	if (link == NULL) {
		return (NULL);
	}

	while ((*link != '\0') && (error == FALSE)) {
		link_list = (link_list_t *)s_malloc(sizeof (link_list_t));
		link_list->next = NULL;

		while ((*link != '\0') && (*link != '\\')) {
			/* a non-escaped string */
			constant[x++] = *(link++);
		}
		if (x != 0) {
			constant[x] = '\0';
			link_list->type = CONSTANT;
			link_list->constant = s_strdup(constant);
			x = 0;
			vprint(DEVLINK_MID, "CONSTANT FOUND %s\n", constant);
		} else {
			switch (*(++link)) {
			case 'M':
				link_list->type = MINOR;
				break;
			case 'A':
				link_list->type = ADDR;
				break;
			case 'N':
				if (counter_found == TRUE) {
					error = TRUE;
					error_str = "multiple counters "
						"not permitted";
					free(link_list);
				} else {
					counter_found = TRUE;
					link_list->type = COUNTER;
				}
				break;
			case 'D':
				link_list->type = NAME;
				break;
			default:
				error = TRUE;
				free(link_list);
				error_str = "unrecognized escape sequence";
				break;
			}
			if (*(link++) != 'D') {
				if (isdigit(*link) == FALSE) {
					error_str = "escape sequence must be "
						"followed by a digit\n";
					error = TRUE;
					free(link_list);
				} else {
					link_list->arg =
						(int)strtoul(link, &link, 10);
					vprint(DEVLINK_MID, "link_list->arg = "
						"%d\n", link_list->arg);
				}
			}
		}
		/* append link_list struct to end of list */
		if (error == FALSE) {
			for (ptr = &head; *ptr != NULL; ptr = &((*ptr)->next));
			*ptr = link_list;
		}
	}

	if (error == FALSE) {
		return (head);
	} else {
		err_print(CONFIG_INCORRECT, devlinktab_line, devlinktab_file,
		    error_str);
		free_link_list(head);
		return (NULL);
	}
}

/*
 * Called for each minor node devfsadm processes; for each minor node,
 * look for matches in the devlinktab_list list which was created on
 * startup read_devlinktab_file().  If there is a match, call build_links()
 * to build a logical devlink and a possible extra devlink.
 */
static int
process_devlink_compat(di_minor_t minor, di_node_t node)
{
	int link_built = FALSE;
	devlinktab_list_t *entry;
	char *nodetype;

	if (devlinks_debug == TRUE) {
		nodetype =  di_minor_nodetype(minor);
		if (nodetype == NULL) {
			nodetype = "ddi_pseudo";
		}
		vprint(INFO_MID, "'%s' entry: %s:%s\n", nodetype,
			s_di_devfs_path(node, minor),
			di_minor_name(minor) ? di_minor_name(minor) :
			"");
	}


	/* don't process devlink.tab if devfsadm invoked with -c <class> */
	if (num_classes > 0) {
		return (FALSE);
	}

	for (entry = devlinktab_list; entry != NULL; entry = entry->next) {
		if (devlink_matches(entry, minor, node) == DEVFSADM_SUCCESS) {
			link_built = TRUE;
			(void) build_links(entry, minor, node);
		}
	}
	return (link_built);
}

/*
 * For a given devlink.tab devlinktab_list entry, see if the selector
 * field matches this minor node.  If it does, return DEVFSADM_SUCCESS,
 * otherwise DEVFSADM_FAILURE.
 */
static int
devlink_matches(devlinktab_list_t *entry, di_minor_t minor, di_node_t node)
{
	selector_list_t *selector = entry->selector;
	char *addr;
	char *minor_name;
	char *node_type;

	for (; selector != NULL; selector = selector->next) {
		switch (selector->key) {
		case NAME:
			if (strcmp(di_node_name(node), selector->val) != 0) {
				return (DEVFSADM_FAILURE);
			}
			break;
		case TYPE:
			if ((node_type = di_minor_nodetype(minor)) == NULL) {
				node_type = "ddi_pseudo";
			}
			if (strcmp(node_type, selector->val) != 0) {
				return (DEVFSADM_FAILURE);
			}
			break;
		case ADDR:
			if ((addr = di_bus_addr(node)) == NULL) {
				return (DEVFSADM_FAILURE);
			}
			if (selector->arg == 0) {
				if (strcmp(addr, selector->val) != 0) {
					return (DEVFSADM_FAILURE);
				}
			} else {
				if (compare_field(addr, selector->val,
				    selector->arg) == DEVFSADM_FAILURE) {
					return (DEVFSADM_FAILURE);
				}
			}
			break;
		case MINOR:
			if ((minor_name = di_minor_name(minor)) == NULL) {
				return (DEVFSADM_FAILURE);
			}
			if (selector->arg == 0) {
				if (strcmp(minor_name, selector->val) != 0) {
					return (DEVFSADM_FAILURE);
				}
			} else {
				if (compare_field(minor_name, selector->val,
					selector->arg) == DEVFSADM_FAILURE) {
					return (DEVFSADM_FAILURE);
				}
			}
			break;
		default:
			return (DEVFSADM_FAILURE);
		}
	}

	return (DEVFSADM_SUCCESS);
}

/*
 * For the given minor node and devlinktab_list entry from devlink.tab,
 * build a logical dev link and a possible extra devlink.
 * Return DEVFSADM_SUCCESS if link is created, otherwise DEVFSADM_FAILURE.
 */
static int
build_links(devlinktab_list_t *entry, di_minor_t minor, di_node_t node)
{
	char secondary_link[PATH_MAX + 1];
	char primary_link[PATH_MAX + 1];
	char contents[PATH_MAX + 1];

	(void) strcpy(contents, s_di_devfs_path(node, minor));
	(void) strcat(contents, ":");
	(void) strcat(contents, di_minor_name(minor));

	if (construct_devlink(primary_link, entry->p_link, contents,
				minor, node,
			    entry->p_link_pattern) == DEVFSADM_FAILURE) {
		return (DEVFSADM_FAILURE);
	}
	(void) devfsadm_mklink(primary_link, node, minor, 0);

	if (entry->s_link == NULL) {
		return (DEVFSADM_SUCCESS);
	}

	if (construct_devlink(secondary_link, entry->s_link,
			primary_link, minor, node,
				entry->s_link_pattern) == DEVFSADM_FAILURE) {
		return (DEVFSADM_FAILURE);
	}

	(void) devfsadm_secondary_link(secondary_link, primary_link, 0);

	return (DEVFSADM_SUCCESS);
}

static int
construct_devlink(char *link, link_list_t *link_build, char *contents,
			di_minor_t minor, di_node_t node, char *pattern)
{
	int counter_offset = -1;
	devfsadm_enumerate_t rules[1] = {NULL};
	char templink[PATH_MAX + 1];
	char *buff;
	char start[10];
	char *node_path;

	link[0] = '\0';

	for (; link_build != NULL; link_build = link_build->next) {
		switch (link_build->type) {
		case NAME:
			(void) strcat(link, di_node_name(node));
			break;
		case CONSTANT:
			(void) strcat(link, link_build->constant);
			break;
		case ADDR:
			if (component_cat(link, di_bus_addr(node),
				    link_build->arg) == DEVFSADM_FAILURE) {
				node_path = di_devfs_path(node);
				err_print(CANNOT_BE_USED, pattern, node_path,
					    di_minor_name(minor));
				di_devfs_path_free(node_path);
				return (DEVFSADM_FAILURE);
			}
			break;
		case MINOR:
			if (component_cat(link, di_minor_name(minor),
				    link_build->arg) == DEVFSADM_FAILURE) {
				node_path = di_devfs_path(node);
				err_print(CANNOT_BE_USED, pattern, node_path,
					    di_minor_name(minor));
				di_devfs_path_free(node_path);
				return (DEVFSADM_FAILURE);
			}
			break;
		case COUNTER:
			counter_offset = strlen(link);
			(void) strcat(link, "([0-9]+)");
			(void) sprintf(start, "%d", link_build->arg);
			break;
		default:
			return (DEVFSADM_FAILURE);
		}
	}

	if (counter_offset != -1) {
		/*
		 * copy anything appended after "([0-9]+)" into
		 * templink
		 */

		(void) strcpy(templink,
			    &link[counter_offset + strlen("([0-9]+)")]);
		rules[0].re = link;
		rules[0].subexp = 1;
		rules[0].flags = MATCH_ALL;
		if (devfsadm_enumerate_int_start(contents, 0, &buff,
		    rules, 1, start) == DEVFSADM_FAILURE) {
			return (DEVFSADM_FAILURE);
		}
		(void) strcpy(&link[counter_offset], buff);
		free(buff);
		(void) strcat(link, templink);
		vprint(DEVLINK_MID, "COUNTER is	%s\n", link);
	}
	return (DEVFSADM_SUCCESS);
}

/*
 * Compares "field" number of the comma separated list "full_name" with
 * field_item.	Returns DEVFSADM_SUCCESS for match,
 * DEVFSADM_FAILURE for no match.
 */
static int
compare_field(char *full_name, char *field_item, int field)
{
	--field;
	while ((*full_name != '\0') && (field != 0)) {
		if (*(full_name++) == ',') {
			field--;
		}
	}

	if (field != 0) {
		return (DEVFSADM_FAILURE);
	}

	while ((*full_name != '\0') && (*field_item != '\0') &&
			(*full_name != ',')) {
		if (*(full_name++) != *(field_item++)) {
			return (DEVFSADM_FAILURE);
		}
	}

	if (*field_item != '\0') {
		return (DEVFSADM_FAILURE);
	}

	if ((*full_name == '\0') || (*full_name == ','))
		return (DEVFSADM_SUCCESS);

	return (DEVFSADM_FAILURE);
}

/*
 * strcat() field # "field" of comma separated list "name" to "link".
 * Field 0 is the entire name.
 * Return DEVFSADM_SUCCESS or DEVFSADM_FAILURE.
 */
static int
component_cat(char *link, char *name, int field)
{

	if (name == NULL) {
		return (DEVFSADM_FAILURE);
	}

	if (field == 0) {
		(void) strcat(link, name);
		return (DEVFSADM_SUCCESS);
	}

	while (*link != '\0') {
		link++;
	}

	--field;
	while ((*name != '\0') && (field != 0)) {
		if (*(name++) == ',') {
			--field;
		}
	}

	if (field != 0) {
		return (DEVFSADM_FAILURE);
	}

	while ((*name != '\0') && (*name != ',')) {
		*(link++) = *(name++);
	}

	*link = '\0';
	return (DEVFSADM_SUCCESS);
}

static void
free_selector_list(selector_list_t *head)
{
	selector_list_t *temp;

	while (head != NULL) {
		temp = head;
		head = head->next;
		free(temp->val);
		free(temp);
	}
}

static void
free_link_list(link_list_t *head)
{
	link_list_t *temp;

	while (head != NULL) {
		temp = head;
		head = head->next;
		if (temp->type == CONSTANT) {
			free(temp->constant);
		}
		free(temp);
	}
}

/*
 * Prints only if level matches one of the debug levels
 * given on command line.  INFO_MID is always printed.
 *
 * See devfsadm.h for a listing of globally defined levels and
 * meanings.  Modules should prefix the level with their
 * module name to prevent collissions.
 */
void
devfsadm_print(char *msgid, char *message, ...)
{
	va_list ap;
	static int newline = TRUE;
	int x;

	if (msgid != NULL) {
		for (x = 0; x < num_verbose; x++) {
			if (strcmp(verbose[x], msgid) == 0) {
				break;
			}
			if (strcmp(verbose[x], ALL_MID) == 0) {
				break;
			}
		}
		if (x == num_verbose) {
			return;
		}
	}

	va_start(ap, message);

	if (msgid == NULL) {
		if (logflag == TRUE) {
			(void) vsyslog(LOG_NOTICE, message, ap);
		} else {
			(void) vfprintf(stdout, message, ap);
		}

	} else {
		if (logflag == TRUE) {
			(void) syslog(LOG_DEBUG, "%s[%ld]: %s: ",
				    prog, getpid(), msgid);
			(void) vsyslog(LOG_DEBUG, message, ap);
		} else {
			if (newline == TRUE) {
				(void) fprintf(stdout, "%s[%ld]: %s: ",
					prog, getpid(), msgid);
			}
			(void) vfprintf(stdout, message, ap);
		}
	}

	if (message[strlen(message) - 1] == '\n') {
		newline = TRUE;
	} else {
		newline = FALSE;
	}
	va_end(ap);
}

/*
 * print error messages to the terminal or to syslog
 */
void
devfsadm_errprint(char *message, ...)
{
	va_list ap;

	va_start(ap, message);

	if (logflag == TRUE) {
		(void) vsyslog(LOG_ERR, message, ap);
	} else {
		(void) fprintf(stderr, "%s: ", prog);
		(void) vfprintf(stderr, message, ap);
	}
	va_end(ap);
}

/*
 * return noupdate state (-s)
 */
int
devfsadm_noupdate(void)
{
	return (file_mods == TRUE ? DEVFSADM_TRUE : DEVFSADM_FALSE);
}

/*
 * return current root update path (-r)
 */
const char *
devfsadm_root_path(void)
{
	if (root_dir[0] == '\0') {
		return ("/");
	} else {
		return ((const char *)root_dir);
	}
}

/* common exit function which ensures releasing locks */
static void
devfsadm_exit(int status)
{
	if (DEVFSADM_DEBUG_ON) {
		vprint(INFO_MID, "exit status = %d\n", status);
	}

	exit_dev_lock();

	exit_daemon_lock();

	if (logflag == TRUE) {
		closelog();
	}

	exit(status);
}

/*
 * When SIGHUP/INT is received, clean up and exit
 */
static void
catch_sigs(void)
{
	vprint(VERBOSE_MID, SIGNAL_RECEIVED);

	if (sema_trywait(&sema_minor) == 0) {
		unload_modules();
	}

	devfsadm_exit(23);
}

/*
 * set root_dir, devices_dir, dev_dir using optarg
 */
static void
set_root_devices_dev_dir(char *dir)
{
	root_dir = s_malloc(strlen(dir) + 1);
	(void) strcpy(root_dir, dir);
	devices_dir  = s_malloc(strlen(dir) + strlen(DEVICES) + 1);
	(void) sprintf(devices_dir, "%s%s", root_dir, DEVICES);
	dev_dir = s_malloc(strlen(root_dir) + strlen(DEV) + 1);
	(void) sprintf(dev_dir, "%s%s", root_dir, DEV);
}

/*
 * Removes quotes.
 */
static char *
dequote(char *src)
{
	char	*dst;
	int	len;

	len = strlen(src);
	dst = s_malloc(len + 1);
	if (src[0] == '\"' && src[len - 1] == '\"') {
		len -= 2;
		(void) strncpy(dst, &src[1], len);
		dst[len] = '\0';
	} else {
		(void) strcpy(dst, src);
	}
	return (dst);
}

/*
 * For a given physical device pathname and spectype, return the
 * ownwership and permissions attributes by looking in data from
 * /etc/minor_perm.  If currently in installation mode, check for
 * possible major number translations from the miniroot to the installed
 * root's name_to_major table. Note that there can be multiple matches,
 * but the last match takes effect.  pts seems to rely on this
 * implementation behavior.
 */
static void
getattr(char *phy_path, int spectype, dev_t dev, mode_t *mode,
	uid_t *uid, gid_t *gid)
{
	char devname[PATH_MAX + 1];
	char *node_name;
	char *minor_name;
	int match = FALSE;
	int is_clone;
	int mp_drvname_matches_node_name;
	int mp_drvname_matches_minor_name;
	int mp_drvname_is_clone;
	int mp_drvname_matches_drvname;
	struct mperm *mp;
	major_t major_no;
	char driver[PATH_MAX + 1];

	/*
	 * Get the driver name based on the major number since the name
	 * in /devices may be generic.  Could be running with more major
	 * numbers than are in /etc/name_to_major, so get it from the kernel
	 */
	major_no = major(dev);

	if (modctl(MODGETNAME, driver, sizeof (driver), &major_no) != 0) {
		/* return default values */
		goto use_defaults;
	}

	(void) strcpy(devname, phy_path);

	node_name = strrchr(devname, '/'); /* node name is the last */
					/* component */
	if (node_name == NULL) {
		err_print(NO_NODE, devname);
		goto use_defaults;
	}

	minor_name = strchr(++node_name, '@'); /* see if it has address part */

	if (minor_name != NULL) {
		*minor_name++ = '\0';
	} else {
		minor_name = node_name;
	}

	minor_name = strchr(minor_name, ':'); /* look for minor name */

	if (minor_name == NULL) {
		err_print(NO_MINOR, devname);
		goto use_defaults;
	}
	*minor_name++ = '\0';

	/*
	 * mp->mp_drvname = device name from minor_perm
	 * mp->mp_minorname = minor part of device name from
	 * minor_perm
	 * drvname = name of driver for this device
	 */

	is_clone = (strcmp(node_name, "clone") == 0 ? TRUE : FALSE);

	for (mp = minor_perms; mp != NULL; mp = mp->mp_next) {
		mp_drvname_matches_node_name =
			(strcmp(mp->mp_drvname, node_name) == 0 ? TRUE : FALSE);
		mp_drvname_matches_minor_name =
			(strcmp(mp->mp_drvname, minor_name) == 0  ? TRUE:FALSE);
		mp_drvname_is_clone =
			(strcmp(mp->mp_drvname, "clone") == 0  ? TRUE : FALSE);
		mp_drvname_matches_drvname =
			(strcmp(mp->mp_drvname, driver) == 0  ? TRUE : FALSE);

		/*
		 * If one of the following cases is true, then we try to change
		 * the permissions if a "shell global pattern match" of
		 * mp_>mp_minorname matches minor_name.
		 *
		 * 1.  mp->mp_drvname matches driver.
		 *
		 * OR
		 *
		 * 2.  mp->mp_drvname matches node_name and this
		 *	name is an alias of the driver name
		 *
		 * OR
		 *
		 * 3.  /devices entry is the clone device and either
		 *	minor_perm entry is the clone device or matches
		 *	the minor part of the clone device.
		 */

		if ((mp_drvname_matches_drvname == TRUE)||

		    ((mp_drvname_matches_node_name == TRUE) &&
		    (alias(driver, node_name) == TRUE)) ||

		    ((is_clone == TRUE) &&
		    ((mp_drvname_is_clone == TRUE) ||
			(mp_drvname_matches_minor_name == TRUE)))) {
			/*
			 * Check that the minor part of the
			 * device name from the minor_perm
			 * entry matches and if so, set the
			 * permissions.
			 */
			if (gmatch(minor_name, mp->mp_minorname) != 0) {
				*uid = mp->mp_uid;
				*gid = mp->mp_gid;
				*mode = spectype | mp->mp_perm;
				match = TRUE;
			}
		}
	}

	if (match == TRUE) {
		return;
	}

	use_defaults:
	/* not found in minor_perm, so just use default values */
	*uid = root_uid;
	*gid = sys_gid;
	*mode = (spectype | 0600);
}

static void
read_minor_perm_file(void)
{
	FILE *pfd;
	struct mperm *mp;
	struct mperm *save;
	char line[MAX_PERM_LINE];
	char *cp;
	char *p;
	char t;
	struct mperm *mptail = minor_perms;
	struct passwd *pw;
	struct group *gp;
	int ln = 0;
	static int cached = FALSE;
	static struct stat cached_sb;
	struct stat current_sb;

	(void) stat(PERMFILE, &current_sb);

	/* If already cached, check to see if it is still valid */
	if (cached == TRUE) {

		if (current_sb.st_mtime == cached_sb.st_mtime) {
			vprint(FILES_MID, "%s cache valid\n", PERMFILE);
			return;
		}
		vprint(FILES_MID, "invalidating %s cache\n", PERMFILE);
		while (minor_perms != NULL) {

			if (minor_perms->mp_drvname != NULL) {
				free(minor_perms->mp_drvname);
			}
			if (minor_perms->mp_minorname != NULL) {
				free(minor_perms->mp_minorname);
			}
			if (minor_perms->mp_owner != NULL) {
				free(minor_perms->mp_owner);
			}
			if (minor_perms->mp_group != NULL) {
				free(minor_perms->mp_group);
			}
			save = minor_perms;
			minor_perms = minor_perms->mp_next;
			free(save);
		}
	} else {
		cached = TRUE;
	}

	(void) stat(PERMFILE, &cached_sb);

	vprint(FILES_MID, "loading binding file: %s\n", PERMFILE);

	if ((pfd = fopen(PERMFILE, "r")) == NULL) {
		err_print(FOPEN_FAILED, PERMFILE, strerror(errno));
		devfsadm_exit(24);
	}
	while (fgets(line, MAX_PERM_LINE - 1, pfd) != NULL) {
		ln++;
		mp = (struct mperm *)s_zalloc(sizeof (struct mperm));
		cp = line;
		if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
			err_print(IGNORING_LINE_IN, ln, PERMFILE);
			free(mp);
			continue;
		}
		mp->mp_drvname = s_strdup(p);
		if (t == '\n' || t == '\0') {
			err_print(IGNORING_LINE_IN, ln, PERMFILE);
			free(mp->mp_drvname);
			free(mp);
			continue;
		}
		if (t == ':') {
			if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
				err_print(IGNORING_LINE_IN, ln, PERMFILE);
				free(mp->mp_drvname);
				free(mp);
			}
			mp->mp_minorname = s_strdup(p);
		} else {
			mp->mp_minorname = NULL;
		}

		if (t == '\n' || t == '\0') {
			free(mp->mp_drvname);
			if (mp->mp_minorname != NULL) {
				free(mp->mp_minorname);
			}
			free(mp);
			err_print(IGNORING_LINE_IN, ln, PERMFILE);
			continue;
		}
		if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
			goto link;
		}
		if (getvalue(p, &mp->mp_perm) == DEVFSADM_FAILURE) {
			goto link;
		}
		if (t == '\n' || t == '\0') {	/* no owner or group */
			goto link;
		}
		if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
			goto link;
		}
		mp->mp_owner = s_strdup(p);
		if (t == '\n' || t == '\0') {	/* no group */
			goto link;
		}
		if (getnexttoken(cp, &cp, &p, 0) == DEVFSADM_FAILURE) {
			goto link;
		}
		mp->mp_group = s_strdup(p);
link:
		if (minor_perms == NULL) {
			minor_perms = mp;
		} else {
			mptail->mp_next = mp;
		}
		mptail = mp;

		/*
		 * Compute the uid's and gid's here - there are
		 * fewer lines in the /etc/minor_perm file than there
		 * are devices to be stat(2)ed.  And almost every
		 * device is 'root sys'.  See 1135520.
		 */
		if (mp->mp_owner == NULL ||
		    strcmp(mp->mp_owner, DEFAULT_USER) == 0 ||
		    (pw = getpwnam(mp->mp_owner)) == NULL) {
			mp->mp_uid = root_uid;
		} else {
			mp->mp_uid = pw->pw_uid;
		}

		if (mp->mp_group == NULL ||
		    strcmp(mp->mp_group, DEFAULT_GROUP) == 0 ||
		    (gp = getgrnam(mp->mp_group)) == NULL) {
			mp->mp_gid = sys_gid;
		} else {
			mp->mp_gid = gp->gr_gid;
		}
	}

	if (s_fclose(pfd) == EOF) {
		err_print(FCLOSE_FAILED, PERMFILE, strerror(errno));
	}
}

/*
 * Tokens are separated by ' ', '\t', ':', '=', '&', '|', ';', '\n', or '\0'
 *
 * Returns DEVFSADM_SUCCESS if token found, DEVFSADM_FAILURE otherwise.
 */
static int
getnexttoken(char *next, char **nextp, char **tokenpp, char *tchar)
{
	register char *cp;
	register char *cp1;
	char *tokenp;

	cp = next;
	while (*cp == ' ' || *cp == '\t') {
		cp++;			/* skip leading spaces */
	}
	tokenp = cp;			/* start of token */
	while (*cp != '\0' && *cp != '\n' && *cp != ' ' && *cp != '\t' &&
		*cp != ':' && *cp != '=' && *cp != '&' &&
		*cp != '|' && *cp != ';') {
		cp++;			/* point to next character */
	}
	/*
	 * If terminating character is a space or tab, look ahead to see if
	 * there's another terminator that's not a space or a tab.
	 * (This code handles trailing spaces.)
	 */
	if (*cp == ' ' || *cp == '\t') {
		cp1 = cp;
		while (*++cp1 == ' ' || *cp1 == '\t')
			;
		if (*cp1 == '=' || *cp1 == ':' || *cp1 == '&' || *cp1 == '|' ||
			*cp1 == ';' || *cp1 == '\n' || *cp1 == '\0') {
			*cp = NULL;	/* terminate token */
			cp = cp1;
		}
	}
	if (tchar != NULL) {
		*tchar = *cp;		/* save terminating character */
		if (*tchar == '\0') {
			*tchar = '\n';
		}
	}
	*cp++ = '\0';			/* terminate token, point to next */
	*nextp = cp;			/* set pointer to next character */
	if (cp - tokenp - 1 == 0) {
		return (DEVFSADM_FAILURE);
	}
	*tokenpp = tokenp;
	return (DEVFSADM_SUCCESS);
}

/*
 * get a decimal octal or hex number. Handle '~' for one's complement.
 *
 * Returns DEVFSADM_SUCCESS if token found, DEVFSADM_FAILURE otherwise.
 */
static int
getvalue(char *token, int *valuep)
{
	int radix;
	int retval = 0;
	int onescompl = 0;
	int negate = 0;
	char c;

	if (*token == '~') {
		onescompl++; /* perform one's complement on result */
		token++;
	} else if (*token == '-') {
		negate++;
		token++;
	}
	if (*token == '0') {
		token++;
		c = *token;

		if (c == '\0') {
			*valuep = 0;	/* value is 0 */
			return (0);
		}

		if (c == 'x' || c == 'X') {
			radix = 16;
			token++;
		} else {
			radix = 8;
		}
	} else
		radix = 10;

	while ((c = *token++)) {
		switch (radix) {
		case 8:
			if (c >= '0' && c <= '7') {
				c -= '0';
			} else {
				/* invalid number */
				return (DEVFSADM_FAILURE);
			}
			retval = (retval << 3) + c;
			break;
		case 10:
			if (c >= '0' && c <= '9') {
				c -= '0';
			} else {
				/* invalid number */
				return (DEVFSADM_FAILURE);
			}
			retval = (retval * 10) + c;
			break;
		case 16:
			if (c >= 'a' && c <= 'f') {
				c = c - 'a' + 10;
			} else if (c >= 'A' && c <= 'F') {
				c = c - 'A' + 10;
			} else if (c >= '0' && c <= '9') {
				c -= '0';
			} else {
				/* invalid number */
				return (DEVFSADM_FAILURE);
			}
			retval = (retval << 4) + c;
			break;
		}
	}
	if (onescompl) {
		retval = ~retval;
	}
	if (negate) {
		retval = -retval;
	}
	*valuep = retval;
	return (DEVFSADM_SUCCESS);
}

/*
 * read or reread the driver aliases file
 */
static void
read_driver_aliases_file(void)
{

	driver_alias_t *save;
	driver_alias_t *lst_tail;
	driver_alias_t *ap;
	static int cached = FALSE;
	FILE *afd;
	char line[256];
	char *cp;
	char *p;
	char t;
	int ln = 0;
	static struct stat cached_sb;
	struct stat current_sb;

	(void) stat(ALIASFILE, &current_sb);

	/* If already cached, check to see if it is still valid */
	if (cached == TRUE) {

		if (current_sb.st_mtime == cached_sb.st_mtime) {
			vprint(FILES_MID, "%s cache valid\n", ALIASFILE);
			return;
		}

		vprint(FILES_MID, "invalidating %s cache\n", ALIASFILE);
		while (driver_aliases != NULL) {
			free(driver_aliases->alias_name);
			free(driver_aliases->driver_name);
			save = driver_aliases;
			driver_aliases = driver_aliases->next;
			free(save);
		}
	} else {
		cached = TRUE;
	}

	(void) stat(ALIASFILE, &cached_sb);

	vprint(FILES_MID, "loading binding file: %s\n", ALIASFILE);

	if ((afd = fopen(ALIASFILE, "r")) == NULL) {
		err_print(FOPEN_FAILED, ALIASFILE, strerror(errno));
		devfsadm_exit(25);
	}

	while (fgets(line, sizeof (line) - 1, afd) != NULL) {
		ln++;
		cp = line;
		if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
			err_print(IGNORING_LINE_IN, ln, ALIASFILE);
			continue;
		}
		if (t == '\n' || t == '\0') {
			err_print(DRV_BUT_NO_ALIAS, ln, ALIASFILE);
			continue;
		}
		ap = (struct driver_alias *)
				s_zalloc(sizeof (struct driver_alias));
		ap->driver_name = s_strdup(p);
		if (getnexttoken(cp, &cp, &p, &t) == DEVFSADM_FAILURE) {
			err_print(DRV_BUT_NO_ALIAS, ln, ALIASFILE);
			free(ap->driver_name);
			free(ap);
			continue;
		}
		if (*p == '"') {
			if (p[strlen(p) - 1] == '"') {
				p[strlen(p) - 1] = '\0';
				p++;
			}
		}
		ap->alias_name = s_strdup(p);
		if (driver_aliases == NULL) {
			driver_aliases = ap;
			lst_tail = ap;
		} else {
			lst_tail->next = ap;
			lst_tail = ap;
		}
	}
	if (s_fclose(afd) == EOF) {
		err_print(FCLOSE_FAILED, ALIASFILE, strerror(errno));
	}
}

/*
 * return TRUE if alias_name is an alias for driver_name, otherwise
 * return FALSE.
 */
static int
alias(char *driver_name, char *alias_name)
{
	driver_alias_t *alias;

	/*
	 * check for a match
	 */
	for (alias = driver_aliases; alias != NULL; alias = alias->next) {
		if ((strcmp(alias->driver_name, driver_name) == 0) &&
		    (strcmp(alias->alias_name, alias_name) == 0)) {
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
 * convenience functions
 */
static void *
s_malloc(const size_t size)
{
	void *rp;

	rp = malloc(size);
	if (rp == NULL) {
		err_print(MALLOC_FAILED, size);
		devfsadm_exit(26);
	}
	return (rp);
}

/*
 * convenience functions
 */
static void *
s_realloc(void *ptr, const size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL) {
		err_print(REALLOC_FAILED, size);
		devfsadm_exit(27);
	}
	return (ptr);
}

static void *
s_zalloc(const size_t size)
{
	void *rp;

	rp = calloc(1, size);
	if (rp == NULL) {
		err_print(CALLOC_FAILED, size);
		devfsadm_exit(28);
	}
	return (rp);
}

static char *
s_strdup(const char *ptr)
{
	void *rp;

	rp = strdup(ptr);
	if (rp == NULL) {
		err_print(STRDUP_FAILED, ptr);
		devfsadm_exit(29);
	}
	return (rp);
}

static void
s_closedir(DIR *dirp)
{
retry:
	if (closedir(dirp) != 0) {
		if (errno == EINTR)
			goto retry;
		err_print(CLOSEDIR_FAILED, strerror(errno));
	}
}

static void
s_mkdirp(const char *path, const mode_t mode)
{
	vprint(CHATTY_MID, "mkdirp(%s, 0x%x)\n", path, mode);
	if (mkdirp(path, mode) == -1) {
		if (errno != EEXIST) {
			err_print(MKDIR_FAILED, path, mode, strerror(errno));
		}
	}
}

static int
s_fclose(FILE *fp)
{
	int	rval;

retry:
	if ((rval = fclose(fp)) == EOF) {
		if (errno == EINTR)
			goto retry;
	}

	return (rval);
}

static void
s_unlink(const char *file)
{
retry:
	if (unlink(file) == -1) {
		if (errno == EINTR || errno == EAGAIN)
			goto retry;
		if (errno != ENOENT) {
			err_print(UNLINK_FAILED, file, strerror(errno));
		}
	}
}

static char *
s_di_devfs_path(di_node_t node, di_minor_t minor)
{
	char *rv;
	static char rcontents[PATH_MAX + 1];

	/*
	 * Clustering: Need to create the /dev link with the appropriate
	 * cluster path prefix.
	 */

	rcontents[0] = '\0';

	switch (di_minor_class(minor)) {
	case GLOBAL_DEV:
	case NODEBOUND_DEV:
	case NODESPECIFIC_DEV:
		break;
	default:
		/* An error, but fall through to ENUMERATED for now. */
	case ENUMERATED_DEV:
		(void) strcat(rcontents, get_dpath_prefix());
		break;
	}

	if ((rv = di_devfs_path(node)) != NULL) {
		strcat(rcontents, rv);
		di_devfs_path_free(rv);
		return (rcontents);
	}
	err_print(DI_DEVFS_PATH_FAILED, strerror(errno));
	devfsadm_exit(30);
	/*NOTREACHED*/
}

char *
get_dpath_prefix()
{
	unsigned int nid;
	static char path_prefix[32];

	if (_cladm(CL_CONFIG, CL_NODEID, &nid) != 0) {
		path_prefix[0] = '\0'; /* not a cluster */
	} else {
		(void) sprintf(path_prefix, "/node@%d", nid);
	}

	return (path_prefix);
}

static void
add_verbose_id(char *mid)
{
	num_verbose++;
	verbose = s_realloc(verbose, num_verbose * sizeof (char *));
	verbose[num_verbose - 1] = mid;
}

/*
 * returns DEVFSADM_TRUE if contents in a minor node in /devices.
 * If mn_root is not NULL, mn_root is set to:
 *	if contents is a /dev node, mn_root = contents
 * 			OR
 *	if contents is a /devices node, mn_root set to the '/'
 *	following /devices.
 */
static int
is_minor_node(char *contents, char **mn_root)
{
	char *ptr;
	char device_prefix[100];

	sprintf(device_prefix, "../devices%s", get_dpath_prefix());

	if ((ptr = strstr(contents, device_prefix)) != NULL) {
		if (mn_root != NULL) {
			/* mn_root should point to the / following /devices */
			*mn_root = ptr += strlen(device_prefix);
		}
		return (DEVFSADM_TRUE);
	}

	sprintf(device_prefix, "/devices%s/", get_dpath_prefix());

	if (strncmp(contents, device_prefix, strlen(device_prefix)) == 0) {
		if (mn_root != NULL) {
			/* mn_root should point to the / following /devices */
			*mn_root = contents + strlen(device_prefix) - 1;
		}
		return (DEVFSADM_TRUE);
	}

	if (mn_root != NULL) {
		*mn_root = contents;
	}
	return (DEVFSADM_FALSE);
}
