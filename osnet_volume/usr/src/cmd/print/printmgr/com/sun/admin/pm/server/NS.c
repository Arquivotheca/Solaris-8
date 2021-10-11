#pragma ident	"@(#)NS.c	1.3	99/04/02 SMI"
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <rpcsvc/ypclnt.h>

char glob_stdout[BUFSIZ];
char glob_stderr[BUFSIZ];

void f_cleanup(FILE *fp, char *tmpf);
void fd_cleanup(int fd1, int fd2);


#ifdef MAIN
int
main() {
	char *host = "";
	const char *user = "";
	const char *passwd = "";
	const char *locale = "C";
	const char *cmd = "echo one"

	if (_dorexec(host, user, passwd, cmd, locale) != 0)
		printf("%s\n", glob_stderr);
	else
		printf("%s\n", glob_stdout);

	_updateoldyp("modify", "javatest",
		"clarkia", "", "a new comment", "false");
}
#endif

int
_dorexec(
	const char *host,
	const char *user,
	const char *passwd,
	const char *cmd,
	const char *locale) {

	int ret = 0;
	int fd = 0;
	int fd2 = 1;

	FILE *fderr;
	char *ferr;

	(void) memset(glob_stdout, 0, BUFSIZ);
	(void) memset(glob_stderr, 0, BUFSIZ);

	/*
	 * Re-direct stderr to a file
	 */
	ferr = tempnam(NULL, NULL);
	if (ferr != NULL) {
		fderr = freopen(ferr, "w+", stderr);
	}

	fd = rexec((char **)&host, htons(512), user,
	    passwd, cmd, &fd2);

	if (fd > -1) {
		/*
		 * rexec worked. Clean up stderr file.
		 */
		f_cleanup(fderr, ferr);

		ret = read(fd, glob_stdout, BUFSIZ - 1);
		if (ret < 0) {
			(void) strncpy(glob_stderr, strerror(errno),
			    (BUFSIZ - 1));
			fd_cleanup(fd, fd2);
			return (errno);
		}

		ret = read(fd2, glob_stderr, BUFSIZ - 1);
		if (ret < 0) {
			(void) strncpy(glob_stderr, strerror(errno),
			    (BUFSIZ - 1));
			fd_cleanup(fd, fd2);
			return (errno);
		}
	} else {
		/*
		 * rexec failed. Read from the stderr file.
		 */
		if (fderr != NULL) {
			char tmpbuf[BUFSIZ];

			(void) rewind(fderr);
			strcpy(glob_stderr, "");
			while (fgets(tmpbuf, BUFSIZ - 1,
			    fderr) != NULL) {
				if ((strlen(glob_stderr) +
				    strlen(tmpbuf)) > BUFSIZ - 1) {
					break;
				} else {
					(void) strcat(glob_stderr, tmpbuf);
				}
			}
		}
		f_cleanup(fderr, ferr);
		fd_cleanup(fd, fd2);
		return (1);
	}
	fd_cleanup(fd, fd2);
	return (0);
}

void
fd_cleanup(int fd, int fd2)
{
	if (fd > 0) {
		(void) close(fd);
	}
	if (fd2 > 0) {
		(void) close(fd2);
	}
}

void
f_cleanup(FILE *fp, char *tmpf)
{
	if (fp != NULL) {
		(void) fclose(fp);
	}
	if (tmpf != NULL) {
		(void) unlink(tmpf);
		(void) free(tmpf);
	}
}

struct ns_bsd_addr {
	char  *server;		/* server name */
	char  *printer;		/* printer name or NULL */
	char  *extension;	/* RFC-1179 conformance */
};
typedef struct ns_bsd_addr ns_bsd_addr_t;

/* Key/Value pair structure */
struct ns_kvp {
	char *key;	/* key */
	void *value;	/* value converted */
};
typedef struct ns_kvp ns_kvp_t;

/* Printer Object structure */
struct ns_printer {
	char *name;		/* primary name of printer */
	char **aliases;		/* aliases for printer */
	char *source;		/* name service derived from */
	ns_kvp_t **attributes;	/* key/value pairs. */
};
typedef struct ns_printer ns_printer_t;

extern ns_printer_t *ns_printer_get_name(const char *, const char *);
extern int ns_printer_put(const ns_printer_t *);
extern char *ns_get_value_string(const char *, const ns_printer_t *);
extern int ns_set_value(const char *, const void *, ns_printer_t *);
extern int ns_set_value_from_string(const char *, const char *,
					ns_printer_t *);
int
_updateoldyp(
	const char *action,
	const char *printername,
	const char *printserver,
	const char *extensions,
	const char *comment,
	const char *isdefault) {

	ns_printer_t *printer;
	ns_bsd_addr_t *addr;
	int status = 0;

	char mkcmd[BUFSIZ];
	char *domain = NULL;
	char *host = NULL;

	/*
	 * libprint returns before we know that the printers.conf
	 * map is made. So we'll make it again.
	 */
	(void) yp_get_default_domain(&domain);

	if ((yp_master(domain, "printers.conf.byname", &host) != 0) &&
	    (yp_master(domain, "passwd.byname", &host) != 0)) {
		strcat(mkcmd, "/usr/bin/sleep 1");
	} else {
		/* CSTYLED */
		sprintf(mkcmd, "/usr/bin/rsh -n %s 'cd /var/yp; /usr/ccs/bin/make -f /var/yp/Makefile -f /var/yp/Makefile.print printers.conf > /dev/null'", host);
	}

	if (strcmp(action, "delete") == 0) {
		if ((printer = (ns_printer_t *)
		    ns_printer_get_name(printername, "nis")) == NULL) {
			return (0);
		}

		printer->attributes = NULL;
		status = ns_printer_put(printer);
		if (status != 0) {
			(void) free(printer);
			return (status);
		}

		if ((printer = (ns_printer_t *)
		    ns_printer_get_name("_default", "nis")) != NULL) {
			char *dflt = (char *)
			    ns_get_value_string("use", printer);
			if ((dflt != NULL) &&
			    (strcmp(dflt, printername) == 0)) {
				printer->attributes = NULL;
				status = ns_printer_put(printer);
				if (status != 0) {
					(void) free(printer);
					return (status);
				}
			}
		}
		(void) free(printer);
		(void) system(mkcmd);
		return (0);

	} else if (strcmp(action, "add") == 0) {
		printer = (ns_printer_t *)malloc(sizeof (*printer));
		memset(printer, 0, sizeof (*printer));
		printer->name = (char *)printername;
		printer->source = "nis";

		addr = (ns_bsd_addr_t *)malloc(sizeof (*addr));
		memset(addr, 0, sizeof (*addr));
		addr->printer = (char *)printername;
		addr->server = (char *)printserver;
		if ((extensions != NULL) &&
		    (strlen(extensions) > 0)) {
			addr->extension = (char *)extensions;
		}
		ns_set_value("bsdaddr", addr, printer);

		if ((comment != NULL) && (strlen(comment) > 0)) {
			ns_set_value_from_string("description",
			    comment, printer);
		}
		status = ns_printer_put(printer);
		if (status != 0) {
			(void) free(addr);
			(void) free(printer);
			return (status);
		}

		if (strcmp(isdefault, "true") == 0) {
			printer->name = "_default";
			printer->attributes = NULL;
			ns_set_value_from_string("use", printername, printer);
			status = ns_printer_put(printer);
			if (status != 0) {
				(void) free(addr);
				(void) free(printer);
				return (status);
			}
		}
		(void) free(addr);
		(void) free(printer);
		(void) system(mkcmd);
		return (0);
	}

	/*
	 * Modify
	 */
	if ((printer = (ns_printer_t *)
	    ns_printer_get_name(printername, "nis")) == NULL) {
		return (1);
	}
	if ((comment != NULL) && (strlen(comment) > 0)) {
		ns_set_value_from_string("description", comment, printer);
	} else {
		ns_set_value_from_string("description",
		    NULL, printer);
	}
	status = ns_printer_put(printer);
	if (status != 0) {
		(void) free(printer);
		return (status);
	}

	if ((printer = (ns_printer_t *)
	    ns_printer_get_name("_default", "nis")) != NULL) {
		char *dflt = (char *) ns_get_value_string("use", printer);
		if (strcmp(printername, dflt) == 0) {
			if (strcmp(isdefault, "false") == 0) {
				/*
				 * We were the default printer but not
				 * any more.
				 */
				printer->attributes = NULL;
				status = ns_printer_put(printer);
				if (status != 0) {
					(void) free(printer);
					return (status);
				}
			}
		} else {
			if (strcmp(isdefault, "true") == 0) {
				ns_set_value_from_string("use",
				    printername, printer);
				status = ns_printer_put(printer);
				if (status != 0) {
					(void) free(printer);
					return (status);
				}
			}
		}
	} else {
		printer = (ns_printer_t *)malloc(sizeof (*printer));
		memset(printer, 0, sizeof (*printer));
		printer->name = "_default";
		printer->source = "nis";
		ns_set_value_from_string("use", printername, printer);
		status = ns_printer_put(printer);
		if (status != 0) {
			(void) free(printer);
			return (status);
		}
	}
	(void) system(mkcmd);
	return (0);
}
