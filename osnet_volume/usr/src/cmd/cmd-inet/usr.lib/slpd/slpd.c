/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)slpd.c	1.3	99/04/23 SMI"

/*
 * This is a proxy daemon for the real Java slpd. This deamon starts
 * at boot time, and listens for any incoming SLP messages only on
 * loopback -- this way, only local processes can start the real
 * daemon. When a message comes in, the proxy daemon dups the message
 * fds onto fds 0, 1, and 2, and execs the real Java slpd. The purpose
 * of this approach is for performance: boot time performance is
 * not degraded by cranking off the (huge) JVM, and systems take
 * the JVM resource hit only if they actually use SLP.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <unistd.h>
#include <slp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/systeminfo.h>

/* This is an index which points into the args array at the conf file arg */
#define	CONF_INDEX	6

/* The actual JVM will be at JAVA_HOME/{arch}/native_threads/java */
#define	JAVA_HOME	"/usr/java1.2/jre/bin/"
#define	JVM_LOC		"/native_threads/java"

static char *slpd_args[] = {
	0,		/* will be filled in later */
	"-Xmx128m",
	"-classpath",
	"/usr/share/lib/slp/slpd.jar:/usr/share/lib/ami/ami.jar",
	"com.sun.slp.slpd",
	"-f",
	"/etc/inet/slp.conf",
	0
};

/*
 * These are global so they can be easily accessed from a signal
 * handler for cleanup.
 */

static void run_slpd() {
	/* Set up the LD path so Java can find slpd's JNI methods */
	(void) putenv("LD_LIBRARY_PATH=/usr/lib");
	closelog();

	if (execv(*slpd_args, slpd_args) == -1) {
	    openlog("slpd", LOG_PID, LOG_DAEMON);
	    syslog(LOG_ERR, "execv failed: %s", strerror(errno));
	    closelog();
	}
}

/*
 * If an alternate config file was specified with -f, make sure slpd
 * uses that config file. Also, force libslp.so to use that new config
 * file when checking to see if slpd is a DA. If any other arguments
 * are given, they are ignored.
 */
static void do_args(int argc, char *const *argv) {
	int c;
	char *conf = NULL;
	char arch[32];
	long archlen;
	char *java_cmd;

	while ((c = getopt(argc, argv, "f:")) != EOF) {
	    switch (c) {
	    case 'f':
		conf = optarg;
		break;
	    default:
		break;
	    }
	}

	if (conf) {
	    char *conf_env;
	    if (!(conf_env = malloc(
		strlen("SLP_CONF_FILE=") + strlen(conf) + 1))) {
		syslog(LOG_ERR, "no memory");
		exit(1);
	    }
	    (void) strcpy(conf_env, "SLP_CONF_FILE=");
	    (void) strcat(conf_env, conf);

	    (void) putenv(conf_env);

	    slpd_args[CONF_INDEX] = conf;
	}

	/* create the Java command string */
	archlen = sysinfo(SI_ARCHITECTURE, arch, 32);
	java_cmd = malloc(archlen + strlen(JAVA_HOME) + strlen(JVM_LOC));
	if (!java_cmd) {
	    syslog(LOG_ERR, "no memory");
	    exit(1);
	}

	(void) strcpy(java_cmd, JAVA_HOME);
	(void) strcat(java_cmd, arch);
	(void) strcat(java_cmd, JVM_LOC);

	slpd_args[0] = java_cmd;
}

static void detachfromtty() {
	register int i;
	struct rlimit rl;

	switch (fork()) {
	case -1:
	    perror("slpd: can not fork");
	    exit(1);
	    /*NOTREACHED*/
	case 0:
	    break;
	default:
	    exit(0);
	}

	/*
	 * Close existing file descriptors, open "/dev/null" as
	 * standard input, output, and error, and detach from
	 * controlling terminal.
	 */
	(void) getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++)
	    (void) close(i);
	(void) open("/dev/null", O_RDONLY);
	(void) open("/dev/null", O_WRONLY);
	(void) dup(1);
	(void) setsid();
}

static void cleanup_and_exit(int retval) {
	closelog();
	exit(retval);
}

int main(int argc, char *const *argv) {
	struct sockaddr_in bindaddr;
	socklen_t addrlen;
	const char *isDA;
	const char *proxyReg;
	int connfd;
	int lfd;
	const int on = 1;

	detachfromtty();

	openlog("slpd", LOG_PID, LOG_DAEMON);

	do_args(argc, argv);

	/* If slpd has been configured to run as a DA, start it and exit */
	isDA = SLPGetProperty("net.slp.isDA");
	proxyReg = SLPGetProperty("net.slp.serializedRegURL");
	if ((isDA && (strcasecmp(isDA, "true") == 0)) || proxyReg) {
	    run_slpd();
	    return (1);
	}

	if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    syslog(LOG_ERR, "socket failed: %s", strerror(errno));
	    cleanup_and_exit(1);
	}

	(void) setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

	(void) memset((void *)&bindaddr, 0, sizeof (bindaddr));
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bindaddr.sin_port = htons(427);

	if (bind(lfd, (const struct sockaddr *)&bindaddr, sizeof (bindaddr))
	    < 0) {
	    syslog(LOG_ERR, "bind failed: %s", strerror(errno));
	    cleanup_and_exit(1);
	}

	if (listen(lfd, 1) < 0) {
	    syslog(LOG_ERR, "listen failed: %s", strerror(errno));
	    cleanup_and_exit(1);
	}

	addrlen = sizeof (bindaddr);
	if ((connfd = accept(lfd, (struct sockaddr *)&bindaddr, &addrlen))
	    < 0) {
	    syslog(LOG_ERR, "accept failed: %s", strerror(errno));
	    cleanup_and_exit(1);
	}

	(void) close(lfd);

	(void) dup2(connfd, 0);
	(void) close(connfd);
	(void) dup2(0, 1);
	(void) dup2(0, 2);

	run_slpd();

	return (1);
}
