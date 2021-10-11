/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.24	99/08/09 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>
#include <sys/systeminfo.h>

#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <libintl.h>

#include <adaptor.h>


#define	ACK(fp)		{ (void) fputc(NULL, fp); (void) fflush(fp); }
#define	NACK(fp)	{ (void) fputc('\001', fp); (void) fflush(fp); }


/*
 * This file contains the front-end of the BSD Print Protocol adaptor.  This
 * code assumes a BSD Socket interface to the networking side.
 */

/*
 * strsplit() splits a string into a NULL terminated array of substrings
 * determined by a seperator.  The original string is modified, and newly
 * allocated space is only returned for the array itself.  If more than
 * 1024 substrings exist, they will be ignored.
 */
static char **
strsplit(char *string, const char *seperators)
{
	char	*list[BUFSIZ],
		**result;
	int	length = 0;

	if ((string == NULL) || (seperators == NULL))
		return (NULL);

	(void) memset(list, NULL, sizeof (list));
	for (list[length] = strtok(string, seperators);
		(list[length] != NULL) && (length < (BUFSIZ - 2));
		list[length] = strtok(NULL, seperators))
			length++;

	if ((result = (char **)calloc(length+1, sizeof (char *))) != NULL)
		(void) memcpy(result, list, length * sizeof (char *));

	return (result);
}


/*
 * remote_host_name() gets the hostname of the "peer" on the socket
 * connection.
 */
static char *
remote_host_name(FILE *fp)
{
	struct hostent *hp;
	struct sockaddr_in6 peer;
	socklen_t peer_len = sizeof (peer);
	int fd = fileno(fp);
	int error_num;
	char myname[MAXHOSTNAMELEN], tmp_buf[INET6_ADDRSTRLEN];
	char *hostname;

	(void) sysinfo(SI_HOSTNAME, myname, sizeof (myname));

	/* who is our peer ? */
	if (getpeername(fd, (struct sockaddr *)&peer, &peer_len) < 0) {
		if ((errno != ENOTSOCK) && (errno != EINVAL))
			return (NULL);
		else
			return (strdup(myname));
	}

	/* get their name or return a string containing their address */
	if ((hp = getipnodebyaddr((const char *)&peer.sin6_addr,
				sizeof (struct in6_addr), AF_INET6,
				&error_num)) == NULL) {
		return (strdup(inet_ntop(peer.sin6_family,
			&peer.sin6_addr, tmp_buf, sizeof (tmp_buf))));
	}

	/* is it "localhost" ? */
	if (strcasecmp(hp->h_name, "localhost") == 0)
		return (strdup(myname));

	/* duplicate the name because gethostbyXXXX() is not reentrant */
	hostname = strdup(hp->h_name);

	/* is it from one of my addresses ? */
	if ((hp = getipnodebyname(myname, AF_INET6, AI_ALL|AI_V4MAPPED,
	    &error_num)) != NULL) {
		struct in6_addr **tmp = (struct in6_addr **)hp->h_addr_list;
		int i = 0;

		while (tmp[i] != NULL) {
			if (memcmp(tmp[i++], &peer.sin6_addr, hp->h_length)
			    == 0) {
				free(hostname);
				return (strdup(myname));
			}
		}
	}

	/* It must be someone else */
	return (hostname);
}


static int
request_id_no(const char *filename)
{
	int id = -1;

	/* get the first number embedded in the string */
	while ((filename != NULL) && (*filename != NULL) &&
		(isdigit(*filename) == 0))
		filename++;

	if ((filename != NULL) && (*filename != NULL))
		id = atoi(filename);

	return (id);
}


static void
abort_transfer(char **files)
{
	syslog(LOG_DEBUG, "abort_transfer()");
	while ((files != NULL) && (*files != NULL))
		(void) unlink(*files++);
}


/*
 * transfer_job() retrieves jobs being sent from a remote system and
 * submits them as they are completely received.
 */
static int
transfer_job(const char *printer, const char *host, FILE *ifp,
		FILE *ofp)
{
	char	buf[BUFSIZ],
		*tmp;
	char	*cf = NULL;
	char	*df_list[64];	/* more than can be sent via protocol */
	int	file_no = 0;
	int	current_request = -1;
	int	tmp_id;

	(void) memset(&df_list, NULL, sizeof (df_list));

	if (adaptor_spooler_accepting_jobs(printer) < 0) {
		syslog(LOG_DEBUG,
			"attempt to transfer job(s) to disabled printer %s",
			printer);
		return (-1);
	}

	ACK(ofp);
	/* start to receive job(s) */
	while (fgets(buf, sizeof (buf), ifp) != NULL) {
		int size = 0;
		char *name = NULL;
		char *ptr;
		int fd;
		int count;

		/*
		 * When receiving jobs, buf[0] can only be 1,2 or 3
		 * Anything else, abort transfer - rfc1179
		*/
		if ((buf[0] < 1 || buf[0] > 3)) {
			syslog(LOG_ERR,
			    "protocol error - bad message from client");
			syslog(LOG_DEBUG, "control file contained: (%s)",
			    (cf ? cf : "NULL"));
			abort_transfer(df_list);
			return (-1);
		}

		count = size = atoi(strtok(&buf[1], "\n\t "));
		if ((tmp = strtok(NULL, "\n\t ")) != NULL) {
			if ((name = strrchr(tmp, '/')) != NULL) {
				/* for security */
				syslog(LOG_INFO|LOG_NOTICE,
					"Attempt to tranfer Absolute path: %s",
					tmp);
				name++;
			} else
				name = tmp;

			tmp_id = request_id_no(name);
			if ((cf != NULL) && (df_list[0] != NULL) &&
			    (tmp_id != current_request)) {
				if (adaptor_submit_job(printer, host, cf,
						df_list) != 0) {
					abort_transfer(df_list);
					return (-1);
				}
				while (file_no-- > 0)
					free(df_list[file_no]);
				(void) memset(df_list, NULL, sizeof (df_list));
				file_no = 0;
				free(cf);
				cf = NULL;
			}
		} else if (buf[0] != 1) {
			syslog(LOG_ERR, "Bad xfer message(%d), no file name",
				buf[0]);
			return (-1);
		}
		current_request = tmp_id;
		tmp = NULL;

		switch (buf[0]) {
		case '\1':	/* Abort Transfer */
			/* remove files on file_list */
			abort_transfer(df_list);
			return (-1);
		case '\2':	/* Transfer Control File */
			syslog(LOG_DEBUG, "control(%s, %d)", name, size);
			if ((cf = malloc(size + 1)) == NULL) {
				NACK(ofp);
				break;
			}
			(void) memset(cf, NULL, size + 1);
			ACK(ofp);
			ptr = cf;
			while (count > 0)
				if (((fd = fread(ptr, 1, count, ifp)) == 0) &&
				    (feof(ifp) != 0)) {
					syslog(LOG_ERR,
						"connection closed(%s): %m",
						name);
					return (-1);
				} else {
					ptr += fd;
					count -= fd;
				}
			if (fgetc(ifp) != 0) /* get ACK/NACK */
				return (-1);
			ACK(ofp);
			break;
		case '\3':	/* Transfer Data File */
			syslog(LOG_DEBUG, "data(%s, %d)", name, size);
			if ((fd = open(name, O_RDWR|O_TRUNC|O_CREAT, 0640))
			    < 0) {
				syslog(LOG_ERR, "open(%s): %m", name);
				NACK(ofp);
				break;
			}
			if (ftruncate(fd, size) < 0) {
				syslog(LOG_ERR, "ftruncate(%s): %m", name);
				NACK(ofp);
				break;
			}
			if ((size > 0) &&
			    ((tmp = mmap((caddr_t)0, (size_t)size,
					PROT_READ|PROT_WRITE,
					(MAP_SHARED | MAP_NORESERVE),
					fd, (off_t)0)) == (char *)MAP_FAILED)) {
				syslog(LOG_ERR, "mmap(%d, %d): %m", size, fd);
				NACK(ofp);
				break;
			}
			(void) close(fd);
			ACK(ofp);
			ptr = tmp;
			while (count > 0)
				if (((fd = fread(ptr, 1, count, ifp)) == 0) &&
				    (feof(ifp) != 0)) {
					syslog(LOG_ERR,
						"connection closed(%s): %m",
						name);
					return (-1);
				} else {
					ptr += fd;
					count -= fd;
				}
			(void) munmap(tmp, size);
			if (fgetc(ifp) != 0) /* get ACK/NACK */
				return (-1);
			df_list[file_no++] = strdup(name);
			ACK(ofp);
			break;
		default:
			syslog(LOG_ERR, "protocol screwup");
			return (-1);
		}
	}
	if ((cf != NULL) && (file_no != 0)) {
		if (adaptor_submit_job(printer, host, cf, df_list) != 0) {
					abort_transfer(df_list);
					return (-1);
		}
		while (file_no-- > 0)
			free(df_list[file_no]);
		free(cf);
	} else
		abort_transfer(df_list);

	return (0);
}


/*
 * This is the entry point for this program.  The program takes the
 * following options:
 * 	(none)
 */
main(int ac, char *av[])
{
	FILE	*ifp = stdin,
		*ofp = stdout;
	int	c,
		rc;
	char	buf[BUFSIZ],
		**args,
		*host,
		*dir,
		*printer,
		*requestor;


	openlog("bsd-gw", LOG_PID, LOG_LPR);

	while ((c = getopt(ac, av, "d")) != EOF)
		switch (c) {
		case 'd':
		default:
			;
		}

	if (fgets(buf, sizeof (buf), ifp) == NULL) {
		if (feof(ifp) == 0)
			syslog(LOG_ERR, "Error reading from connection: %s",
				strerror(errno));
		exit(1);
	}

#ifdef DEBUG
	if ((buf[0] > '0') && (buf[0] < '6'))
		buf[0] -= '0';
#endif

	if ((buf[0] < 1) || (buf[0] > 5)) {
		syslog(LOG_ERR, "Invalid protocol request (%d): %c%s",
			buf[0], buf[0], buf);
		(void) fprintf(ofp,
			gettext("Invalid protocol request (%d): %c%s\n"),
			buf[0], buf[0], buf);
		exit(1);
	}

	args = strsplit(&buf[1], "\t\n ");
	printer = *args++;

	if ((host = remote_host_name(ifp)) == NULL) {
		syslog(LOG_ERR, "Can't determine requesting host");
		(void) fprintf(ofp,
			gettext("Can't determine requesting host\n"));
		exit(1);
	}

	if (adaptor_available(printer) < 0) {
		if (errno == ENOENT) {
			syslog(LOG_ERR,
				"request to %s (unknown printer) from %s",
				printer, host);
			(void) fprintf(ofp,
				gettext("%s: unknown printer\n"), printer);
		} else {
			syslog(LOG_ERR,
				"Can't locate protocol adaptor for %s from %s",
				printer, host);
			(void) fprintf(ofp, gettext(
				"Can't locate protocol adaptor for %s\n"),
				printer);
		}
		exit(1);
	}

	if (adaptor_spooler_available(printer) < 0) {
		syslog(LOG_ERR, "Can't communicate with spooler for %s",
			printer);
		(void) fprintf(ofp,
			gettext("Can't communicate with spooler for %s\n"),
			printer);
		exit(1);
	}

	if (adaptor_client_access(printer, host) < 0) {
		syslog(LOG_ERR, "%s doesn't have permission to talk to %s",
			host, printer);
		(void) fprintf(ofp,
			gettext("%s doesn't have permission to talk to %s\n"),
			host, printer);
		exit(1);
	}

	if ((dir = adaptor_temp_dir(printer, host)) == NULL) {
		syslog(LOG_DEBUG, "failure to locate tmp dir");
		return (1);
	}

	if (chdir(dir) < 0) {
		syslog(LOG_DEBUG, "chdir(%s): %m", dir);
		return (1);
	}

	switch (buf[0]) {
	case '\1':	/* restart printer */
		if ((rc = adaptor_restart_printer(printer)) == 0) {
			ACK(ofp);
		} else {
			NACK(ofp);
		}
		break;
	case '\2':	/* transfer job(s) */
		rc = transfer_job(printer, host, ifp, ofp);
		break;
	case '\3':	/* show queue (short) */
	case '\4':	/* show queue (long) */
		rc = adaptor_show_queue(printer, ofp, buf[0], args);
		break;
	case '\5':	/* cancel job(s) */
		requestor = *args++;
		rc = adaptor_cancel_job(printer, ofp, requestor, host, args);
		break;
	default:
		/* NOTREACHED */
		/* your system would have to be completely hosed */
		syslog(LOG_ERR, "reboot or reinstall your system");
		rc = -1;
	}
	(void) fflush(ofp);

	syslog(LOG_DEBUG,
		"protocol request(%d) for %s completed with status %d",
		buf[0], printer, rc);
	exit(0);
}
