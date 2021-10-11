/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)nss_write.c	1.11	99/08/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <netdb.h>	/* for rcmd() */

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>


/*  escaped chars include delimiters and shell meta characters */
#define	ESCAPE_CHARS	"\\\n=: `&;|>^$()<*?["

/*
 * This modules contains all of the code nedessary to write back to each
 * printing configuration data repository.  The support is intended to
 * introduce the least number of dependencies in the library, so it doesn't
 * always perform it's operations in the cleanest fashion.  For example,
 * the XFN support doesn't use the XFN API, but instead, calls out to the
 * XFN CLI to perform it's tasks.
 */


/*
 * Generic Files support begins here.
 */
static char *
freadline(FILE *fp, char *buf, int buflen)
{
	char *s = buf;

	while (fgets(s, buflen, fp)) {
		if ((s == buf) && ((*s == '#') || (*s == '\n'))) {
			continue;
		} else {
			if ((*s == '#') || (*s == '\n')) {
				*s = NULL;
				break;
			}

			buflen -= strlen(s);
			s += strlen(s);

			if (*(s - 2) != '\\')
				break;
#ifdef STRIP_CONTINUATION
			buflen -= 2;
			s -= 2;
#endif
		}
	}

	if (s == buf)
		return (NULL);
	else
		return (buf);
}


static int
_file_put_printer(const char *file, const ns_printer_t *printer)
{
	FILE	*ifp,
		*ofp;
	char *tmpfile;
	int fd;
	int exit_status = 0;
	int size;

	size = strlen(file) + 1 + 20;
	tmpfile = malloc(size);
	if ((snprintf(tmpfile, size, "%s.%ld", file, getpid()))
						>= size) {
		syslog(LOG_ERR, "_file_put_printer:buffer overflow:tmpfile");
		return (-1);
	}

	/* LINTED */
	while (1) {	/* syncronize writes */
		fd = open(file, O_RDWR|O_CREAT);
		if (fd < 0) {
			if (errno == EAGAIN)
				continue;
			free(tmpfile);
			return (-1);
		}
		if (lockf(fd, F_TLOCK, 0) == 0)
			break;
		close(fd);
	}

	if ((ifp = fdopen(fd, "r")) == NULL) {
		close(fd);
		free(tmpfile);
		return (-1);
	}

	if ((ofp = fopen(tmpfile, "wb+")) != NULL) {
		char buf[4096];

		fprintf(ofp,
	"#\n#\tIf you hand edit this file, comments and structure may change.\n"
	"#\tThe preferred method of modifying this file is through the use of\n"
	"#\tlpset(1M)\n#\n");

	/*
	 * Handle the special case of lpset -x all
	 * This deletes all entries in the file
	 * In this case, just don't write any entries to the tmpfile
	 */

		if (!((strcmp(printer->name, "all") == 0) &&
				(printer->attributes == NULL))) {
			char *t, *entry, *pentry;

			_cvt_printer_to_entry((ns_printer_t *)printer,
							buf, sizeof (buf));
			t = pentry = strdup(buf);

			while (freadline(ifp, buf, sizeof (buf)) != NULL) {
				ns_printer_t *tmp = (ns_printer_t *)
					_cvt_nss_entry_to_printer(buf, "");

				if (ns_printer_match_name(tmp, printer->name)
						== 0) {
					entry = pentry;
					pentry = NULL;
				} else
					entry = buf;

				fprintf(ofp, "%s\n", entry);
			}

			if (pentry != NULL)
				fprintf(ofp, "%s\n", pentry);
			free(t);
		}

		fclose(ofp);
		rename(tmpfile, file);
	} else
		exit_status = -1;

	fclose(ifp);	/* releases the lock, after rename on purpose */
	chmod(file, 0644);
	free(tmpfile);
	return (exit_status);
}


/*
 * Support for writing a printer into the FILES /etc/printers.conf
 * file.
 */
int
files_put_printer(const ns_printer_t *printer)
{
	static char *file = "/etc/printers.conf";

	return (_file_put_printer(file, printer));
}


/*
 * Support for writing a printer into the NIS printers.conf.byname
 * map.
 */

#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>

/*
 * Run the remote command.  We aren't interested in any io, Only the
 * return code.
 */
static int
remote_command(char *command, char *host)
{
	struct passwd *pw;

	if ((pw = getpwuid(getuid())) != NULL) {
		int fd;

		if ((fd = rcmd_af(&host, htons(514), pw->pw_name, "root",
				command, NULL, AF_INET6)) < 0)
			return (-1);
		close(fd);
		return (0);
	} else
		return (-1);
}


/*
 * This isn't all that pretty, but you can update NIS if the machine this
 * runs on is in the /.rhosts or /etc/hosts.equiv on the NIS master.
 *   copy it local, update it, copy it remote
 */
#define	TMP_PRINTERS_FILE	"/tmp/printers.NIS"
#define	NIS_MAKEFILE		"/var/yp/Makefile"
#define	MAKE_EXCERPT		"/usr/lib/print/Makefile.yp"
/*ARGSUSED*/
int
nis_put_printer(const ns_printer_t *printer)
{
	static char	*domain = NULL;
	char *map = "printers.conf.byname";
	char *tmp = NULL;
	char *host = NULL;
	char lfile[BUFSIZ];
	char rfile[BUFSIZ];
	char cmd[BUFSIZ];

	if (domain == NULL)
		(void) yp_get_default_domain(&domain);

	if ((yp_master(domain, (char *)map, &host) != 0) &&
	    (yp_master(domain, "passwd.byname", &host) != 0))
		return (-1);

	if (snprintf(lfile, sizeof (lfile), "/tmp/%s", map) >=
			sizeof (lfile)) {
		syslog(LOG_ERR, "nis_put_printer:lfile buffer overflow");
		return (-1);
	}
	if (snprintf(rfile, sizeof (rfile), "root@%s:/etc/%s", host, map) >=
			sizeof (rfile)) {
		syslog(LOG_ERR, "nis_put_printer:rfile buffer overflow");
		return (-1);
	}

	if (((tmp = strrchr(rfile, '.')) != NULL) &&
	    (strcmp(tmp, ".byname") == 0))
		*tmp = NULL;	/* strip the .byname */

	/* copy it local */
	if (snprintf(cmd, sizeof (cmd), "rcp %s %s >/dev/null 2>&1",
		rfile, lfile) >= sizeof (cmd)) {
		    syslog(LOG_ERR,
			    "nis_put_printer:buffer overflow building cmd");
		    return (-1);
	}
	system(cmd);	/* could fail because it doesn't exist */


	/* update it */
	if (_file_put_printer(lfile, printer) != 0)
		return (-1);

	/* copy it back */
	if (snprintf(cmd, sizeof (cmd), "rcp %s %s >/dev/null 2>&1",
		lfile, rfile) >= sizeof (cmd)) {
		    syslog(LOG_ERR,
			    "nis_put_printer:buffer overflow building cmd");
		    return (-1);
	}
	if (system(cmd) != 0)
		return (-1);

	/* copy the Makefile excerpt */
	if (snprintf(cmd, sizeof (cmd),
			"rcp %s root@%s:%s.print >/dev/null 2>&1",
			MAKE_EXCERPT, host, NIS_MAKEFILE) >= sizeof (cmd)) {
		syslog(LOG_ERR,
			"nis_put_printer:buffer overflow building cmd");
		return (-1);
	}

	if (system(cmd) != 0)
		return (-1);

	/* run the make */
	if (snprintf(cmd, sizeof (cmd),
			"/bin/sh -c 'PATH=/usr/ccs/bin:/bin:/usr/bin:$PATH "
			"make -f %s -f %s.print printers.conf >/dev/null 2>&1'",
			NIS_MAKEFILE, NIS_MAKEFILE) >= sizeof (cmd)) {
		syslog(LOG_ERR,
			"nis_put_printer:buffer overflow on make");
		return (-1);
	}

	return (remote_command(cmd, host));
}

/*
 * Support for writing a printer into the NISPLUS org_dir.printers table
 * begins here.  This support uses the nisplus(5) commands rather than the
 * nisplus API.  This was done to remove the dependency in libprint on the
 * API, which is used for lookup in a configuration dependent manner.
 */
#define	NISPLUS_CREATE	"/usr/bin/nistest -t T printers.org_dir || "\
			"( /usr/bin/nistbladm "\
			"-D access=og=rmcd,nw=r:group=admin."\
				"`/usr/bin/nisdefaults -d` "\
			"-c printers_tbl key=S,nogw= datum=,nogw= "\
			"printers.org_dir.`/usr/bin/nisdefaults -d` )"

#define	NISPLUS_REMOVE	"/usr/bin/nistbladm  -R key=%s printers.org_dir"
#define	NISPLUS_UPDATE	"/usr/bin/nistbladm  -A key=%s datum="

nisplus_put_printer(const ns_printer_t *printer)
{
	int rc = 0;
	char cmd[BUFSIZ];

	if (printer == NULL)
		return (rc);

	/* create the table if it doesn't exist */
	system(NISPLUS_CREATE);

	if (printer->attributes != NULL) {
		int		len;
		ns_kvp_t	**kvp;

		if (snprintf(cmd, sizeof (cmd), NISPLUS_UPDATE,
				printer->name) >= sizeof (cmd)) {
		    syslog(LOG_ERR,
		    "nisplus_put_printer:NISPLUS_UPDATE:buffer overflow");
		    return (-1);
		}

		len = strlen(cmd);

		/* Append key/value pairs */
		for (kvp = printer->attributes; *kvp != NULL; kvp++)
			if (((*kvp)->key != NULL) && ((*kvp)->value != NULL)) {
			strlcat(cmd, ":", sizeof (cmd));
			strncat_escaped(cmd, (*kvp)->key, sizeof (cmd),
				ESCAPE_CHARS);
			strlcat(cmd, "=", sizeof (cmd));
			strncat_escaped(cmd, (*kvp)->value,
			sizeof (cmd), ESCAPE_CHARS);
	}

		if (len != strlen(cmd))
			strlcat(cmd, " printers.org_dir", sizeof (cmd));
		else
			snprintf(cmd, sizeof (cmd), NISPLUS_REMOVE,
						printer->name);

	} else
		snprintf(cmd, sizeof (cmd), NISPLUS_REMOVE, printer->name);

	if (strlcat(cmd, " >/dev/null 2>&1", sizeof (cmd)) >= sizeof (cmd)) {
		syslog(LOG_ERR, "nisplus_put_printer: buffer overflow");
		return (-1);
	}

	/* add/modify/delete the entry */
	rc = system(cmd);

	return (rc);
}

/*
 * Support for writing a printer into an XFN printer context begins
 * here.  The support uses fns(5) commands rather than the XFN API.
 * This is done for a couple of reasons.  First, and most important,
 * creating a printer context with the XFN API requires use of private
 * XFN internal data structures.  Second, using the API introduces an
 * unnecessary runtime dependency on libxfn.
 */

#define	XFN_CREATE	"/usr/sbin/fncreate -t org org// >/dev/null 2>&1"
#define	XFN_UPDATE	"/usr/bin/fncreate_printer -s %s %s"
#define	XFN_DESTROY	"/usr/sbin/fndestroy %s/%s"
#define	XFN_UNBIND	"/usr/bin/fnunbind %s/%s"
#define	XFN_CONTEXT	"thisorgunit/service/printer"

/*
 * This routine will write data into an XFN printer context.
 */
int
xfn_put_printer(const ns_printer_t *printer)
{
	char cmd[BUFSIZ];
	int rc = -1;
	int bufferavail;

	if (printer == NULL)
		return (rc);

#ifdef NOTDEF
	/* create the initial FNS contexts, this can take a very long time */
	system(XFN_CREATE);
#endif

	/* remove the old reference */
	if (snprintf(cmd, sizeof (cmd), XFN_DESTROY " >/dev/null 2>&1",
		XFN_CONTEXT, printer->name) >= sizeof (cmd))
			return (-1);
	system(cmd);

	if (snprintf(cmd, sizeof (cmd), XFN_UNBIND " >/dev/null 2>&1",
		XFN_CONTEXT, printer->name) >= sizeof (cmd))
			return (-1);
	rc = system(cmd);

	if (printer->attributes != NULL) {
		ns_kvp_t	**kvp;
		int 		len;

		if (snprintf(cmd, sizeof (cmd), XFN_UPDATE, XFN_CONTEXT,
				printer->name) >= sizeof (cmd)) {
			syslog(LOG_ERR, "xfn_put_printer:buffer overflow");
			return (-1);
		}

		len = strlen(cmd);

		/* Append key/value pairs */
		for (kvp = printer->attributes; *kvp != NULL; kvp++) {

			bufferavail = sizeof (cmd) - strlen(cmd) - 1;

			if (((*kvp)->key != NULL) && ((*kvp)->value != NULL) &&
				(strlen((*kvp)->key) + strlen((*kvp)->value) +
				strlen(ESCAPE_CHARS) * 2 + 2 < bufferavail)) {

				strlcat(cmd, " ", sizeof (cmd));

				strncat_escaped(cmd, (*kvp)->key,
						sizeof (cmd),
						ESCAPE_CHARS);

				strlcat(cmd, "=", sizeof (cmd));

				strncat_escaped(cmd, (*kvp)->value,
						sizeof (cmd),
						ESCAPE_CHARS);
			}
		}

		if (len != strlen(cmd)) {
			strlcat(cmd, " >/dev/null 2>&1", sizeof (cmd));

			rc = system(cmd);
		}
	}

	return (rc);
}
