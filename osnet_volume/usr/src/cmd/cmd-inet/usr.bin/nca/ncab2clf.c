/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ncab2clf.c	1.2	99/08/11 SMI"

/*
 *
 *	Converts binary log files to CLF (Common Log Format).
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#include <time.h>
#include <synch.h>
#include <syslog.h>

#include "ncadoorhdr.h"
#include "ncalogd.h"

extern char *gettext();

typedef	enum	{	/* Boolean type */
	false = 0,
	true  = 1
} bool;

static const char *const
g_method_strings[8] = {
	"UNKNOWN",
	"OPTIONS",
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"TRACE"
};

/* Short month strings */
static const char * const sMonthStr [12] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec",
};

#define	SEC_PER_MIN		(60)
#define	SEC_PER_HOUR		(60*60)
#define	SEC_PER_DAY		(24*60*60)
#define	SEC_PER_YEAR		(365*24*60*60)
#define	LEAP_TO_70		(70/4)

#define	KILO_BYTE		(1024)
#define	MEGA_BYTE		(KILO_BYTE * KILO_BYTE)
#define	GIGA_BYTE		(KILO_BYTE * MEGA_BYTE)

#define	CLF_DATE_BUF_LENGTH	(128)
#define	OUTFILE_BUF_SIZE	(256 * KILO_BYTE)

static bool	g_enable_directio = true;
static ssize_t	g_invalid_count = 0;

/* init value must match logd & NCA kmod */
static ssize_t	g_n_log_upcall = 0;

/* input binary file was written in 64k chunks by default  */
static ssize_t	g_infile_blk_size = NCA_DEFAULT_LOG_BUF_SIZE;

/*
 * http_version(version)
 *
 * Returns out the string of a given http version
 */

static char *
http_version(int http_ver)
{
	char	*ver_num;

	switch (http_ver) {
	case HTTP_0_9:
	case HTTP_0_0:
		ver_num = "HTTP/0.9";
		break;
	case HTTP_ERR:
	case HTTP_1_0:
		ver_num = "HTTP/1.0";
		break;
	case HTTP_1_1:
		ver_num = "HTTP/1.1";
		break;
	default:
		ver_num = "HTTP/unknown";
	}

	return (ver_num);
}

static bool
valid_version(int http_ver)
{
	switch (http_ver) {
	case HTTP_0_9:
	case HTTP_0_0:
	case HTTP_1_0:
	case HTTP_1_1:
		return (true);
	default:
		break;
	}

	return (false);
}

static bool
valid_method(int method)
{
	switch (method) {
	case NCA_OPTIONS:
	case NCA_GET:
	case NCA_HEAD:
	case NCA_POST:
	case NCA_PUT:
	case NCA_DELETE:
	case NCA_TRACE:
		return (true);
	default:
		break;
	}

	return (false);
}

/*
 * http_method
 *
 *   Returns the method string for the given method.
 */

static char *
http_method(int method)
{
	if (method < sizeof (g_method_strings) / sizeof (g_method_strings[0]))
		return ((char *)(g_method_strings[method]));
	else
		return ((char *)(g_method_strings[0]));
}

/* sMonth: Return short month string */

static const char *
sMonth(int index)
{
	return (sMonthStr[index]);
}

/*
 * Debug formatting routine.  Returns a character string representation of the
 * addr in buf, of the form xxx.xxx.xxx.xxx.  This routine takes the address
 * as a pointer.  The "xxx" parts including left zero padding so the final
 * string will fit easily in tables.  It would be nice to take a padding
 * length argument instead.
 */

static char *
ip_dot_saddr(uchar_t *addr, char *buf)
{
	(void) sprintf(buf, "%03d.%03d.%03d.%03d",
	    addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF, addr[3] & 0xFF);
	return (buf);
}

/*
 * Debug formatting routine.  Returns a character string representation of the
 * addr in buf, of the form xxx.xxx.xxx.xxx.  This routine takes the address
 * in the form of a ipaddr_t and calls ip_dot_saddr with a pointer.
 */

static char *
ip_dot_addr(ipaddr_t addr, char *buf)
{
	return (ip_dot_saddr((uchar_t *)&addr, buf));
}

static void
http_clf_date(char *buf, time_t t)
{
	struct tm	local_time;
	long		time_zone_info;
	char		sign;
	extern struct tm *localtime_r();

	if (localtime_r(&t, &local_time) == 0)
		return;

	if (local_time.tm_isdst)
		time_zone_info = -timezone + SEC_PER_HOUR;
	else
		time_zone_info = -timezone;

	if (time_zone_info < 0) {
		sign = '-';
		time_zone_info = -time_zone_info;
	} else {
		sign = '+';
	}

	(void) sprintf(buf, "[%02d/%s/%04d:%02d:%02d:%02d %c%02ld%02ld]",
		local_time.tm_mday, sMonth(local_time.tm_mon),
		1900 + local_time.tm_year, local_time.tm_hour,
		local_time.tm_min, local_time.tm_sec,
		sign, time_zone_info / SEC_PER_HOUR,
		time_zone_info % SEC_PER_HOUR);
}

/*
 * xmalloc(size)
 * Abort if malloc fails
 */

static void *
xmalloc(size_t size)
{
	void *p;

	if (! size)
		size = 1;

	if ((p = malloc(size)) == NULL) {
		syslog(LOG_ERR, gettext("Error: Out of memory\n"));
		abort();
	}

	return (p);
}

/*
 * xstrdup(string)
 *   duplicate string
 */

static char *
xstrdup(const char *string)
{
	char	*new_string;

	if (string) {
		new_string = xmalloc(strlen(string) + 1);
		(void) strcpy(new_string, string);

		return (new_string);
	}

	return (NULL);
}

static void
usage()
{

	(void) fprintf(stderr, gettext(
		"\nncab2clf [-hvbD] [-i <binary-log-file>]"
		"  [-o <output-file-name>]\n"));
	(void) fprintf(stderr, gettext(
	    "\t converts a NCA binary log file to HTTP CLF"
	    " (Common Log Format)\n\n"));
	(void) fprintf(stderr, gettext("\t -h	- this usage message\n"));
	(void) fprintf(stderr, gettext("\t -i <binary-log-file>\n"));
	(void) fprintf(stderr, gettext("\t -o <output-file-name>\n"));
	(void) fprintf(stderr, gettext("\t -D disable directio on"
	    " <output-file-name>\n"));
	(void) fprintf(stderr, gettext("\t -b <input-file-blocking in KB -"
		" default is 64K bytes>\n"));
	(void) fprintf(stderr, gettext(
		"\t note: if no <output-file> - output goes to standard"
		" output\n"));
	(void) fprintf(stderr, gettext(
		"\t note: if no <input-file> - input is taken from"
		" standard input\n"));

	exit(3);
}

static void
close_files(int ifd, int ofd)
{
	if (ifd != STDIN_FILENO)
		(void) close(ifd);

	if (ofd != STDOUT_FILENO)
		(void) close(ofd);
}

/*
 * Read the requested number of bytes from the given file descriptor
 */

static ssize_t
read_n_bytes(int fd, char *buf, ssize_t bufsize)
{
	ssize_t	num_to_read = bufsize;
	ssize_t	num_already_read = 0;
	ssize_t	i;

	while (num_to_read > 0) {

		i = read(fd, &(buf[num_already_read]), num_to_read);
		if (i < 0) {
			if (errno == EINTR)
				continue;
			else
				(void) fprintf(stderr, gettext(
				    "Error (%d) reading input file\n"), errno);
				return (-1);	/* some wierd interrupt */
		}

		if (i == 0)
			break;

		num_already_read += i;
		num_to_read -= i;
	}

	return (num_already_read);
}

/*
 * Write the requested number of bytes to the given file descriptor
 */

static ssize_t
write_n_bytes(int fd, char *buf, ssize_t bufsize)
{
	ssize_t	num_to_write = bufsize;
	ssize_t	num_written = 0;
	ssize_t	i;

	while (num_to_write > 0) {

		i = write(fd, &(buf[num_written]), num_to_write);
		if (i < 0) {
			if (errno == EINTR)
				continue;
			else
				(void) fprintf(stderr, gettext(
				    "Error (%d) writing output file\n"), errno);
				return (-1);	/* some wierd interrupt */
		}

		num_written += i;
		num_to_write -= i;
	}

	return (num_written);
}

/* do constraint checks and determine if it's a valid header */

static bool
is_valid_header(void *ibuf)
{
	nca_log_buf_hdr_t	*h;
	nca_log_stat_t		*s;

	h = (nca_log_buf_hdr_t *)ibuf;

	/* Do some validity checks on ibuf */

	if (((h->nca_loghdr).nca_version != NCA_LOG_VERSION1) ||
	    ((h->nca_loghdr).nca_op != log_op)) {
		return (false);
	}

	s = &(h->nca_logstats);

	if (g_n_log_upcall == 0) {
		g_n_log_upcall = s->n_log_upcall;
	} else {
		if ((++g_n_log_upcall) != (ssize_t)s->n_log_upcall) {
			(void) fprintf(stderr, gettext(
				"Warning: expected record number (%d) is"
				" different from the one seen (%d)\n."
				" Resetting the expected record"
				" number.\n"), g_n_log_upcall, s->n_log_upcall);

			g_n_log_upcall = s->n_log_upcall;
		}
	}

	return (true);
}

/* convert input binary buffer into CLF */

static int
b2clf_buf(
	void	*ibuf,
	char	*obuf,
	ssize_t	isize,
	ssize_t	osize,
	ssize_t	*out_size)
{
	nca_log_buf_hdr_t	*h;
	nca_log_stat_t		*s;
	nca_request_log_t	*r;

	char	*br;
	void	*er;
	char	ip_buf[64];
	ssize_t	max_input_size, num_bytes_read;
	int	n_recs;
	bool	error_seen;

	ssize_t	count;
	char	clf_timebuf[CLF_DATE_BUF_LENGTH];
	char	*method;
	char	*http_version_string;
	char	*ruser;
	char	*req_url;
	char	*remote_ip;

	h = (nca_log_buf_hdr_t *)ibuf;
	s = &(h->nca_logstats);
	r = (nca_request_log_t *)(&(h[1]));

	/* OK, it's a valid buffer which we can use, go ahead and convert it */

	max_input_size = (ssize_t)isize - sizeof (nca_log_buf_hdr_t);

	*out_size = 0;
	error_seen = false;
	num_bytes_read = 0;
	for (n_recs = 0; n_recs < s->n_log_recs; n_recs++) {

		/* Make sure there is enough space in the output buffer */

		if ((*out_size >= osize) ||
				(num_bytes_read >= max_input_size)) {
			error_seen = true;
			break;
		}

		http_clf_date(clf_timebuf, ((time_t)r->start_process_time));

		/* Only logs valid HTTP ops */

		if ((! valid_method((int)r->method)) ||
				(! valid_version((int)r->version))) {
			++g_invalid_count;
			goto skip;
		}

		method = http_method((int)r->method);
		http_version_string = http_version((int)r->version);

		remote_ip = ip_dot_addr(r->remote_host, (char *)&ip_buf);
		if (r->remote_user_len) {
			ruser = NCA_REQLOG_RDATA(r, remote_user);
		} else {
			ruser = "-";
		}

		if (r->request_url_len) {
			req_url = NCA_REQLOG_RDATA(r, request_url);
		} else {
			req_url = "UNKNOWN";
		}

		count = (ssize_t)sprintf(&(obuf[*out_size]),
				"%s %s %s %s \"%s %s %s\" %d %d\n",
				((remote_ip) ? remote_ip : "-"),
				/* should be remote_log_name */
				"-",
				ruser,
				clf_timebuf,
				method,
				req_url,
				http_version_string,
				r->response_status,
				r->response_len);

		*out_size += count;
	skip:
		br = (char *)r;
		er = ((char *)r) + NCA_LOG_REC_SIZE(r);

		r = (nca_request_log_t *)NCA_LOG_ALIGN(er);
		num_bytes_read += (ssize_t)(((char *)r) - br);
	}

	if (error_seen) {
		(void) fprintf(stderr, gettext(
			"Error: Input buffer not fully converted.\n"));

		if (n_recs != s->n_log_recs)
			(void) fprintf(stderr, gettext(
				"Warning: Converted only %d of %d records\n"),
				n_recs, s->n_log_recs);
	}

	return (0);
}

static int
b2clf(int ifd, int ofd)
{
	char	*ibuf;
	char	*obuf;
	bool	error_seen;
	bool	eof_seen;
	ssize_t	num_iterations, ni, nh, no, olen;

	nca_log_buf_hdr_t	*h;
	nca_log_stat_t		*s;

	ibuf = xmalloc(g_infile_blk_size);
	obuf = xmalloc(OUTFILE_BUF_SIZE);
	error_seen = false;

	eof_seen = false;
	num_iterations = 0;
	while (! eof_seen) {
		++num_iterations;

		nh = ni = no = 0;

		/* read the binary header first */

		nh = read_n_bytes(ifd, ibuf, sizeof (nca_log_buf_hdr_t));
		if (nh != sizeof (nca_log_buf_hdr_t)) {
			eof_seen = true;
			break;
		}

		if (! is_valid_header(ibuf)) {
			(void) fprintf(stderr, gettext(
			    "Error: Can't convert the input data to CLF\n"));
			continue;
		}

		/* read the data to be converted */

		h = (nca_log_buf_hdr_t *)ibuf;
		s = &(h->nca_logstats);

		if (s->n_log_size == 0)
			continue;

		ni = read_n_bytes(ifd, &(ibuf[nh]), (ssize_t)s->n_log_size);
		if (ni < 0) {
			error_seen = true;
			break;
		} else if (ni < (ssize_t)s->n_log_size) {
			eof_seen = true;
		}

		if (ni == 0)
			break;

		/* convert binary input into text output */

		if (b2clf_buf(ibuf, obuf, ni + nh, OUTFILE_BUF_SIZE, &olen)) {
			(void) fprintf(stderr, gettext(
			    "Error: Can't convert the input data to CLF\n"));
			error_seen = true;
			break;
		}

		/* write out the text data */
		no = write_n_bytes(ofd, obuf, olen);
		if (no != olen) {
			error_seen = true;
			break;
		}

		bzero(ibuf, nh + ni);
		bzero(obuf, no);
	}

	free(ibuf);
	free(obuf);

	if (error_seen)
		return (-1);

	return (0);
}


int
main(int argc, char **argv)
{
	int	c;
	int	ifd;		/* input fd - binary log file */
	int	ofd;

	char	*infile = NULL;  /* input file name */
	char	*outfile = NULL; /* output file name */

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	/* parse any arguments */
	while ((c = getopt(argc, argv, "hDi:o:b:")) != EOF) {
		switch (c) {
		case 'h':
			usage();
			break;
		case 'i':
			infile = xstrdup(optarg);
			break;
		case 'D':
			g_enable_directio = false;
			break;
		case 'o':
			outfile = xstrdup(optarg);
			break;
		case 'b':
			g_infile_blk_size = (KILO_BYTE * atoi(optarg));
			break;
		case '?':
			usage();
			break;
		}
	}

	/* set up the input stream */

	if (infile) {

		if ((ifd = open(infile, O_RDONLY)) < 0) {
			(void) fprintf(stderr,
				gettext(
				"Error: Failure to open binary log file %s \n"),
				infile);
			exit(1);
		}

	} else {
		ifd = STDIN_FILENO;
	}

	/* set up the output stream */

	if (outfile) {

		if ((ofd = open(outfile, O_WRONLY|O_CREAT, 0644)) < 0) {
			(void) fprintf(stderr, gettext(
			"Error: Failure to open output file %s \n"), outfile);
			exit(1);
		}

		/* Enable directio on output stream if specified */

		if (g_enable_directio)
			(void) directio(ofd, DIRECTIO_ON);

	} else {
		ofd = STDOUT_FILENO;
	}

	if ((b2clf(ifd, ofd) != 0)) {
		close_files(ifd, ofd);
		exit(2);
	}

	close_files(ifd, ofd);

	if (g_invalid_count) {
		(void) fprintf(stderr, gettext("Warning: %d"
		" number of invalid log records encountered in binary input"
		" file were skipped\n"), g_invalid_count);
	}

	return (0);
}
