/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ncahttp.c	1.18	99/12/01 SMI"

const char ncahttp_version[] = "@(#)ncahttp.c	1.18	99/12/01 SMI";

#define	_IP_C

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#define	_SUN_TPI_VERSION 2
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/atomic.h>

#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/isa_defs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/tcp.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/sockio.h>

#include <sys/strick.h>

#include <netinet/igmp_var.h>
#include <inet/ip.h>

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/door.h>
#include <sys/door_data.h>

#include <sys/filio.h>
#include <sys/fcntl.h>

#include <sys/time.h>

#include "nca.h"
#include "ncakmem.h"
#include "ncadoorhdr.h"
#include "ncalogd.h"
#include "ncandd.h"


#if	SunOS == SunOS_5_7

#define	DD2F(ddp) (file_t *)(ddp)->d_data.d_fp

#elif	SunOS == SunOS_5_8

#define	DD2F(ddp) (file_t *)(ddp)->d_data.d_handle

#endif	/* SunOS */


extern ncaparam_t nca_param_arr[];

#define	LOG_HELP


ulong_t nca_fill_mp_NULL1 = 0;
ulong_t nca_fill_mp_NULL2 = 0;
ulong_t nca_fill_mp_NULL3 = 0;

int nca_httpd_debug = false;


static int nca_expire_ttl = -1;

extern char nca_httpd_door_path[];

extern time_t time;

static uint16_t	tcp_cksum(uint8_t *, size_t);

static void	node_fr(node_t *);


static log_buf_t	*logbuf = NULL;
static struct kmem_cache *logbuf_cache;
static kmutex_t		logbuf_lock;

int			nca_logging_on = 0;

nca_fio_t logfio;
int nca_logger_debug = 0;
caddr_t	nca_log_dir = "/var/nca/";
caddr_t	nca_current_log = "current";


typedef struct {
	uint_t			tid;
	uint32_t		n_used;	/* # of bytes of buffer used */
	char			*buf;
} http_buf_table_t;

static http_buf_table_t *g_http_buf_table = NULL;

static n_http_buf_table;	/* Initialized in http_init() */
uint32_t g_log_upcall_failed_msg = 0;

static timeout_id_t nca_logit_flush_tid;

/* max size of data sent to httpd via door upcall */
/* XXX - make configurable ? */
static uint32_t n_http_buf_size = NCA_IO_MAX_SIZE;
static uint32_t n_http_chunk_size = NCA_IO_MAX_SIZE - sizeof (nca_io_t);

/* routines */

static void log_buf_free(void *);
static void log_buf_alloc();
void nca_logger_init(void *);
static void nca_logger(void *, mblk_t *);
static void nca_http_logit(conn_t *);
static void nca_logit_flush();
static char *find_http_buffer();

static void node_del(node_t *, boolean_t);

squeue_t nca_log_squeue;

door_handle_t nca_httpd_door_hand = NULL;


/*
 * Given a ptr to a nca_io_t, a field and the field_length, write data
 * into buffer (Note: word aligned offsets).
 */
#define	NCA_IO_WDATA(val, vsize, p, n_used, len, off)		\
	/*CONSTCOND*/						\
	if ((val) == NULL) {					\
		(p)->len = vsize;				\
		(p)->off = 0;					\
	} else {						\
		(p)->len = (vsize);				\
		(p)->off = ((n_used) + sizeof (uint32_t) - 1) &	\
				(~(sizeof (uint32_t) - 1));	\
		bcopy((char *)(val),				\
			((char *)(p) + sizeof (nca_io_t) + (p)->off),\
			(vsize));				\
		(n_used) = (p)->off + (p)->len;			\
	}

/*
 * Given a ptr to an nca_io_t, a field length member name, append data to
 * it in the buffer. Note: must be the last field a WDATA() was done for.
 *
 * Note: a NULL NCA_IO_WDATA() can be followed by a NCA_IO_ADATA() only if
 *		vsize was == -1.
 *
 */
#define	NCA_IO_ADATA(val, vsize, p, n_used, len, off)		\
	if ((p)->len == -1) {					\
		(p)->len = 0;					\
		(p)->off = ((n_used) + sizeof (uint32_t) - 1) &	\
		(~(sizeof (uint32_t) - 1));			\
	}							\
	bcopy((char *)(val), ((char *)(p) + sizeof (nca_io_t) +	\
		(p)->off + (p)->len), (vsize));			\
		(p)->len += (vsize);				\
		(n_used) += (vsize);


static int nca_door_upcall_init(door_handle_t *, char *);
static int nca_door_upcall(door_handle_t *, char *, int, int *, squeue_t *);


static int nca_miss_threads1 = 0;	/* Miss threads for miss_fanout1 */
static int nca_miss_threads2 = 0;	/* Miss threads for miss_fanout2 */
static sqfan_t nca_miss_fanout1;	/* Cache miss squeue_t fanout */
static sqfan_t nca_miss_fanout2;	/* Cache map miss squeue_t fanout */

static void nca_miss_init(void *);
static void nca_missed(node_t *, mblk_t *, squeue_t *);

static struct kmem_cache *node_cache;

/*
 * A lrub (LRU Bucket) is used to maintain a LRU list of node_t(s)
 * with a node_t.size <= lrub_t.size. This is done to minimize Virt
 * address fragmentation.
 *
 * The nca_lru is a vector of lrb_s's by size in ascending order.
 *
 * Note: the size member must be the first member as we use this
 * fact for an abbreviated array initialization, this also assumes
 * that the default zero initialization of all other members is
 * accurate (we can guarantee this for all members but lock and
 * depend on this being true (which it currently is)).
 */
typedef struct lrub_s {
	int	size;		/* Max size of node_t.size */
	node_t	*phead;		/* Phys LRU list head (MRU) */
	node_t	*ptail;		/* Phys LRU list tail (LRU) */
	node_t	*vhead;		/* Virt LRU list head (MRU) */
	node_t	*vtail;		/* Virt LRU list tail (LRU) */

	uint32_t pcount;	/* Phys count of node_t members */
	uint32_t vcount;	/* Virt count of node_t members */

	kmutex_t lock;		/* Guarantee atomic access of above */
} lrub_t;

static lrub_t	nca_lru[] = {{302}, {404}, {506}, {608}, {710}, {812},
	{914}, {1016}, {1118}, {1224}, {2248}, {3272}, {4296}, {5320},
	{6344}, {7368}, {8392}, {9416}, {10440}, {20680}, {30920},
	{41160}, {51400}, {61640}, {71880}, {82120}, {92360}, {102600},
	{205000}, {307400}, {409800}, {512200}, {614600}, {717000},
	{819400}, {921800}};

/*
 * Given a node_t.size value in sz walk nca_lru using the given
 * lrub_t *p until a bucket large enough is found.
 */

#define	LRU_BP(sz, p) {							\
	(p) = nca_lru;							\
	while (sz > (p)->size) {					\
		if (++(p) == &nca_lru[sizeof (nca_lru) / sizeof (lrub_t)]) { \
			(p)--;						\
			break;						\
		}							\
	}								\
}

/*
 * Given a lrub_t *p return the next lrub_t *p.
 */

#define	LRU_NEXT_BP(p) {						\
	if (++(p) == &nca_lru[sizeof (nca_lru) / sizeof (lrub_t)]) {	\
		(p) = NULL;						\
	}								\
}

/*
 * Given a lrub_t *p return the previous lrub_t *p.
 */

#define	LRU_PREV_BP(p) {						\
	if (--(p) < nca_lru) {						\
		(p) = NULL;						\
	}								\
}

/*
 * URI and filename hash, a simple static hash bucket array of singly
 * linked grounded lists is used with a hashing algorithm which has
 * proven to have good distribution properities for strings of ...
 *
 * Note: HASH_SZ must be a prime number and should be the nearest prime
 *	 less then a power of 2.
 */

#define	HASH_SZ 4097

static nodef_t urihash[HASH_SZ];
static nodef_t filehash[HASH_SZ];

#define	HASH_IX(s, l, hix) { \
	char *cp = s; \
	int len = l; \
			\
	cp = &cp[len - 2]; \
	hix = 0; \
	while (cp > s) { \
		hix <<= 1; \
		hix += (cp[0] << 7) + cp[1]; \
		cp -= 2; \
	} \
	hix %= HASH_SZ; \
}


/*
 * NCA HTTP date routines:
 */

static char *dow[] = {"sunday", "monday", "tuesday", "wednsday", "thursday",
	"friday", "saturday", 0};

static char *Dow[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed", 0};

static char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	"Aug", "Sep", "Oct", "Nov", "Dec", 0};

static int dom[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/*
 * http_today(char *) - returns in the given char* pointer the current
 * date in ascii with a format of (char [29]):
 *
 *	Sun, 07 Dec 1998 14:49:37 GMT	; RFC 822, updated by RFC 1123
 */

static void
nca_http_today(char *cp)
{
	ssize_t	i;
	char	*fp;

	ssize_t	leap;
	ssize_t	year;
	ssize_t	month;
	ssize_t	dow;
	ssize_t	day;
	ssize_t	hour;
	ssize_t	min;
	ssize_t	sec;

	/* Secs since Thu, 01 Jan 1970 00:00:00 GMT */
	time_t	now = hrestime.tv_sec;

	sec = now % 60;
	now /= 60;
	min = now % 60;
	now /= 60;
	hour = now % 24;
	now /= 24;
	dow = now % 7;

	year = 1970;
	for (;;) {
		if (year % 4 == 0 && year % 100 != 0 || year % 400 == 0)
			day = 366;
		else
			day = 365;
		if (now < day)
			break;
		now -= day;
		year++;
	}

	now++;
	if (year % 4 == 0 && year % 100 != 0 || year % 400 == 0)
		leap = 1;
	else
		leap = 0;
	month = 11;
	for (i = 11; i; i--) {
		if (i < 2)
			leap = 0;
		if (now > dom[i] + leap)
			break;
		month--;
	}
	day = now - dom[i] - leap;

	fp = Dow[dow];
	*cp++ = *fp++;
	*cp++ = *fp++;
	*cp++ = *fp++;
	*cp++ = ',';
	*cp++ = ' ';

	i = day / 10;
	*cp++ = '0' + i;
	*cp++ = '0' + (day - i * 10);
	*cp++ = ' ';

	fp = months[month];
	*cp++ = *fp++;
	*cp++ = *fp++;
	*cp++ = *fp++;
	*cp++ = ' ';

	i = year / 1000;
	*cp++ = '0' + i;
	year -= i * 1000;
	i = year / 100;
	*cp++ = '0' + i;
	year -= i * 100;
	i = year / 10;
	*cp++ = '0' + i;
	year -= i * 10;
	*cp++ = '0' + year;
	*cp++ = ' ';

	i = hour / 10;
	*cp++ = '0' + i;
	*cp++ = '0' + (hour - i * 10);
	*cp++ = ':';

	i = min / 10;
	*cp++ = '0' + i;
	*cp++ = '0' + (min - i * 10);
	*cp++ = ':';

	i = sec / 10;
	*cp++ = '0' + i;
	*cp++ = '0' + (sec - i * 10);
	*cp++ = ' ';

	*cp++ = 'G';
	*cp++ = 'M';
	*cp = 'T';
}

/*
 * time_t http_date(const char *) - returns the time(2) value (i.e.
 * the value 0 is Thu, 01 Jan 1970 00:00:00 GMT) for the following
 * time formats used by HTTP request and response headers:
 *
 *	1) Sun, 07 Dec 1998 14:49:37 GMT	; RFC 822, updated by RFC 1123
 *	2) Sunday, 07-Dec-98 14:49:37 GMT	; RFC 850, obsoleted by RFC 1036
 *	3) Sun Nov  7 14:49:37 1998		; ANSI C's asctime() format
 *	4) 60					; Time delta of N seconds
 *
 * Note, on error a time_t value of -1 is returned.
 *
 * Also, all dates are GMT (must be part of the date string for types
 * 1 and 2 and not for type 1).
 *
 */

static time_t
nca_http_date(char *date)
{
	time_t	secs;
	char	*dp = date;
	char	**cpp;
	char	*cp;
	char	c;
	ssize_t	n;

	ssize_t	zeroleap = 1970 / 4 - 1970 / 100 + 1970 / 400;
	ssize_t	leap;
	ssize_t	year;
	ssize_t	month;
	ssize_t	day;
	ssize_t	hour;
	ssize_t	min;
	ssize_t	sec;

	/* Parse day-of-week (we don't actually use it) */
	cpp = dow;
	cp = *cpp++;
	dp = date;
	n = 0;
	while ((c = *dp++) != 0) {
		if (c == ',' || c == ' ') {
			if (dp - date > 3)
				n = cpp - dow;
			break;
		}
		c = tolower(c);
		if (*cp == 0 || *cp != c) {
			dp = date;
			if ((cp = *cpp++) == 0)
				break;
			continue;
		}
		cp++;
	}
	if (n == 0) {
		/* Not case 1-3, try 4 */
		dp = date;
		while ((c = *dp++) != 0) {
			if (isdigit(c)) {
				n *= 10;
				n += c - '0';
				continue;
			}
			/* An invalid date sytax */
			return (-1);
		}
		if (dp - date == 1)
			return (-1);
		/* case 4, add to current time */
		return (hrestime.tv_sec + n);
	}
	c = *dp++;
	if (c == 0)
		return (-1);
	if (c == ' ') {
		/* case 1 or 2 */
		if ((c = *dp++) != 0 && isdigit(c)) {
			n = c - '0';
			if ((c = *dp++) != 0 && isdigit(c)) {
				n *= 10;
				n = c - '0';
			} else
				return (-1);
		} else
			return (-1);
		day = n;
		if ((c = *dp++) == 0 || (c != ' ' && c != '-'))
			return (-1);
		cpp = months;
		cp = *cpp++;
		date = dp;
		n = 0;
		while ((c = *dp++) != 0) {
			if (c == ' ' || c == '-') {
				if (dp - date > 3)
					n = cpp - months;
				break;
			}
			c = tolower(c);
			if (*cp == 0 || tolower(*cp) != c) {
				dp = date;
				if ((cp = *cpp++) == 0)
					break;
				continue;
			}
			cp++;
		}
		if (n == 0)
			return (-1);
		month = n;
		if (c == ' ') {
		/* case 1 */
			if ((c = *dp++) == 0 || ! isdigit(c))
				return (-1);
			n = c - '0';
			if ((c = *dp++) == 0 || ! isdigit(c))
				return (-1);
			n *= 10;
			n += c - '0';
			if ((c = *dp++) == 0 || ! isdigit(c))
				return (-1);
			n *= 10;
			n += c - '0';
			if ((c = *dp++) == 0 || ! isdigit(c))
				return (-1);
			n *= 10;
			n += c - '0';
		} else {
		/* case 2 */
			if ((c = *dp++) != 0 && isdigit(c)) {
				n = c - '0';
				if ((c = *dp++) != 0 && isdigit(c)) {
					n *= 10;
					n += c - '0';
				} else
					return (-1);
			} else
				return (-1);
			/*
			 * KLUDGE: sense we no that this is a so-called Unix
			 * date format and the begining of time is 1970 then
			 * we can extend this obsoleted date syntax past the
			 * year 1999 into the year 2038 for 32 bit machines
			 * and through 2999 for 64 bit (and greater) machines.
			 */
			if (n > 69)
				n += 1900;
			else
				n += 2000;
		}
		year = n;
		if ((c = *dp++) == 0 || c != ' ')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		hour = n;
		if ((c = *dp++) == 0 || c != ':')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		min = n;
		if ((c = *dp++) == 0 || c != ':')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		sec = n;
		if ((c = *dp++) == 0 || c != ' ')
			return (-1);
		if ((c = *dp++) == 0 || c != 'G')
			return (-1);
		if ((c = *dp++) == 0 || c != 'M')
			return (-1);
		if ((c = *dp++) == 0 || c != 'T')
			return (-1);
	} else if (isalpha(c)) {
		/* case 3 */
		dp--;
		cpp = months;
		cp = *cpp++;
		date = dp;
		n = 0;
		while ((c = *dp++) != 0) {
			if (c == ' ') {
				if (dp - date > 3)
					n = cpp - months;
				break;
			}
			c = tolower(c);
			if (*cp == 0 || *cp != c) {
				dp = date;
				if ((cp = *cpp++) == 0)
					break;
				continue;
			}
			cp++;
		}
		if (n == 0)
			return (-1);
		month = n;
		if ((c = *dp++) != 0) {
			if (isdigit(c))
				n = c - '0';
			else if (c == ' ')
				n = 0;
			else
				return (-1);
		}
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n = c - '0';
		day = n;
		if ((c = *dp++) == 0 || c != ' ')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		hour = n;
		if ((c = *dp++) == 0 || c != ':')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		min = n;
		if ((c = *dp++) == 0 || c != ':')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		sec = n;
		if ((c = *dp++) == 0 || c != ' ')
			return (-1);
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n = c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		if ((c = *dp++) == 0 || ! isdigit(c))
			return (-1);
		n *= 10;
		n += c - '0';
		year = n;
	} else
		return (-1);

	leap = year;
	if (month < 3)
		leap--;
	leap = leap / 4 - leap / 100 + leap / 400 - zeroleap;
	secs = ((((year - 1970) * 365
		+ dom[month - 1] + day - 1 + leap) * 24
	+ hour) * 60
		+ min) * 60
	+ sec;
	return (secs);
}

/*
 * In-line NCA HTTP responses:
 */

static char nca_resp_304[] =
	"HTTP/#.# 304 Not Modified\r\n"
	"Date: #############################\r\n"
	"Server: NCA/#.# (Solaris)\r\n"
	"\r\n";

static char nca_resp_400[] =
	"HTTP/#.# 400 Bad Request\r\n"
	"Date: #############################\r\n"
	"Server: NCA/#.# (Solaris)\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	"<HTML><HEAD>\r\n"
	"<TITLE>400 Bad Request</TITLE>\r\n"
	"</HEAD><BODY>\r\n"
	"<H1>Bad Request</H1>\r\n"
	"Your browser sent a request that\r\n"
	"this server could not understand.<P>\r\n"
	"</BODY></HTML>\r\n"
	"\r\n";

static char nca_resp_500[] =
	"HTTP/#.# 500 Internal Server Error\r\n"
	"Date: #############################\r\n"
	"Server: NCA/#.# (Solaris)\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	"<HTML><HEAD>\r\n"
	"<TITLE>500 Internal Server Error</TITLE>\r\n"
	"</HEAD><BODY>\r\n"
	"<H1>Internal Server Error</H1>\r\n"
	"The server is currently unable\r\n"
	"to handle your request.<P>\r\n"
	"</BODY></HTML>\r\n"
	"\r\n";

static char nca_resp_501[] =
	"HTTP/#.# 501 Not Implemented\r\n"
	"Date: #############################\r\n"
	"Server: NCA/#.# (Solaris)\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	"<HTML><HEAD>\r\n"
	"<TITLE>501 Not Implemented</TITLE>\r\n"
	"</HEAD><BODY>\r\n"
	"<H1>Not Implemented</H1>\r\n"
	"The server does not support the functionality\r\n"
	"required to fulfill the request.<P>\r\n"
	"</BODY></HTML>\r\n"
	"\r\n";

static char nca_resp_503[] =
	"HTTP/#.# 503 Service Unavailable\r\n"
	"Date: #############################\r\n"
	"Server: NCA/#.# (Solaris)\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	"<HTML><HEAD>\r\n"
	"<TITLE>503 Service Unavailable</TITLE>\r\n"
	"</HEAD><BODY>\r\n"
	"<H1>Service Unavailable</H1>\r\n"
	"The server is currently unable\r\n"
	"to handle your request.<P>\r\n"
	"</BODY></HTML>\r\n"
	"\r\n";

/*
 * Return a node_t for conn_t processing of an NCA in-line HTTP response.
 */

static node_t *
nca_http_response(node_t *np, conn_t *connp, char *resp, int respsz,
	char *path, int pathsz, uint_t version)
{
	char	*cp;
	int	kmflag = connp->inq->sq_isintr ? KM_NOSLEEP : KM_SLEEP;
	char	*msg = "???";

	char	*ep = &resp[respsz];
	char	*dp;

	int		kasz = 0;
	char		*kadata = NULL;
	char		connka[] = "\r\nConnection: Keep-Alive";
	char		connclose[] = "\r\nConnection: close";

	extern int nca_major_version;
	extern int nca_minor_version;

	if (np == NULL) {
		/* Allocate and initialize a node_t */
		np = (node_t *)kmem_cache_alloc(node_cache, kmflag);
		if (np == NULL)
			return (NULL);
		bzero(np, sizeof (*np));
		mutex_init(&np->lock, NULL, MUTEX_DEFAULT, NULL);
		np->path = path;
		np->pathsz = pathsz;
		np->frtn.free_func = node_fr;
		np->frtn.free_arg = (caddr_t)np;
		np->ref = REF_DONE|REF_SAFE|REF_PHYS|REF_VIRT|REF_KMEM;
		np->next = NULL;
		NCA_COUNTER(&nca_nodes, 1);
		np->mss = connp->tcp_mss;
		connp->req_np = np;
	} else {
		/* Caller provided a node_t, so use it */
		node_del(np, false);
		np->ref |= REF_DONE|REF_PHYS|REF_VIRT|REF_KMEM;
		path = np->path;
		pathsz = np->pathsz;
		version = np->version;
	}
	if (version > HTTP_1_0) {
		/*
		 * HTTP/1.1 or greater, so check for both
		 * "Keep-Alive" and "close" keywords.
		 */
		if (connp->http_persist) {
			kadata = connka;
			kasz = sizeof (connka) - 1;
		} else if (connp->http_was_persist) {
			kadata = connclose;
			kasz = sizeof (connclose) - 1;
		}
	} else if (version == HTTP_1_0) {
		/*
		 * HTTP/1.0, so check for "Keep-Alive" keyword.
		 */
		if (connp->http_persist) {
			/*
			 * No "Content-Lenght:" field and is persist,
			 * so not any more.
			 */
			connp->http_persist = 0;
			np->persist = 0;
		}
	}
	if (kadata != NULL) {
		respsz += kasz;
	}
	if ((dp = kmem_alloc(respsz, kmflag)) == NULL) {
		/*
		 * Set the size to -1 so that caller can
		 * deal with it.
		 */
		np->size = -1;
		return (NULL);
	}
	NCA_COUNTER((ulong_t *)&nca_kbmem, respsz);
	np->size = respsz;
	np->pp = (page_t **)dp;		/* Overloaded as data valid */
	np->data = dp;
	np->datasz = respsz;

	cp = dp;
	if (version >= HTTP_1_0) {
		/*
		 * Full response format.
		 *
		 * Copy to first sub char '#'.
		 */
		while (resp < ep) {
			if (*resp == '#')
				break;
			*cp++ = *resp++;
		}

		/* Process the HTTP version substitutions */
		if (*resp != '#') {
			msg = "first";
			goto bad;
		}
		*cp++ = '0' + (version >> 16);
		resp++;
		while (resp < ep) {
			if (*resp == '#')
				break;
			*cp++ = *resp++;
		}
		if (*resp != '#') {
			msg = "HTTP minor";
			goto bad;
		}
		*cp++ = '0' + (version & 0xFFFF);
		resp++;

		/* Copy to the next sub char '#' */
		while (resp < ep) {
			if (*resp == '#')
				break;
			*cp++ = *resp++;
		}

		/* Process the "Date: " substitution */
		if (*resp != '#') {
			msg = "date";
			goto bad;
		}
		nca_http_today(cp);

		/* Skip to the next nonsub char '#' */
		while (resp < ep) {
			if (*resp != '#')
				break;
			cp++;
			resp++;
		}

		/* Copy to the next sub char '#' */
		while (resp < ep) {
			if (*resp == '#')
				break;
			*cp++ = *resp++;
		}

		/* Process the NCA version substitutions */
		if (*resp != '#') {
			msg = "NCA major";
			goto bad;
		}
		*cp++ = '0' + nca_major_version;
		resp++;
		while (resp < ep) {
			if (*resp == '#')
				break;
			*cp++ = *resp++;
		}
		if (*resp != '#') {
			msg = "NCA minor";
			goto bad;
		}
		*cp++ = '0' + nca_minor_version;
		resp++;
	} else {
		/*
		 * Simple response format
		 *
		 * Skip to end-of-header.
		 */
		int n = 0;

		while (resp < ep) {
			if (n == 0 && *resp == '\r')
				n++;
			else if (n == 1 && *resp == '\n')
				n++;
			else if (n == 2 && *resp == '\r')
				n++;
			else if (n == 3 && *resp == '\n')
				break;
			else
				n = 0;
			resp++;
		}
		/* Adjust node_t data size */
		np->datasz = ep - resp;
	}

	/* Copy to the end */
	while (resp < ep) {
		*cp++ = *resp++;
	}

	/* Do any kadata to append */
	if (kadata != NULL) {
		cp -= 2;
		resp -= 2;
		while (kasz--) {
			*cp++ = *kadata++;
		}
		*cp++ = *resp++;
		*cp++ = *resp++;
	}

	return (np);
bad:
	cmn_err(CE_WARN, "nca_http_response: bad substitution \"%s\"", msg);
	return (np);
}

static uint16_t
tcp_cksum(uint8_t *kaddr, size_t cnt)
{
	uint32_t psum = 0;
	int odd = (cnt & 1l) == 1l;
	extern unsigned int ip_ocsum(ushort_t *, int, unsigned int);


	if (((uintptr_t)kaddr & 0x1) != 0) {
		/*
		 * Kaddr isn't 16 bit aligned.
		 */
		unsigned int tsum;

#ifdef _LITTLE_ENDIAN
		psum += *kaddr;
#else
		psum += *kaddr << 8;
#endif
		cnt--;
		kaddr++;
		tsum = ip_ocsum((ushort_t *)kaddr, cnt >> 1, 0);
		psum += (tsum << 8) & 0xffff | (tsum >> 8);
		if (cnt & 1) {
			kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
			psum += *kaddr << 8;
#else
			psum += *kaddr;
#endif
		}
	} else {
		/*
		 * Kaddr is 16 bit aligned.
		 */
		psum = ip_ocsum((ushort_t *)kaddr, cnt >> 1, psum);
		if (odd) {
			kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
			psum += *kaddr;
#else
			psum += *kaddr << 8;
#endif
		}
	}
	psum = (psum >> 16) + (psum & 0xFFFF);
	return ((uint16_t)psum);
}

static void
node_fr(node_t *np)
{
	node_t	*head = NULL;
	uint32_t ref;
	boolean_t safed = false;

	NCA_DEBUG_COUNTER(&nca_desballoc, -1);
	NCA_COUNTER((ulong_t *)&nca_mbmem, -(sizeof (mblk_t)+sizeof (dblk_t)));

	/*
	 * Note: We can access np->hashfanout without holding np->lock
	 *	 because this field changes only in node_add() and here.
	 */
	if (np->hashfanout != NULL) {
		/*
		 * Node is still hashed, so it is reachable by other
		 * threads.  Lock it to get exclusive access; we will
		 * drop this lock before any calls into the rest of
		 * the kernel.
		 */
		head = np;
		mutex_enter(&head->lock);
	}

	ref = --np->ref;
	if ((ref & REF_CNT) != 0) {
		if (head != NULL)
			mutex_exit(&head->lock);
		return;
	}

	/*
	 * Last reference to our self-referencing desballoc()'d node_t.
	 *
	 * At best, we can free physical memory (REF_PHYS), virtual
	 * memory (REF_VIRT), mblk-related memory (REF_MBLK), and the
	 * memory associated with the node_t itself.
	 *
	 * The node_t can be freed whenever it is no longer accessible
	 * via hash (i.e., hashfanout is NULL) and the reference count
	 * on the node_t has dropped to zero (to get here, the latter
	 * condition has already been met).
	 *
	 * If the node_t can be freed, then all resources can be
	 * reclaimed.  This is the simple case; there are no race
	 * conditions to worry about because the node_t is no longer
	 * reachable.
	 *
	 * If the node_t cannot be freed (i.e., it is still accessible
	 * via hash), then only REF_* resources that are *not set* in
	 * the node's `ref' flags can be reclaimed.  For instance, if
	 * REF_VIRT is not set in the node_t's `ref' flags, then the
	 * node's REF_VIRT resources are not being referenced and can
	 * be reclaimed (freed).
	 *
	 * Note that in the latter case, it is possible that while
	 * this thread was freeing resources, this node_t was
	 * discovered by another thread via the hash.  In this case,
	 * when this thread is done reclaiming what resources it can,
	 * it must then kickoff miss processing on the node_t.
	 *
	 * The rationale for the latter case is performance: in the
	 * case where only virtual memory is scarce, it makes little
	 * sense to free the node_t entirely and invalidate its
	 * physical mappings if only to map it in again later.
	 *
	 * There are three common paths that lead here:
	 *
	 *	1. Physical memory was scarce, and this node_t was
	 *	   chosen for reclaim by nca_reclaim_phys().  Since
	 *	   all resources need reclaiming in this case,
	 *	   nca_reclaim_phys() called node_del() to remove the
	 *	   node_t from the URI hash and decremented the
	 *	   reference count on the node_t.  Eventually, the
	 *	   reference count on the node_t reached zero.
	 *
	 *	2. Virtual memory was scarce, and this node_t was
	 *	   chosen for reclaim by nca_reclaim_vlru().  Since
	 *	   only virtual memory needed reclaiming, the node
	 *	   was left in the URI hash but the reference count
	 *	   was decremented.  Eventually the reference count
	 *	   on the node_t reached zero.
	 *
	 *	3. The node was no longer cacheable (for whatever
	 *	   reason).  Thus node_del() was called to remove the
	 *	   node_t from the URI hash and eventually the
	 *	   reference count on the node_t reached zero.
	 */

	for (;;) {
		node_t	*nnp = np->next;
		page_t	**pp = np->pp;
		char	*dp = np->data;
		uint16_t *cksum = np->cksum;
		boolean_t unlocked = false;

		if (nca_debug_counter) {
			/*
			 * The nca_ref[] counters keep track of what
			 * resources the node_t still has referenced.
			 * Each resource is assigned a bit in an
			 * integer index (REF_PHYS is bit 1, REF_VIRT
			 * is bit 2, and REF_MBLK is bit 3).
			 *
			 * For instance, to see how many node_t's
			 * still had REF_VIRT and REF_MBLK set, check
			 * binary index 110, which is decimal index 6.
			 */
			int i = 0;
			if (ref & REF_PHYS)
				i |= (1 << 0);
			if (ref & REF_VIRT)
				i |= (1 << 1);
			if (ref & REF_MBLK)
				i |= (1 << 2);
			NCA_COUNTER(&nca_ref[i], 1);
		}

		if (head != NULL) {
			if ((ref & (REF_PHYS|REF_VIRT|REF_MBLK)) !=
			    (REF_PHYS|REF_VIRT|REF_MBLK)) {
				/*
				 * Node's still hashed and we're going
				 * to remove a resource (i.e., we're
				 * going to have to call another part
				 * of the kernel).  Keep other threads
				 * from finding the resources we're
				 * gonna free, then drop the lock.
				 */
				np->ref &= ~REF_KMEM;

				if ((ref & REF_MBLK) == 0)
					np->cksum = NULL;

				if ((ref & REF_VIRT) == 0) {
					head->ref |= REF_SAFE;
					safed = true;
					np->data = NULL;
				}

				if ((ref & REF_PHYS) == 0)
					np->pp = NULL;

				mutex_exit(&head->lock);
				unlocked = true;
			}
		} else {
			/*
			 * Not hashed, so on last ref everthing is freed.
			 */
			np->ref &= ~(REF_PHYS|REF_VIRT|REF_MBLK);
			ref = np->ref;
			NCA_DEBUG_COUNTER(&nca_rnh, 1);
		}

		if ((ref & REF_MBLK) == 0) {
			if (cksum != NULL) {
				NCA_COUNTER((ulong_t *)&nca_mbmem, -*cksum);
				kmem_free(cksum, *cksum);
				NCA_DEBUG_COUNTER(&nca_rmdone, 1);
			} else
				NCA_DEBUG_COUNTER(&nca_rmfail, 1);
		}
		if (ref & REF_KMEM) {
			if ((ref & (REF_PHYS|REF_VIRT)) == 0) {
				/*
				 * kmem_alloc()'d data; just free dp.
				 */
				kmem_free(dp, np->datasz);
				NCA_DEBUG_COUNTER(&nca_rkdone, 1);
				NCA_COUNTER((ulong_t *)&nca_kbmem,
				    -(np->datasz));
			} else
				NCA_DEBUG_COUNTER(&nca_rkfail, 1);
		} else {
			/*
			 * kmem_phys_alloc()'d data which may have been
			 * kmem_phys_mapin()'d.
			 */
			if ((ref & REF_VIRT) == 0) {
				if (dp != NULL) {
					kmem_phys_mapout(pp, dp);
					NCA_DEBUG_COUNTER(&nca_rvdone, 1);
					NCA_COUNTER(&nca_vpmem,
						-(btopr(np->datasz)));
				} else
					NCA_DEBUG_COUNTER(&nca_rvfail, 1);
			}
			if ((ref & REF_PHYS) == 0) {
				if (pp != NULL) {
					kmem_phys_free(pp);
					NCA_DEBUG_COUNTER(&nca_rpdone, 1);
					NCA_COUNTER(&nca_ppmem,
						-(btopr(np->datasz)));
				} else
					NCA_DEBUG_COUNTER(&nca_rpfail, 1);
			}
		}
		if (head == NULL) {
			/* Free any node_t resources prior to freeing it */
			node_t	*fnp = np->fileback;

			if (np->req != NULL)
				/* freeb() the request header data */
				freeb(np->req);
			if (fnp != NULL) {
				/*
				 * The node_t's a member of a file list,
				 * so find it in the list and delete it.
				 */
				node_t	*cfnp;
				node_t	*pfnp = fnp;

				mutex_enter(&fnp->lock);
				cfnp = fnp->filenext;
				while (cfnp != np) {
					pfnp = cfnp;
					cfnp = cfnp->filenext;
				}
				pfnp->filenext = cfnp->filenext;
				mutex_exit(&fnp->lock);
			}
			kmem_cache_free(node_cache, np);
			NCA_DEBUG_COUNTER(&nca_rndone, 1);
			NCA_COUNTER(&nca_nodes, -1);
		} else {
			if (unlocked) {
				mutex_enter(&head->lock);

				/*
				 * See if the node_t was referenced
				 * by another thread while we were
				 * away; if so, no need to continue.
				 */
				if ((head->ref & REF_CNT) != 0)
					break;
			}
		}
		if ((np = nnp) == NULL)
			break;
		ref = np->ref;
	}
	if (head != NULL) {
		if (safed) {
			/*
			 * The node_t head was marked SAFE buy us, so
			 * walk the conn_t list and move any conn_t's
			 * not for us to the appropriate node_t's list
			 * and process.
			 *
			 * Then if any conn_t(s) left process.
			 */
			mblk_t	*mp;
			conn_t	*connp = head->connhead;
			conn_t	*nconnp;
			conn_t	*pconnp = NULL;

			head->ref &= ~REF_SAFE;
			while (connp != NULL) {
				if ((np = connp->req_np) != head) {
					/* Remove from current list */
					if ((nconnp = connp->nodenext) == NULL)
						head->conntail = NULL;
					if (pconnp == NULL)
						head->connhead = nconnp;
					else
						pconnp->nodenext = nconnp;
					/* Add to correct list and process */
					connp->nodenext = NULL;
					if (np->connhead != NULL)
						np->conntail->nodenext = connp;
					else
						np->connhead = connp;
					np->conntail = connp;
					mp = dupb(connp->req_mp);
					sqfan_fill(&nca_miss_fanout2, mp, np);
					connp = nconnp;
				} else {
					pconnp = connp;
					connp = connp->nodenext;
				}
			}
			if ((connp = head->connhead) != NULL) {
				mp = dupb(connp->req_mp);
				sqfan_fill(&nca_miss_fanout2, mp, head);
			}
		}
		mutex_exit(&head->lock);
	}
}

static node_t *
node_add(char *path, int pathsz, nodef_t *hashv, int kmflag)
{
	unsigned hix;
	node_t	*np;
	uint32_t ref;

	if ((np = (node_t *)kmem_cache_alloc(node_cache, kmflag)) == NULL)
		return (NULL);

	if (hashv == urihash)
		ref = REF_URI | REF_SAFE;
	else if (hashv == filehash)
		ref = REF_FILE | REF_SAFE;

	bzero(np, sizeof (*np));
	mutex_init(&np->lock, NULL, MUTEX_DEFAULT, NULL);
	np->size = -1;
	np->path = path;
	np->pathsz = pathsz;
	np->frtn.free_func = node_fr;
	np->frtn.free_arg = (caddr_t)np;
	mutex_enter(&np->lock);
	if (hashv != NULL) {
		np->ref = ref;
		HASH_IX(np->path, np->pathsz, hix);
		np->hashfanout = &hashv[hix];
		np->hashnext = hashv[hix].head;
		hashv[hix].head = np;
	} else {
		np->ref = REF_SAFE;
	}
	NCA_COUNTER(&nca_nodes, 1);
	return (np);
}

static void
node_del(node_t *np, boolean_t locked)
{
	nodef_t	*fp = np->hashfanout;
	node_t	*p;
	node_t	*pp;
	lrub_t	*bp;

	if (fp == NULL)
		/* node_del pending */
		return;

	if (! locked) {
		mutex_enter(&fp->lock);
		mutex_enter(&np->lock);
	}
	pp = NULL;
	p = fp->head;
	while (p != np) {
		pp = p;
		p = p->hashnext;
	}
	if (pp)
		pp->hashnext = p->hashnext;
	else
		fp->head = p->hashnext;

	np->hashfanout = NULL;
	if (! locked)
		mutex_exit(&fp->lock);

	if ((bp = np->bucket) != NULL && (np->ref & (REF_PHYS|REF_VIRT))) {
		mutex_enter(&bp->lock);
		if (np->ref & REF_PHYS) {
			if (np->plrunn != NULL)
				np->plrunn->plrupn = np->plrupn;
			else
				bp->ptail = np->plrupn;
			if (np->plrupn != NULL)
				np->plrupn->plrunn = np->plrunn;
			else
				bp->phead = np->plrunn;
			np->ref &= ~REF_PHYS;
			bp->pcount -= 1;
			NCA_DEBUG_COUNTER(&nca_plrucnt, -1);
		}
		if (np->ref & REF_VIRT) {
			if (np->vlrunn != NULL)
				np->vlrunn->vlrupn = np->vlrupn;
			else
				bp->vtail = np->vlrupn;
			if (np->vlrupn != NULL)
				np->vlrupn->vlrunn = np->vlrunn;
			else
				bp->vhead = np->vlrunn;
			np->ref &= ~REF_VIRT;
			bp->vcount -= 1;
			NCA_DEBUG_COUNTER(&nca_vlrucnt, -1);
		}
		mutex_exit(&bp->lock);
	}
	mutex_exit(&np->lock);
}

/*
 * Add a node to the tail of a singly linked list of node_t's.
 */
static node_t *
node_add_next(node_t *head)
{
	node_t	*tail;
	node_t	*np;

	np = (node_t *)kmem_cache_alloc(node_cache, KM_NOSLEEP);

	/* XXX - Need to recover from np == NULL */
	bzero(np, sizeof (*np));
	np->back = head;
	if ((tail = head->back) != NULL)
		tail->next = np;
	else
		head->next = np;
	head->back = np;
	np->size = -1;
	NCA_COUNTER(&nca_nodes, 1);
	return (np);
}

void
nca_http_init(void)
{
	int		count;
	sqfan_t		*sqfp;

	/*
	 * Ideally, since NCA is designed to operate on dedicated
	 * servers, we'd like to consume as much memory as possible.
	 * However, the maximum amount of memory that can be consumed
	 * before starving the kernel depends loosely on the number
	 * of cpus, the speed of those cpus, and other hardware
	 * characteristics, and is thus highly machine-dependent.
	 *
	 * So, until we have a backpressure (reclaim) system in place
	 * we'll take the cheap way out and only consume at most
	 * 25% of memory.  Note that this will be inadequate for many
	 * configurations; in those cases, these parameters will have
	 * to be increased via ndd(1M).
	 */

	nca_maxkmem = kmem_maxavail();
	if (nca_vpmax == 0) {
		/* Use 25% of virt mem */
		nca_vpmax = btop(nca_maxkmem >> 2);
	}
	if (nca_ppmax == 0) {
		/* Use 25% of phys mem */
		nca_ppmax = (availrmem >> 2);
	}

	node_cache = kmem_cache_create("nca_node_cache", sizeof (node_t),
		0, NULL, NULL, NULL, NULL, NULL, 0);

	for (count = 0; count < HASH_SZ; count++) {
		urihash[count].head = NULL;
		mutex_init(&urihash[count].lock, NULL, MUTEX_DEFAULT, NULL);
	}

	for (count = 0; count < HASH_SZ; count++) {
		filehash[count].head = NULL;
		mutex_init(&filehash[count].lock, NULL, MUTEX_DEFAULT, NULL);
	}

	/* upcall miss thread(s) */
	if (nca_miss_threads1 == 0)
		nca_miss_threads1 = CPUS * 4;
	sqfp = &nca_miss_fanout1;
	sqfan_init(sqfp, nca_miss_threads1, SQF_DIST_CNT, 0);
	/*
	 * Initialize the upcall miss door path name.  Note that here we
	 * assume that MISS_DOOR_FILE is shorter than PATH_MAX so that we
	 * can copy it into nca_httpd_door_path without checking...
	 */
	bcopy(MISS_DOOR_FILE, nca_httpd_door_path, strlen(MISS_DOOR_FILE) + 1);
	for (count = 0; count < nca_miss_threads1; count++) {
		(void) sqfan_ixinit(sqfp, count, NULL, SQT_DEFERRED, NULL,
			nca_miss_init, NULL, nca_missed, 0, maxclsyspri);
	}
	if (nca_debug)
		printf("nca miss upcalls serviced by %d thread(s)\n", count);

	/* map miss thread(s) */
	if (nca_miss_threads2 == 0)
		nca_miss_threads2 = CPUS;
	sqfp = &nca_miss_fanout2;
	sqfan_init(sqfp, nca_miss_threads2, SQF_DIST_CNT, 0);
	for (count = 0; count < nca_miss_threads2; count++) {
		(void) sqfan_ixinit(sqfp, count, NULL, SQT_DEFERRED, NULL,
			nca_miss_init, NULL, nca_missed, 0, maxclsyspri);
	}
	if (nca_debug)
		printf("nca miss map serviced by %d thread(s)\n", count);

	/* logger thread */
	logbuf_cache = kmem_cache_create("nca_logbuf", sizeof (log_buf_t),
		0, NULL, NULL, NULL, NULL, NULL, 0);
	mutex_init(&logbuf_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_enter(&logbuf_lock);
	log_buf_alloc(KM_SLEEP);
	mutex_exit(&logbuf_lock);
	(void) squeue_init(&nca_log_squeue, SQT_DEFERRED, NULL, nca_logger_init,
				NULL, nca_logger, 0, maxclsyspri);
	/*
	 * Allocate a table of buffers i.e. one per NCA upcall miss thread
	 * Each thread uses it's own buffer for upcalls to the HTTP server
	 */

	if (!g_http_buf_table) {
		http_buf_table_t *p;

		n_http_buf_table = nca_miss_threads1;
		g_http_buf_table = kmem_alloc((n_http_buf_table *
						sizeof (http_buf_table_t)),
						KM_SLEEP);
		if (!g_http_buf_table) {
			printf("NCA: out of kernel memory\n");
			return;
		}
		p = g_http_buf_table;

		for (count = 0; count < n_http_buf_table; count++, p++) {
			if (!(p->buf =
				kmem_zalloc(n_http_buf_size, KM_SLEEP))) {
				printf("NCA: out of kernel memory\n");
				return;
			}
			p->tid = 0;	/* initialize */
			p->n_used = 0;
		}
	}
}

void
nca_http_fini(void)
{
	int		ix;
	node_t		*np;

	/* Note: this should really be done in logger_fini() !!! */
	if (nca_logit_flush_tid != 0) {
		(void) untimeout(nca_logit_flush_tid);
	}

	for (ix = 0; ix < n_http_buf_table; ix++) {
		kmem_free(g_http_buf_table[ix].buf, n_http_buf_size);
	}
	kmem_free(g_http_buf_table,
		n_http_buf_table * sizeof (http_buf_table_t));
	squeue_fini(&nca_log_squeue);

	if (logbuf != NULL) {
		kmem_cache_free(logbuf_cache, logbuf);
	}
	kmem_cache_destroy(logbuf_cache);
	mutex_destroy(&logbuf_lock);

	sqfan_fini(&nca_miss_fanout2);
	sqfan_fini(&nca_miss_fanout1);

	for (ix = 0; ix < HASH_SZ; ix++) {
		while ((np = urihash[ix].head) != NULL) {
			node_del(np, false);
			if (np->mp != NULL) {
				freeb(np->mp);
			}
		}
		mutex_destroy(&urihash[ix].lock);
	}

	for (ix = 0; ix < HASH_SZ; ix++) {
		while ((np = filehash[ix].head) != NULL) {
			node_del(np, false);
			if (np->mp != NULL) {
				freeb(np->mp);
			}
		}
		mutex_destroy(&filehash[ix].lock);
	}

	kmem_cache_destroy(node_cache);
}

/*
 * If we decide to continue to use then ppthresh needs to be made
 * dynamic (i.e. if maxkmem < physmem don't use kmem ?).
 */
#if defined(i386)
static pgcnt_t	nca_ppthresh = 8;
#else
static pgcnt_t	nca_ppthresh = 4;
#endif

/*
 * Reclaim a node_t from virt LRU.
 */

static boolean_t
nca_reclaim_vlru(size_t size)
{
	lrub_t	*bp;
	lrub_t	*sbp;
	node_t	*np;
	mblk_t	*mp;
	node_t	*head;
	boolean_t locked;

	NCA_DEBUG_COUNTER(&nca_rvcall, 1);

	/*
	 * Look for a reclaim candidate starting in the same size bucket,
	 * then in bigger buckets then in smaller buckets, then ???
	 *
	 * Note: REF_KMEM node_t's aren't virt reclaim candidates as they
	 *	 have no seperate virt and phys resources. However, they
	 *	 are phys reclaim candidates.
	 */
	LRU_BP(size, bp);
	sbp = bp;
	mutex_enter(&bp->lock);
	while ((np = bp->vtail) == NULL || (np->ref & REF_KMEM) != 0) {
		mutex_exit(&bp->lock);
		NCA_DEBUG_COUNTER(&nca_rvempty, 1);
		if (sbp != NULL) {
			LRU_NEXT_BP(bp);
		} else {
			LRU_PREV_BP(bp);
		}
		if (bp == NULL) {
			if ((bp = sbp) == NULL)
				return (false);
			sbp = NULL;
		}
		mutex_enter(&bp->lock);
	}
	sbp = bp;
	while ((locked = false, mutex_tryenter(&np->lock) == 0) ||
	    (locked = true, (np->ref & REF_SAFE))) {
		/*
		 * Can't lock the node_t or it's maked as SAFE, in the first
		 * case the owner of the node_t is either waiting on the LRU
		 * lock or soon will be and the node_t will be moved to the
		 * head of the LRU (i.e. MRU) so move on to the next oldest.
		 */
		if (locked)
			mutex_exit(&np->lock);
		NCA_DEBUG_COUNTER(&nca_rvbusy, 1);
		if ((np = np->vlrupn) == NULL) {
			/*
			 * No more node_t's for this bucket, so look in bigger
			 * buckets, then in smaller buckets, then ???
			 *
			 * Note: need to handle then no virt LRU case !!!
			 */
			do {
				mutex_exit(&bp->lock);
				NCA_DEBUG_COUNTER(&nca_rvempty, 1);
				if (sbp != NULL) {
					LRU_NEXT_BP(bp);
				} else {
					LRU_PREV_BP(bp);
				}
				if (bp == NULL) {
					if ((bp = sbp) == NULL)
						return (false);
					sbp = NULL;
				}
				mutex_enter(&bp->lock);
			} while ((np = bp->vtail) == NULL);
		}
	}
	/*
	 * We have a vlru reclaim node_t, so do VIRT recliam.
	 *
	 * Remove the node_t from the virt lru list then free the node_t
	 * mblk, when the last reference on the mblk is released node_fr()
	 * will finish the reclaim of the node_t's resources.
	 */
	np->ref |= REF_SAFE;
	if (np->vlrunn != NULL)
		np->vlrunn->vlrupn = np->vlrupn;
	else
		bp->vtail = np->vlrupn;
	if (np->vlrupn != NULL)
		np->vlrupn->vlrunn = np->vlrunn;
	else
		bp->vhead = np->vlrunn;
	bp->vcount -= 1;
	NCA_DEBUG_COUNTER(&nca_vlrucnt, -1);
	mutex_exit(&bp->lock);

	head = np;
	do {
		np->ref &= ~(REF_VIRT);
	} while ((np = np->next) != NULL);

	mp = head->mp;
	head->mp = NULL;
	mutex_exit(&head->lock);
	freemsg(mp);
	return (true);
}

/*
 * Reclaim a node_t from all lru list and free all resources.
 *
 * Note: the node_t's lock, containing lrub_t's lock,
 *	 and containing name hashfanout bucket's lock
 *	 must be held and will be released prior to return.
 */

static void
nca_reclaim_phys(node_t *np, lrub_t *bp)
{
	kmutex_t *lock = NULL;
	node_t	*head = np;
	mblk_t	*mp;

	if (bp != NULL) {
		/* Remove the node_t from the phys lru list. */
		if (np->plrunn != NULL)
			np->plrunn->plrupn = np->plrupn;
		else
			bp->ptail = np->plrupn;
		if (np->plrupn != NULL)
			np->plrupn->plrunn = np->plrunn;
		else
			bp->phead = np->plrunn;
		bp->pcount -= 1;
		NCA_DEBUG_COUNTER(&nca_plrucnt, -1);

		/* Remove the node_t from the virt lru list (if need be). */
		if (np->ref & REF_VIRT) {
			if (np->vlrunn != NULL)
				np->vlrunn->vlrupn = np->vlrupn;
			else
				bp->vtail = np->vlrupn;
			if (np->vlrupn != NULL)
				np->vlrupn->vlrunn = np->vlrunn;
			else
				bp->vhead = np->vlrunn;
			bp->vcount -= 1;
			NCA_DEBUG_COUNTER(&nca_vlrucnt, -1);
		}
		mutex_exit(&bp->lock);
	}

	/*
	 * Mark node_t(s) for reclaim of all resources, delete the
	 * head node_t from the name lookup hash, last free the head
	 * node_t's mblk, when the last reference on the mblk is released
	 * node_fr() will finish the reclaim of the node_t resources.
	 */
	do {
		np->ref &= ~(REF_PHYS | REF_VIRT | REF_MBLK);

	} while ((np = np->next) != NULL);

	mp = head->mp;
	head->mp = NULL;

	/*
	 * Try to acquire the name hashfanout bucket list lock that
	 * this head node_t is a member of. Note, the normal (i.e.
	 * name lookup) lock order is list lock then node_t lock.
	 */
	if (head->hashfanout != NULL) {
		lock = &head->hashfanout->lock;
		if (mutex_tryenter(lock) == 0) {
			/*
			 * Couldn't lock it, so to avoid dead-lock we have
			 * to drop our node_t lock and acquire both in the
			 * normal order.
			 */
			mutex_exit(&head->lock);
			mutex_enter(lock);
			mutex_enter(&head->lock);
		}
	}

	/* Note: node_del() exits the node_t lock */
	node_del(head, true);

	if (lock)
		mutex_exit(lock);

	freemsg(mp);
}

/*
 * Reclaim a node_t from phys LRU.
 */

static boolean_t
nca_reclaim_plru(size_t size)
{
	lrub_t	*bp;
	lrub_t	*sbp;
	node_t	*np;
	node_t	*fnp;
	boolean_t locked;

	NCA_DEBUG_COUNTER(&nca_rpcall, 1);

	/*
	 * Look for a reclaim candidate starting in the same size bucket,
	 * then in bigger buckets then in smaller buckets, then ???
	 */
	LRU_BP(size, bp);
	sbp = bp;
	mutex_enter(&bp->lock);
	while ((np = bp->ptail) == NULL) {
		mutex_exit(&bp->lock);
		NCA_DEBUG_COUNTER(&nca_rpempty, 1);
		if (sbp != NULL) {
			LRU_NEXT_BP(bp);
		} else {
			LRU_PREV_BP(bp);
		}
		if (bp == NULL) {
			if ((bp = sbp) == NULL)
				return (false);
			sbp = NULL;
		}
		mutex_enter(&bp->lock);
	}
	sbp = bp;
	while ((locked = false, mutex_tryenter(&np->lock) == 0) ||
	    (locked = true, (np->ref & REF_SAFE))) {
		/*
		 * Can't lock the node_t or it's maked as SAFE, in the first
		 * case the owner of the node_t is either waiting on the LRU
		 * lock or soon will be and the node_t will be moved to the
		 * head of the LRU (i.e. MRU) so move on to the next oldest.
		 */
		if (locked)
			mutex_exit(&np->lock);
	busy:;
		NCA_DEBUG_COUNTER(&nca_rpbusy, 1);
		if ((np = np->plrupn) == NULL) {
			/*
			 * No more node_t's for this bucket, so look in bigger
			 * buckets, then in smaller buckets, then ???
			 */
			do {
				mutex_exit(&bp->lock);
				NCA_DEBUG_COUNTER(&nca_rpempty, 1);
				if (sbp != NULL) {
					LRU_NEXT_BP(bp);
				} else {
					LRU_PREV_BP(bp);
				}
				if (bp == NULL) {
					if ((bp = sbp) == NULL)
						return (false);
					sbp = NULL;
				}
				mutex_enter(&bp->lock);
			} while ((np = bp->ptail) == NULL);
		}
	}
	np->ref |= REF_SAFE;
	if ((fnp = np->filenext) != NULL && np->fileback == NULL) {
		/*
		 * The node_t's a file node_t, so first walk it's
		 * file list and try to lock all member node_t's.
		 */
		while (fnp) {
			if ((locked = false, mutex_tryenter(&fnp->lock) == 0) ||
			    (locked = true, (fnp->ref & REF_SAFE))) {
				/*
				 * Can't lock a member node_t or the node_t
				 * is marked safe so as this file node_t will
				 * be used soon move on to the next oldest.
				 */
				node_t	*end = fnp;

				if (locked) {
					mutex_exit(&fnp->lock);
				}
				fnp = np->filenext;
				while (fnp != end) {
					fnp->ref &= ~REF_SAFE;
					mutex_exit(&fnp->lock);
					fnp = fnp->filenext;
				}
				np->ref &= ~REF_SAFE;
				mutex_exit(&np->lock);
				goto busy;
			}
			fnp->ref |= REF_SAFE;
			fnp = fnp->filenext;
		}
		/* Next walk the file list again and phys reclaim all */
		fnp = np->filenext;
		while (fnp) {
			lrub_t	*fbp = fnp->bucket;

			np->filenext = fnp->filenext;
			if (fbp != NULL && fbp != bp) {
				/* Different lru bucket, so lock it down */
				mutex_enter(&fbp->lock);
			}
			fnp->fileback = NULL;
			nca_reclaim_phys(fnp, fbp);
			if (fbp == bp) {
				/* Same lru bucket, so lock it down again */
				mutex_enter(&bp->lock);
			}
			fnp = np->filenext;
		}
	} else if ((fnp = np->fileback) != NULL) {
		/*
		 * The node_t's a uri node_t, so find it
		 * in the file node_t's list and delete it.
		 */
		node_t	*cfnp;
		node_t	*pfnp = fnp;

		np->fileback = NULL;
		mutex_enter(&fnp->lock);
		cfnp = fnp->filenext;
		while (cfnp != np) {
			pfnp = cfnp;
			cfnp = cfnp->filenext;
		}
		pfnp->filenext = cfnp->filenext;
		mutex_exit(&fnp->lock);
	}
	nca_reclaim_phys(np, bp);
	return (true);
}

static boolean_t
nca_http_mmap(node_t *np, int kmflag)
{
	node_t	*head = np;
	char	*dp;
	uint16_t *p;
	ssize_t	len;
	ssize_t	tlen;
	ssize_t	sz1;
	ssize_t	sz2;
	ssize_t	mss = np->mss;
	ssize_t sz = 0;

	if (np->datasz == 0) {
		/* Zero length node_t */
		goto done;
	}

	do {
		if (np->cksum != NULL)
			continue;

		dp = np->data;
		if ((len = np->hlen) > 0) {
			tlen = np->datasz - len;
		} else {
			len = np->datasz;
			tlen = 0;
		}
		sz1 = len / mss;
		if (sz1 * mss != len)
			sz1++;
		sz2 = tlen / mss;
		if (sz2 * mss != tlen)
			sz2++;
		sz2 += sz1 + 1;
		sz2 *= sizeof (*p);
		sz += sz2;
		if ((p = (uint16_t *)kmem_alloc(sz2, kmflag)) == NULL) {
			goto fail;
		}
		np->cksum = p;
		*p++ = (uint16_t)sz2;
		/*
		 * Note: using a uint16_t to store the allocation size
		 * limits the array size to 32768 elements or for an
		 * Ethernet mss of 1460 a limit of 47841280 bytes for
		 * any single node_t.
		 */
		sz1 = mss;
		while (len > 0) {
			if (len < sz1)
				sz1 = len;
			*p++ = tcp_cksum((uint8_t *)dp, sz1);
			dp += sz1;
			if ((len -= sz1) == 0) {
				if ((len = tlen) == 0)
					break;
				tlen = 0;
				sz1 = mss;
			}
		}
	} while ((np = np->next) != NULL);
done:
	head->mp = desballoc((unsigned char *)head->data, head->datasz,
	    BPRI_HI, &head->frtn);
	if (head->mp == NULL) {
		goto fail;
	}
	head->mp->b_queue = (queue_t *)head;
	head->ref++;
	head->ref |= REF_MBLK;
	sz += sizeof (mblk_t) + sizeof (dblk_t);
	NCA_COUNTER((ulong_t *)&nca_mbmem, sz);
	NCA_DEBUG_COUNTER(&nca_desballoc, 1);
	return (true);
fail:
	while (head != np) {
		kmem_free(head->cksum, *head->cksum);
		head->cksum = NULL;
		head = head->next;
	}
	return (false);
}

static boolean_t
nca_http_vmap(node_t *np, int kmflag)
{
	char	*dp;
	int	len = np->datasz;
	page_t	**pp = np->pp;
	ssize_t	avail;

	if (np->ref & REF_KMEM) {
		/*
		 * Was kmem_alloc()ed in pmap(), so already vmap()ed
		 */
		np->data = (char *)np->pp;
		np->ref |= REF_VIRT;
		return (true);
	}
again:
	for (;;) {
		avail = nca_vpmax - nca_vpmem;
		avail -= btopr(nca_kbmem)
			+ btopr(nca_mbmem)
			+ btopr(nca_cbmem)
			+ btopr(nca_lbmem)
			+ btopr(len);

		if (avail >= 0)
			break;

		if (! nca_reclaim_vlru(np->size))
			break;
	}
	if ((dp = kmem_phys_mapin(pp, KM_NOSLEEP)) == NULL) {
		NCA_DEBUG_COUNTER(&nca_mapinfail, 1);
		if (kmflag == KM_NOSLEEP) {
			/*
			 * Called with KM_NOSLEEP, so simply return failure
			 * and the caller will make other arrangements (i.e.
			 * call again from a worker thread whith KM_SLEEP).
			 */
			NCA_DEBUG_COUNTER(&nca_mapinfail3, 1);
			return (false);
		}
		if (kmem_maxavail() >= len) {
			/*
			 * Looks like freed enough virt mapping above, but
			 * may not be enough contiguous so punt, reclaim
			 * some extra.
			 *
			 * A solution would be to do segmented mapin()s
			 * (i.e. don't rely on contiguous virt mapping)?
			 *
			 * This is a node_t of a node_t chain (i.e. a
			 * singly linked list (np->next) of node_t's),
			 * so we could try to half this node_t segment
			 * into two? The downside is that we'd have to
			 * allocate anohter node_t, two new phys_map()s,
			 * copy the current phys_map() into the two halfs,
			 * then freeup the current phys_map().
			 */
			NCA_DEBUG_COUNTER(&nca_mapinfail2, 1);
			if (! nca_reclaim_vlru(np->size))
				return (false);
			goto again;
		}
		/*
		 * Looks like didn't free enough virt mapping above,
		 * so punt, reclaim some extra.
		 *
		 * This is either caused by a race for resources and
		 * we lost or the node_t has dangling mblk references
		 * which have caused mapout() to not be called?
		 */
		NCA_DEBUG_COUNTER(&nca_mapinfail1, 1);
		if (! nca_reclaim_vlru(np->size))
			return (false);
		goto again;
	}
	np->data = dp;
	np->ref |= REF_VIRT;
	NCA_COUNTER(&nca_vpmem, btopr(len));
	return (true);
}

static void
nca_http_pmap(node_t *np)
{
	int	len = np->datasz;
	page_t	**pp;
	ssize_t	avail;
	pgcnt_t	npages = btopr(len);

	for (;;) {
		avail = nca_ppmax - nca_ppmem;
		avail -= btopr(nca_kbmem)
			+ btopr(nca_mbmem)
			+ btopr(nca_cbmem)
			+ btopr(nca_lbmem)
			+ btopr(len);

		if (avail >= 0)
			break;

		if (! nca_reclaim_plru(np->size))
			break;
	}
	if (npages < nca_ppthresh) {
		/*
		 * Better (more efficient and/or faster) to use
		 * kmem_alloc() then kmem_phys/kmem_map.
		 */
		char	*dp = kmem_alloc(len, KM_SLEEP);

		np->pp = (page_t **)dp;	/* overload */
		np->ref |= REF_PHYS|REF_KMEM;
		NCA_COUNTER((ulong_t *)&nca_kbmem, len);
		return;
	}
	pp = kmem_phys_alloc(len, KM_SLEEP);
	np->pp = pp;
	np->ref |= REF_PHYS;
	NCA_COUNTER(&nca_ppmem, btopr(len));
}

node_t *
nca_file_lookup(conn_t *connp, char *path, int pathsz)
{
	node_t		*np;
	vnode_t		*vp;
	int		sz;
	unsigned	ui;
	kmutex_t	*lock;
	int		rval;
	char		*dp;
	int		datasz;
	mblk_t		*mp;
	vattr_t		vattr;
	uio_t		uioin;
	iovec_t		iov;
	char		*a;
	char		*b;
	boolean_t	directio;

	boolean_t	newphys = false;
	boolean_t	newvirt = false;

	/*
	 * File path component slash seperator compression
	 * (i.e. only one slash seperator per and no suffix).
	 */
	a = &path[pathsz];
	/* No suffix slash(es) */
	do {
		if (*--a != '/')
			break;
		*a = NULL;
		pathsz--;
	} while (a > path);
	/* Only one slash per path component seperator */
	b = NULL;
	do {
		if (*--a == '/') {
			if (b == NULL) {
				b = a;
			}
		} else if (b != NULL) {
			if (b != a + 1) {
				char *cp = a + 1;

				pathsz -= (b - cp);
				do {
					*cp++ = *b;
				} while (*b++ != NULL);
			}
			b = NULL;
		}
	} while (a > path);
	/* Only one prefix slash */
	if (b != NULL && a != b) {
		path = b;
		pathsz -= (b - a);
	}
	/*
	 * File hash lookup.
	 */
	HASH_IX(path, pathsz, ui);
	lock = &filehash[ui].lock;
	mutex_enter(lock);
	np = filehash[ui].head;
	dp = &path[pathsz];

	while (np) {
		a = &np->path[np->pathsz];
		b = dp;
		do {
			if (*--a != *--b)
				break;
		} while (a > np->path && b > path);
		if (b == path && a == np->path) {
			mutex_enter(&np->lock);
			break;
		}
		np = np->hashnext;
	}
	if (np != NULL) {
		/* Found a match */
		mutex_exit(lock);
		if ((np->ref & REF_VIRT) != NULL) {
			/* Ready to be used */
			NCA_COUNTER(&nca_filehits, 1);
			goto done;
		}
		if ((np->ref & REF_PHYS) == NULL) {
			/* No phys mapping, try again */
			goto dophys;
		}
		/* Do virt mapping again */
		np->ref |= REF_SAFE;
		(void) nca_http_vmap(np, KM_SLEEP);
		NCA_COUNTER(&nca_filemissfast2, 1);
		newvirt = true;
		goto done;
	}

	/*
	 * No match, add it to the hash. But do the lookup first.
	 */

	rval = lookupname(path, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (rval != 0) {
		/* XXX Generate appropriate HTTP response */
		cmn_err(CE_WARN, "nca_file_lookup: lookup of \"%s\" failed "
		    "(error %d)", path, rval);
		mutex_exit(lock);
		return (NULL);
	}

	vattr.va_mask = AT_SIZE;
	rval = (VOP_GETATTR(vp, &vattr, 0, kcred));
	if (rval != 0) {
		/* XXX Generate appropriate HTTP response */
		cmn_err(CE_WARN, "nca_file_lookup: getattr of \"%s\" failed "
		    "(error %d)", path, rval);
		mutex_exit(lock);
		goto fileout;
	}

	NCA_COUNTER(&nca_filemiss, 1);
	mp = allocb(pathsz + 1, BPRI_HI);
	if (mp == NULL) {
		mutex_exit(lock);
		return (NULL);
	}

	bcopy(path, mp->b_rptr, pathsz + 1);
	mp->b_wptr = mp->b_rptr + pathsz + 1;
	np = node_add((char *)mp->b_rptr, pathsz, filehash, KM_SLEEP);
	mutex_exit(lock);
	np->req = mp;
	np->expire = -1;
	np->mss = connp->tcp_mss;

	if ((datasz = (int)vattr.va_size) == 0) {
		/*
		 * Zero length file !!!
		 */
		np->ref |= (REF_VIRT | REF_DONE);
		goto done;
	}

	/* Allocate node_t phys and virt memory */
	np->size = datasz;
	np->datasz = datasz;

	/* Do phys mapping */
dophys:
	nca_http_pmap(np);
	newphys = true;

	/* Do virt mapping */
	(void) nca_http_vmap(np, KM_SLEEP);
	dp = np->data;
	newvirt = true;

	if (((unsigned)dp & 1) == 0 && datasz > DEV_BSIZE) {
		/* May be able to use directio */
		directio = true;
		(void) VOP_IOCTL(vp, _FIODIRECTIO, DIRECTIO_ON, 0, kcred, NULL);
		NCA_DEBUG_COUNTER(&nca_httpd_filename1, 1);
	} else {
		directio = false;
		NCA_DEBUG_COUNTER(&nca_httpd_filename2, 1);
	}
	VOP_RWLOCK(vp, 0);
	uioin.uio_iov = &iov;
	uioin.uio_iovcnt = 1;
	uioin.uio_segflg = UIO_SYSSPACE;
	uioin.uio_fmode = 0;
	uioin.uio_loffset = 0;
	do {
		if (((unsigned)dp & 1) == 0 && datasz > DEV_BSIZE) {
			sz = datasz & (~(DEV_BSIZE - 1));
		} else {
			sz = datasz;
		}
		iov.iov_base = dp;
		iov.iov_len = sz;
		uioin.uio_resid = sz;
		rval = VOP_READ(vp, &uioin, 0, kcred);
		if (rval != 0) {
			/* XXX Generate appropriate HTTP response */
			VOP_RWUNLOCK(vp, 0);
			goto fileout;
		}
		dp += sz;
		if ((datasz -= sz) > 0) {
			/* Tail i/o after a directio */
			sz = datasz;
		}
	} while (datasz > 0);
	VOP_RWUNLOCK(vp, 0);
	if (directio) {
		/* Disable directio, others may not want it on */
		(void) VOP_IOCTL(vp, _FIODIRECTIO, DIRECTIO_OFF, 0,
					kcred, NULL);
	}
	VN_RELE(vp);
	NCA_DEBUG_COUNTER(&nca_httpd_filename, 1);

	np->ref |= REF_DONE;

done:
	if (np->mp == NULL) {
		/* No mblk mapping, map it */
		(void) nca_http_mmap(np, KM_SLEEP);
	}

	if (np->hashfanout != NULL) {
		/* Make MRU in phys and virt LRUs */
		lrub_t	*bp;

		LRU_BP(np->size, bp);
		mutex_enter(&bp->lock);
		np->bucket = bp;
		if (np->ref & REF_PHYS) {
			if (newphys) {
				/* Insert at head */
				np->plrupn = NULL;
				if ((np->plrunn = bp->phead) != NULL)
					bp->phead->plrupn = np;
				else
					bp->ptail = np;
				bp->phead = np;
				bp->pcount++;
				NCA_DEBUG_COUNTER(&nca_plrucnt, 1);
			} else if (np->plrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->plrupn->plrunn = np->plrunn) != NULL)
					np->plrunn->plrupn = np->plrupn;
				else
					bp->ptail = np->plrupn;
				/* Next, insert at head */
				np->plrupn = NULL;
				np->plrunn = bp->phead;
				bp->phead->plrupn = np;
				bp->phead = np;
			}
		}
		if (np->ref & REF_VIRT) {
			if (newvirt) {
				/* Insert at head */
				np->vlrupn = NULL;
				if ((np->vlrunn = bp->vhead) != NULL)
					bp->vhead->vlrupn = np;
				else
					bp->vtail = np;
				bp->vhead = np;
				bp->vcount++;
				NCA_DEBUG_COUNTER(&nca_vlrucnt, 1);
			} else if (np->vlrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->vlrupn->vlrunn = np->vlrunn) != NULL)
					np->vlrunn->vlrupn = np->vlrupn;
				else
					bp->vtail = np->vlrupn;
				/* Next, insert at head */
				np->vlrupn = NULL;
				np->vlrunn = bp->vhead;
				bp->vhead->vlrupn = np;
				bp->vhead = np;
			}
		}
		mutex_exit(&bp->lock);
	}
	np->ref &= ~REF_SAFE;
	return (np);

fileout:
	if (vp != NULL) {
		VN_RELE(vp);
	}
	return (NULL);
}

/*
 * httpd_data() - called to process an httpd_upcall response chunk, this is
 * done in three parts:
 *
 * 1) If not first response chunk goto 2) else preparse response for
 *    HTTP response code and any response header fields needed pre
 *    response data node_t allocation.
 *
 * 2) Allocate physical memory and virtual mapping, copy any header,
 *    file, and trailer data.
 *
 * 3) If not first response chunk then done else postparse response
 *    for any other needed header fields.
 */

static node_t *
/*ARGSUSED*/
nca_httpd_data(node_t *np, conn_t *connp, nca_io_t *iop, int iolen)
{
	char		*resp;
	int		reshdrsz;
	char		*respt;
	char		*dp;
	char		*start;
	int		sz;
	int		len;
	int		kasz;
	int32_t		conl;
	char		*filename;
	node_t		*fnp;
	char		ncai_c;
	char		*ncai_e;
	char		*conl_e;
	char		*conl_i;
	char		*conn_e;

	int32_t		hlen = 0;
	int32_t		tlen = 0;
	int32_t		off = 0;
	int		filename_len = 0;
	char		*ncai_s = NULL;
	char		*conl_s = NULL;
	char		*conn_s = NULL;
	char		*kadata = NULL;
	char		lastmod[] = "Last-Modified:";
	char		expire[] = "Expire:";
	char		resp0[] = "HTTP/1.0 200 ";
	char		resp1[] = "HTTP/1.1 200 ";
	char		cookie[] = "Set-Cookies:";
	char		auth[] = "Authenticate:";
	char		cntrl[] = "Control:";

	char		connka[] = "\r\nConnection: Keep-Alive";
	char		connclose[] = "\r\nConnection: close";

	char		ncai[] = "NCA-Include:";

	if (iop->version != NCA_HTTP_VERSION1) {
		return (np);
	}

	/* Get pointer into ncaio_t of http_data */
	resp = (char *)iop + sizeof (*iop) + iop->http_data;
	/* Need to preparse ? */
	if ((np->ref & REF_RESP) != NULL) {
		/* Nop, skip */
		reshdrsz = 0;
		fnp = NULL;
		goto nopreparse;
	}
	np->ref |= REF_RESP;
	/*
	 * Preparse, first find response header terminator.
	 */
	dp = resp;
	start = &dp[iop->http_len];
	len = 0;
	while (dp < start) {
		char c = *dp++;

		if (c == '\n') {
			len++;
			if (len == 2)
				break;
			continue;
		} else if (c == '\r') {
			continue;
		}
		len = 0;
		respt = dp;
	}
	if (len == 2) {
		reshdrsz = (dp - resp);
	} else {
		/* No response header termination ??? */
		reshdrsz = 0;
		fnp = NULL;
		goto nopreparse;
	}

	/*
	 * Check for "NCA-Include: hlen tlen off filename".
	 *
	 * Note: the WS must be a single <SP>.
	 */
	start = resp;
	sz = reshdrsz;
	if ((dp = strrncasestr(start, ncai, sz)) != NULL) {
		/* Got "NCA-Include:" */
		ncai_s = dp;
		dp += sizeof (ncai);
		sz -= (dp - start);
		start = dp;
		if ((ncai_e = strnchr(start, '\n', sz)) == NULL)
			goto no_ncai;
		if (ncai_e[-1] == '\r')
			ncai_e--;
		/* Got "NCA-Include: ...\n" */
		if ((dp = strnchr(start, ' ', sz)) == NULL || dp >= ncai_e)
			goto no_ncai;
		/* Got "NCA-Include: hlen ...\n" */
		hlen = (dp - start);
		sz -= hlen + 1;
		hlen = atoin(start, hlen);
		start = dp + 1;
		if ((dp = strnchr(start, ' ', sz)) == NULL || dp >= ncai_e)
			goto no_ncai;
		/* Got "NCA-Include: hlen tlen ...\n" */
		tlen = (dp - start);
		sz -= tlen + 1;
		tlen = atoin(start, tlen);
		start = dp + 1;
		if ((dp = strnchr(start, ' ', sz)) == NULL || dp >= ncai_e)
			goto no_ncai;
		/* Got "NCA-Include: hlen tlen off ...\n" */
		off = (dp - start);
		sz -= off + 1;
		off = atoin(start, off);
		start = dp + 1;
		if (start >= ncai_e)
			goto no_ncai;
		/*
		 * Got "NCA-Include: hlen tlen off filename\n", terminate
		 * "filename" with a NULL for string function usage.
		 */
		ncai_c = *ncai_e;
		filename = start;
		filename_len = (ncai_e - start);
	} else {
	no_ncai:
		ncai_s = NULL;
		ncai_e = NULL;
		filename = (char *)iop + sizeof (*iop) + iop->filename;
		if ((filename_len = iop->filename_len) > 0) {
			/* Skip the zero byte term */
			filename_len--;
		}
	}
	/* Check for "Connection: ..." */
	start = resp;
	sz = reshdrsz;
	if ((len = sizeof (connclose) - 1,
	    (dp = strrncasestr(start, connclose, sz)) != NULL) ||
	    (len = sizeof (connka) - 1,
	    (dp = strrncasestr(start, connka, sz)) != NULL)) {
		/*
		 * Got "Connection: close" or "Connection: Keep-Alive",
		 * skip it as we handle all HTTP connection managment.
		 */
		conn_s = dp;
		conn_e = dp + len;
	}
	/* Need to check for "Content-Length: ..." ? */
	sz = reshdrsz;
	if (sz > 0 && (connp->http_persist || filename_len)) {
		char	contl[] = "Content-Length:";

		start = resp;
		dp = strncasestr(start, contl, sz);
		if (dp != NULL) {
			/*
			 * This response has a "Content-Length: ..."
			 * header field, so save the argument value.
			 */
			dp += sizeof (contl);
			sz -= (dp - start);
			start = dp;
			if ((dp = strnchr(start, '\n', sz)) != NULL) {
				if (dp[-1] == '\r')
					dp--;
				sz = (dp - start);
				conl = atoin(start, sz);
				/* Save pointers for possible head rewrite */
				conl_s = start;
				conl_e = dp;
			} else {
				/* Invalid "Content-Lenght:" field ? */
				conl = -1;
			}
		} else {
			/* No "Content-Lenght:" field */
			conl = -1;
		}
	} else {
		/* Don't need to check for "Content-Lenght:" field */
		conl = -1;
	}

	if (np->version > HTTP_1_0) {
		/*
		 * HTTP/1.1 or greater, so check for both
		 * "Keep-Alive" and "close" keywords.
		 */
		if (conl < 0 && connp->http_persist) {
			/*
			 * No "Content-Lenght:" field and is persist,
			 * so not any more.
			 */
			connp->http_persist = 0;
			connp->http_was_persist = 1;
			/*
			 * Note: we change the node_t state for connection
			 *	 persist without regard to any other conn_t
			 *	 request pending for this node_t as we mark
			 *	 the node_t for nocache.
			 */
			np->persist = 0;
			np->was_persist = 1;
			iop->nocache = 1;
		}
		if (connp->http_persist) {
			kadata = connka;
			kasz = sizeof (connka) - 1;
		} else if (connp->http_was_persist) {
			kadata = connclose;
			kasz = sizeof (connclose) - 1;
		}
	} else if (np->version == HTTP_1_0) {
		/*
		 * HTTP/1.0, so check for "Keep-Alive" keyword.
		 */
		if (conl < 0 && connp->http_persist) {
			/*
			 * No "Content-Lenght:" field and is persist,
			 * so not any more.
			 */
			connp->http_persist = 0;
			/*
			 * Note: we change the node_t state for connection
			 *	 persist without regard to any other conn_t
			 *	 request pending for this node_t as we mark
			 *	 the node_t for nocache.
			 */
			np->persist = 0;
			iop->nocache = 1;
		}
		if (connp->http_persist) {
			kadata = connka;
			kasz = sizeof (connka) - 1;
		}
	}
	if (filename_len) {
		if (ncai_e != NULL) {
			*ncai_e = NULL;
		}
		fnp = nca_file_lookup(connp, filename, filename_len);
		if (fnp == NULL) {
			/*
			 * Error of some sort, let the caller handle it.
			 * We set the size to -1 so that the caller
			 * (nca_http_httpd) can create a 500 response.
			 */
			np->size = -1;
			return (np);
		}
		sz = fnp->datasz;

		if (conl < 0) {
			/* No header, so rely on the file size for conl */
			conl = sz;
			conl_s = NULL;
		} else if (hlen > 0) {
			/* We have a "NCA-Include: ...", so adjust conl */
			conl = hlen + sz - off + tlen;
		} else if (sz != conl) {
			/* "Content-Length:" != file size ? */
			if (nca_httpd_debug) {
				prom_printf("nca_httpd_data: \"%s\" %d != %d\n",
				    filename, sz, conl);
			}
			NCA_DEBUG_COUNTER(&nca_httpd_badsz, 1);
			goto fileout;
		} else {
			/* No "Content-Length: ..." rewrite. */
			conl_s = NULL;
		}
		if (ncai_e != NULL) {
			*ncai_e = ncai_c;
		}
	} else {
		/* No filename, so no conl processing */
		conl_s = NULL;
		fnp = NULL;
	}
nopreparse:
	/* Time to calculate the amount of storage to allocate and vmap */
	len = 0;
	if (kadata != NULL) {
		/*
		 * We have a KeepAlive "Connection: " header field
		 * to insert after last header field, can we?
		 */
		if (reshdrsz > 0) {
			/* Yup */
			len += kasz;
		} else {
			/* Nop */
			kadata = NULL;
		}
	}
	if (ncai_s != NULL) {
		/*
		 * We have a "NCA-Include: ..." header field
		 * to remove, so make appropriate adjustments.
		 */
		len -= (ncai_e - ncai_s) + 2;
		if (ncai_c != '\r') {
			/* Not a "\r\n" line term, so just "\n" */
			len++;
		}
	}
	if (conl_s != NULL) {
		/*
		 * We have a "Content-Length: ..." header rewrite to do
		 * for a file include (i.e.  "NCA-Include: ..." case)
		 * where we didn't know the conl until now.
		 */
		int d;
		int n;

		dp = conl_s;
		for (d = 1; conl >= d; d *= 10)
			;
		for (dp = conl_s; dp < conl_e; dp++) {
			if ((d /= 10) == 0)
				break;
			n = conl / d;
			*dp = '0' + n;
			conl -= n * d;
		}
		if (d > 0) {
			/*
			 * Didn't fit in the old conl string, so use the
			 * "NCA-Include: ..." string for overflow digit(s),
			 * overload conl_s to point to the end of the partial
			 * rewrite above (i.e. the insert point) and conl_e
			 * to point to the end of the overflow area pointed
			 * to by ncai_s.
			 */
			conl_s = dp;
			dp = ncai_s;
			conl_i = dp;
			for (dp = ncai_s; dp < ncai_e; dp++) {
				if ((d /= 10) == 0)
					break;
				n = conl / d;
				*dp = '0' + n;
				conl -= n * d;
				len++;
			}
			/* What if an overflow of the overflow? */
			conl_e = dp;
		} else {
			/* Did fit, no overflow pointer, space pad if needed. */
			conl_s = NULL;
			while (dp < conl_e)
				*dp++ = ' ';
		}
	}
	if (hlen > 0) {
		sz = reshdrsz + hlen + tlen + iop->trailer_len + len;
	} else {
		sz = iop->http_len + iop->trailer_len + len;
	}
	if (sz == 0) {
		/* No returned data */
		if (nca_httpd_debug) {
			prom_printf("nca_httpd_data: no returned data?\n");
		}
		NCA_DEBUG_COUNTER(&nca_httpd_nosz, 1);
		goto fileout;
	}
	if (np->size > 0) {
		/* Node used, need a new one */
		np = node_add_next(np->back ? np->back : np);
		np->size = 0;

		if (nca_debug) {
			prom_printf("nca_httpd_data: add node: %p\n",
			    (void *)np);
		}
	}
	/* Do any file node_t initialization */
	if (filename_len) {
		if (fnp->datasz > 0) {
			/* Only setup for non zero length files */
			if (hlen > 0)
				np->hlen = reshdrsz + hlen + len;
			else
				np->hlen = sz;
			np->fileoff = off;
			np->fileback = fnp;
			np->filenext = fnp->filenext;
			fnp->filenext = np;
		}
		mutex_exit(&fnp->lock);
	}
	/* Allocate node_t phys and virt memory */
	np->size = sz;
	np->datasz = sz;

	/* Do phys mapping */
	nca_http_pmap(np);

	/* Do virt mapping */
	(void) nca_http_vmap(np, KM_SLEEP);

	/* Construct the response from http_data + filename + trailer */
	dp = np->data;
	start = resp;

	/* HTTP http_data (if any? (there always is?)) */
	len = iop->http_len;
	if (len != 0 && reshdrsz > 0) {
		/* While a special case process in source address order. */
		char *cp;

		for (cp = &resp[reshdrsz]; ; cp = &resp[reshdrsz]) {
			if (ncai_s != NULL)
				cp = ncai_s;
			if (conl_s != NULL && conl_s < cp)
				cp = conl_s;
			if (conn_s != NULL && conn_s < cp)
				cp = conn_s;
			if (kadata != NULL && respt < cp)
				cp = respt;
			if (cp == &resp[reshdrsz])
				break;
			/* Copy to special case */
			sz = (cp - start);

			if (nca_httpd_debug) {
				if (cp == ncai_s) {
					prom_printf("ncai_s:");
				} else if (cp == conl_s) {
					prom_printf("conl_s:");
				} else if (cp == conn_s) {
					prom_printf("conn_s:");
				} else if (cp == respt) {
					prom_printf("kadata:");
				} else {
					prom_printf("???:");
				}
				prom_printf(" copy %d from %d to %d", sz,
				    (start - resp), (dp - np->data));
			}

			bcopy(start, dp, sz);
			start = cp;
			len -= sz;
			dp += sz;

			if (nca_httpd_debug) {
				*dp = NULL;
				prom_printf(" \"%s\"\n\n", np->data);
			}

			if (cp == ncai_s) {
				/* Skip "NCA-Include: ..." header */
				sz = (ncai_e - ncai_s) + 2;
				if (ncai_c != '\r') {
					/* Not a "\r\n" line term (i.e. "\n") */
					sz--;
				}
				start += sz;
				len -= sz;
				ncai_s = NULL;
			} else if (cp == conl_s) {
				/* Insert from conl rewrite overflow area */
				sz = (conl_e - conl_i);
				bcopy(conl_i, dp, sz);
				dp += sz;
				conl_s = NULL;
			} else if (cp == conn_s) {
				/* Skip "Connection: ..." header */
				sz = (conn_e - conn_s);
				start += sz;
				len -= sz;
				conn_s = NULL;
			} else if (cp == respt) {
				/* Insert kadata char(s) */
				bcopy(kadata, dp, kasz);
				dp += kasz;
				kadata = NULL;
			}
		}
		/* Copy through respt */
		sz = (cp - start);
		bcopy(start, dp, sz);
		start += sz;
		dp += sz;
		len -= sz;

		if (nca_httpd_debug) {
			*dp = NULL;
			prom_printf("nca_httpd_data: reshdr \"%s\"\n",
			    np->data);
		}

	}
	if (len != 0) {
		/*
		 * Copy data up to hlen worth, the rest (if any)
		 * will be copied in the trailer section below.
		 */
		sz = (hlen == 0 ? len : hlen);
		bcopy(start, dp, sz);
		start += sz;
		len -= sz;
		dp += sz;
		NCA_DEBUG_COUNTER(&nca_httpd_http, 1);
	}

	/* HTTP trailers (if any?) */

	if (nca_httpd_debug && tlen != 0 && len != tlen)
		prom_printf("nca_httpd_data: tlen(%d) != len(%d)\n", tlen, len);

	len = tlen;
	if (len != 0) {
		bcopy(start, dp, len);
		dp += len;
		NCA_DEBUG_COUNTER(&nca_httpd_trailer, 1);
	}
	len = iop->trailer_len;
	if (len != 0) {
		start = (char *)iop + sizeof (*iop) + iop->trailer;
		bcopy(start, dp, len);
		dp += len;
		NCA_DEBUG_COUNTER(&nca_httpd_trailer, 1);
	}
	if (np->datasz != (dp - np->data)) {
		/* Not enough data ??? */
		/* XXX Generate appropriate HTTP response */

		if (nca_httpd_debug) {
			prom_printf("nca_httpd_data: not enough data (%d<%d)\n",
			    (dp - np->data), np->datasz);
		}

		return (np);
	}

	if (reshdrsz == 0 || np->hashfanout == NULL) {
		/*
		 * No HTTP response header to parse (i.e. either
		 * already parsed in a previous chunk or no response
		 * header termination found?) or not cached.
		 */
		goto nopostparse;
	}

	if (iop->advisory) {
		/*
		 * The httpd has marked this response as advisory cache,
		 * as we don't support this yet just treat it as a nocache
		 * (delete this node_t from the node_t lookup and skip the
		 * response header parse).
		 */
		NCA_DEBUG_COUNTER(&nca_nocache12, 1);
		goto nocache;
	}

	if (iop->nocache) {
		/*
		 * The httpd has marked this response as no cache, so delete
		 * this node_t from the node_t lookup and skip the response
		 * header parse.
		 */
		NCA_DEBUG_COUNTER(&nca_nocache12, 1);
		goto nocache;
	}

	/*
	 * Postparse response header fields of interest.
	 */
	resp = np->data;
	sz = reshdrsz;
	if ((dp = strncasestr(resp, lastmod, sz)) != NULL) {
		start = dp + sizeof (lastmod);
		sz -= (dp - resp);
		if ((dp = strnchr(start, '\n', sz)) != NULL) {
			char	sc;

			if (dp[-1] == '\r')
				dp--;
			sc = *dp;
			*dp = 0;
			np->lastmod = nca_http_date(start);
			*dp = sc;
		}
	} else if (resp != NULL) {
		NCA_DEBUG_COUNTER(&nca_nocache8, 1);
		goto nocache;
	}
	if ((dp = strncasestr(resp, expire, sz)) != NULL) {
		start = dp + sizeof (expire);
		sz -= (dp - resp);
		if ((dp = strnchr(start, '\n', sz)) != NULL) {
			time_t	expire;
			char	sc;

			if (dp[-1] == '\r')
				dp--;
			sc = *dp;
			*dp = 0;
			expire = nca_http_date(start);
			*dp = sc;
			expire -= hrestime.tv_sec;
			if (expire < 0)
				np->expire = 0;
			else
				np->expire = lbolt+SEC_TO_TICK(expire);
		}
		if (np->expire <= lbolt) {
			NCA_DEBUG_COUNTER(&nca_nocache9, 1);
			goto nocache;
		}
	} else if (np->expire == -1 && nca_expire_ttl != -1) {
		/* No expire speced and we have a TTL, so ... */
		np->expire = lbolt + SEC_TO_TICK(nca_expire_ttl);
	}
	if (strncasestr(resp, auth, sz) != NULL) {
		NCA_DEBUG_COUNTER(&nca_nocache10, 1);
		goto nocache;
	}
	if (strncasestr(resp, cntrl, sz) != NULL) {
		NCA_DEBUG_COUNTER(&nca_nocache11, 1);
		goto nocache;
	}
	if (strncasestr(resp, resp0, sz) == NULL &&
		strncasestr(resp, resp1, sz) == NULL) {
		/*
		 * Not response 200, no cache.
		 *
		 * XXX may need to parse the response HTTP version and
		 * status code in the future?
		 */
		NCA_DEBUG_COUNTER(&nca_nocache12, 1);
		goto nocache;
	}
	if (strncasestr(resp, cookie, sz) != NULL) {
		NCA_DEBUG_COUNTER(&nca_nocache13, 1);
		goto nocache;
	}
nopostparse:
	return (np);

nocache:
	node_del(np->back ? np->back : np, false);
	return (np);

fileout:
	if (fnp) {
		mutex_exit(&fnp->lock);
	}
	return (np);
}

/*
 * http_httpd() - called when an HTTP request line and any headers have
 * been received for a new HTTP message. A nca_io_t is composed and sent
 * to the http server via an httpd door.
 *
 * PUT/POST ...
 */

static uint32_t	httpd_tag = 0;	/* nca_io_t tag value, one per conn_t */

static void
nca_http_httpd(node_t *np, conn_t *connp)
{
	node_t		*hn = np;
	node_t		*nn;
	boolean_t	safed;
	int		len;
	int		max;
	mblk_t		*mp;
	uint8_t		first;
	uint8_t		more;
	int		rval;
	int		wcount;

	nca_http_method_t method = np->method;
	uint32_t	tag = connp->req_tag;
	nca_io_t	*iop = (nca_io_t *)find_http_buffer();

	struct sockaddr_in solocal;
	struct sockaddr_in sopeer;

	/*
	 * Entered with the node_t lock held and we may be here a while,
	 * so if not already marked safe (only marked safe for the first
	 * call), mark it before releasing the node_t lock.
	 */
	if ((np->ref & REF_SAFE) == 0) {
		safed = true;
		np->ref |= REF_SAFE;
	} else {
		safed = false;
	}
	mutex_exit(&np->lock);
	if (tag == 0) {
		/* First call for a conn_t */
		tag = atomic_add_32_nv(&httpd_tag, 1);

		if (nca_debug)
			prom_printf("nca_http_httpd(%p, %p): first call %d\n",
			    (void *)np, (void *)connp, tag);

		if (tag == 0) {
			/*
			 * Tag value 0 is reserved for NCA's internal use
			 * (conn_t.req_tag == 0 for no nca_io_t.tag), so
			 * just get another one.
			 */
			tag = atomic_add_32_nv(&httpd_tag, 1);
		}
		connp->req_tag = tag;

		np->size = 0;

		switch (method) {

		case NCA_GET:
		case NCA_HEAD:
		case NCA_OPTIONS:
		case NCA_DELETE:
		case NCA_TRACE:
		case NCA_POST:
		case NCA_PUT:
			first = 1;
			break;

		case NCA_UNKNOWN:
			more = 0;
			goto out;

		default:
			more = 0;
			goto out;
		}

		/* Init the sockaddrs */
		solocal.sin_family = AF_INET;
		solocal.sin_addr.s_addr = connp->laddr;
		solocal.sin_port = connp->conn_lport;

		sopeer.sin_family = AF_INET;
		sopeer.sin_addr.s_addr = connp->faddr;
		sopeer.sin_port = connp->conn_fport;

		/* Init nca_io_t fixed size fields */
		iop->version = NCA_HTTP_VERSION1;
		iop->op = http_op;
		iop->tag = tag;
		iop->nca_tid = (uint_t)curthread;
		iop->first = first;
		iop->advisory = 0;

		/* Copy data for other nca_io_t fields. */
		wcount = 0;
		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, filename_len, filename);

		NCA_IO_WDATA(&sopeer, sizeof (sopeer), iop, wcount,
			peer_len, peer);

		NCA_IO_WDATA(&solocal, sizeof (solocal), iop, wcount,
			local_len, local);

		max = n_http_buf_size - wcount - sizeof (nca_io_t);
		len = (np->reqsz > max ? max : np->reqsz);
		NCA_IO_WDATA(np->req->b_rptr, len, iop, wcount,
			http_len, http_data);
		max -= len;

		while (max > 0 && hn->reqcontl > 0 &&
			(mp = connp->tcp_rcv_head) != NULL) {
			len = (mp->b_wptr - mp->b_rptr);
			len = ((len > hn->reqcontl) ? hn->reqcontl : len);
			len = ((len > max) ? max : len);
			NCA_IO_ADATA(mp->b_rptr, len, iop, wcount,
				http_len, http_data);
			max -= len;
			hn->reqcontl -= len;
			mp->b_rptr += len;
			connp->tcp_rcv_cnt -= len;
			if (mp->b_rptr != mp->b_wptr) {
				break;
			}
			connp->tcp_rcv_head = mp->b_cont;
			mp->b_cont = NULL;
			freeb(mp);
			mp = NULL;
		}
		more = np->reqcontl > 0 ? 1 : 0;
		iop->more = more;

		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, trailer_len, trailer);

	} else {
		/* A subsequent call */

		if (nca_debug)
			prom_printf("nca_http_httpd(%p, %p): next call %d\n",
			    (void *)np, (void *)connp, tag);

		switch (method) {

		case NCA_GET:
		case NCA_HEAD:
		case NCA_OPTIONS:
		case NCA_DELETE:
		case NCA_TRACE:
		case NCA_POST:
		case NCA_PUT:
			first = 0;
			break;

		case NCA_UNKNOWN:
			more = 0;
			goto out;

		default:
			more = 0;
			goto out;
		}

	next:;
		/* Init nca_io_t fixed size fields */
		iop->version = NCA_HTTP_VERSION1;
		iop->op = http_op;
		iop->tag = tag;
		iop->nca_tid = (uint_t)curthread;
		iop->first = first;
		iop->advisory = 0;

		/* Copy data for other nca_io_t fields. */
		wcount = 0;

		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, filename_len, filename);
		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, peer_len, peer);
		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, local_len, local);

		if (np->reqcontl > 0) {
			/* LINTED */
			NCA_IO_WDATA(NULL, -1, iop, wcount, http_len,
					http_data);
			max = n_http_buf_size - wcount - sizeof (nca_io_t);
			while ((mp = connp->tcp_rcv_head) != NULL) {
				len = (mp->b_wptr - mp->b_rptr);
				len = (len > np->reqcontl) ? np->reqcontl : len;
				len = (len > max) ? max : len;
				if (len <= 0)
					break;

				if (nca_debug)
					prom_printf("nca_http_httpd: IO_ADATA"
					    "(%p(%p), %d, %p, %d)\n",
					    (void *)mp->b_rptr, (void *)mp, len,
					    (void *)iop, wcount);

				NCA_IO_ADATA(mp->b_rptr, len, iop, wcount,
					http_len, http_data);
				max -= len;
				np->reqcontl -= len;
				mp->b_rptr += len;
				connp->tcp_rcv_cnt -= len;
				if (mp->b_rptr != mp->b_wptr) {
					break;
				}
				connp->tcp_rcv_head = mp->b_cont;
				mp->b_cont = NULL;
				freeb(mp);
				mp = NULL;
				if (np->reqcontl == 0)
					break;
			}
		} else {
			/* LINTED */
			NCA_IO_WDATA(NULL, 0, iop, wcount, http_len, http_data);
		}
		more = np->reqcontl > 0 ? 1 : 0;
		iop->more = more;

		/* LINTED */
		NCA_IO_WDATA(NULL, 0, iop, wcount, trailer_len, trailer);

	}

	if (nca_httpd_debug) {

		prom_printf("iop: %d %d %d %x %d %d %d ", iop->version,
				iop->op, iop->tag, iop->nca_tid, iop->more,
				iop->first, iop->advisory);

		prom_printf("%d,%d %d,%d %d,%d %d,%d %d,%d: %d\n",
				iop->filename_len, iop->filename, iop->peer_len,
				iop->peer, iop->local_len, iop->local,
				iop->http_len, iop->http_data, iop->trailer_len,
				iop->trailer, sizeof (*iop) + wcount);

		prom_printf("Do nca_door_upcall: ");
	}

	/* Make upcall to HTTP server */
	len = n_http_buf_size;
	rval = nca_door_upcall(&nca_httpd_door_hand, (char *)iop,
	    (sizeof (*iop) + wcount), &len, np->sqp);

	if (nca_httpd_debug) {

		prom_printf("%d %d\n", rval, len);

		prom_printf("iop: %d %d %d %x %d %d %d ", iop->version,
				iop->op, iop->tag, iop->nca_tid, iop->more,
				iop->first, iop->advisory);

		prom_printf("%d,%d %d,%d %d,%d %d,%d %d,%d: %d\n",
				iop->filename_len, iop->filename, iop->peer_len,
				iop->peer, iop->local_len, iop->local,
				iop->http_len, iop->http_data, iop->trailer_len,
				iop->trailer, len);

	}

	if (rval == 0) {
		/* No error doors error */
		switch (iop->op) {

		case http_op:		/* process returned data (if any?) */
			break;

		case timeout_op:	/* timeout, for now punt */
		case error_op:		/* some NCA i/o error, punt */
		case error_retry_op:	/* retry, for now punt */

			(void) nca_http_response(np, connp, nca_resp_500,
				sizeof (nca_resp_500), NULL, NULL, NULL);
			more = 0;
			goto out;

		default:
			more = 0;
			goto out;
		}

		nn = nca_httpd_data(np, connp, iop, len);
		if (nn != np) {
			np = nn;
		}
		if (np->size == -1) {
			/* Some processing error, return a 500 response */
			(void) nca_http_response(np, connp, nca_resp_500,
				sizeof (nca_resp_500), NULL, NULL, NULL);
			more = 0;
			goto out;
		}
		if (more) {
			if (connp->tcp_rcv_head != NULL) {
				/* More data to sendup */
				first = 0;
				goto next;
			}
		} else if (iop->more) {
			/* More data to get */
			first = 0;
			goto next;
		}
		more = iop->more;
	} else if (rval == EAGAIN) {
		/* Server not available, return a 503 response */
		(void) nca_http_response(np, connp, nca_resp_503,
			sizeof (nca_resp_503), NULL, NULL, NULL);
		more = 0;
	} else if (rval == EINVAL) {
		/* No door server, return a 503 response */
		(void) nca_http_response(np, connp, nca_resp_503,
			sizeof (nca_resp_503), NULL, NULL, NULL);
		more = 0;
	} else {
		/* Some other error, return a 500 response */
		(void) nca_http_response(np, connp, nca_resp_500,
			sizeof (nca_resp_500), NULL, NULL, NULL);
		more = 0;
	}
out:
	mutex_enter(&hn->lock);
	if (safed) {
		hn->ref &= ~REF_SAFE;
	}
	if (! more) {
		/* No more data for this node_t, so mark it as done */
		hn->ref |= REF_DONE;
	}
}

static boolean_t
nca_http_miss(node_t *np, conn_t *connp, int kmflag)
{
	lrub_t	*bp;
	boolean_t success = true;
	boolean_t newphys = false;
	boolean_t newvirt = false;

	if (nca_debug) {
		prom_printf("nca_http_miss(%p, %p, %x): REF_DONE = %d\n",
		    (void *)np, (void *)connp, kmflag, (np->ref & REF_DONE));
	}

	if (! (np->ref & REF_DONE)) {
		/* Cache node not filled */
		nca_http_httpd(np, connp);

		if (! (np->ref & REF_DONE)) {
			/* Still not filled, more input required */
			np->ref &= ~REF_SAFE;

			if (connp->fill_mp == NULL) {
				NCA_DEBUG_COUNTER(&nca_fill_mp_NULL2, 1);
			}

			return (true);
		}
		if (np->size == -1) {
			/* An error occured, let the caller handle it */
			np->ref &= ~REF_SAFE;
			return (false);
		}
		if (np->pp == NULL) {
			np->ref &= ~REF_SAFE;
			return (false);
		}
		if (np->data == NULL) {
			np->ref &= ~REF_SAFE;
			return (false);
		}
		success = true;
		newphys = true;
		newvirt = true;
	} else {
		/* Have a cache node */
		node_t	*head = np;

		do {
			if (np->pp == NULL) {
				/* ??? */
				np->ref &= ~REF_SAFE;
				return (false);
			}
			if (np->data == NULL) {
				/* No virt mapping, so mapin() */
				success = nca_http_vmap(np, kmflag);
				if (! success) {
					break;
				}
				if (np == head)
					newvirt = true;
			}
		} while ((np = np->next) != NULL);
		np = head;
	}

	if (success && np->mp == NULL) {
		/* No mblk mapping, map it */
		success = nca_http_mmap(np, kmflag);
	}

	if (success && np->fileback != NULL) {
		node_t *head = np;
		boolean_t fnewvirt;

		np = head->fileback;
		mutex_enter(&np->lock);
		if ((np->ref & REF_SAFE) != 0) {
			/*
			 * Not SAFE to process this node_t for VIRT or MBLK
			 * mapping, so process as a miss type 3. Note list
			 * will be post processed by holder of SAFE.
			 */
			connp->nodenext = NULL;
			if (np->connhead != NULL)
				np->conntail->nodenext = connp;
			else
				np->connhead = connp;
			np->conntail = connp;
			CONN_REFHOLD(connp);
			mutex_exit(&np->lock);
			NCA_COUNTER(&nca_missed5, 1);
			return (false);
		}
		if ((np->ref & REF_VIRT) == NULL) {
			/*
			 * File node_t not VIRT mapped, so try to handle it.
			 */
			np->ref |= REF_SAFE;
			if (np->data != NULL) {
				/*
				 * File node_t was marked for VIRT reclaim,
				 * in the race between freeb()/node_fr() in
				 * vlru() and us we won, so just unmark it
				 * and reuse it.
				 */
				np->ref |= REF_VIRT;
			} else {
				success = nca_http_vmap(np, kmflag);
			}
			if (success) {
				/* Now mmap() it */
				success = nca_http_mmap(np, kmflag);
			}
			np->ref &= ~REF_SAFE;
			fnewvirt = true;
			NCA_COUNTER(&nca_missed6, 1);
		} else {
			fnewvirt = false;
		}

		if (success) {
			/* Make MRU in phys */
			LRU_BP(np->size, bp);
			mutex_enter(&bp->lock);
			np->bucket = bp;
			/* Make phys MRU */
			if (np->plrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->plrupn->plrunn = np->plrunn) != NULL)
					np->plrunn->plrupn = np->plrupn;
				else
					bp->ptail = np->plrupn;
				/* Next, insert at head */
				np->plrupn = NULL;
				np->plrunn = bp->phead;
				bp->phead->plrupn = np;
				bp->phead = np;
			}
			/* Make virt MRU */
			if (fnewvirt) {
				/* Not in LRU, Insert at head */
				np->vlrupn = NULL;
				if ((np->vlrunn = bp->vhead) != NULL)
					bp->vhead->vlrupn = np;
				else
					bp->vtail = np;
				bp->vhead = np;
				bp->vcount++;
				NCA_DEBUG_COUNTER(&nca_vlrucnt, 1);
			} else if (np->vlrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->vlrupn->vlrunn = np->vlrunn) != NULL)
					np->vlrunn->vlrupn = np->vlrupn;
				else
					bp->vtail = np->vlrupn;
				/* Next, insert at head */
				np->vlrupn = NULL;
				np->vlrunn = bp->vhead;
				bp->vhead->vlrupn = np;
				bp->vhead = np;
			}
			mutex_exit(&bp->lock);
			/* Last place a ref on the file node_t */
			connp->fill_fmp = dupb(np->mp);
		}
		np->ref &= ~REF_SAFE;
		mutex_exit(&np->lock);
		np = head;
	}

	if (np->hashfanout != NULL) {
		/* Make MRU in phys and virt LRUs */
		LRU_BP(np->size, bp);
		mutex_enter(&bp->lock);
		np->bucket = bp;
		if (np->ref & REF_PHYS) {
			if (newphys) {
				/* Insert at head */
				np->plrupn = NULL;
				if ((np->plrunn = bp->phead) != NULL)
					bp->phead->plrupn = np;
				else
					bp->ptail = np;
				bp->phead = np;
				bp->pcount++;
				NCA_DEBUG_COUNTER(&nca_plrucnt, 1);
			} else if (np->plrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->plrupn->plrunn = np->plrunn) != NULL)
					np->plrunn->plrupn = np->plrupn;
				else
					bp->ptail = np->plrupn;
				/* Next, insert at head */
				np->plrupn = NULL;
				np->plrunn = bp->phead;
				bp->phead->plrupn = np;
				bp->phead = np;
			}
		}
		if (np->ref & REF_VIRT) {
			if (newvirt) {
				/* Insert at head */
				np->vlrupn = NULL;
				if ((np->vlrunn = bp->vhead) != NULL)
					bp->vhead->vlrupn = np;
				else
					bp->vtail = np;
				bp->vhead = np;
				bp->vcount++;
				NCA_DEBUG_COUNTER(&nca_vlrucnt, 1);
			} else if (np->vlrupn != NULL) {
				/* Not MRU already, so delete from list */
				if ((np->vlrupn->vlrunn = np->vlrunn) != NULL)
					np->vlrunn->vlrupn = np->vlrupn;
				else
					bp->vtail = np->vlrupn;
				/* Next, insert at head */
				np->vlrupn = NULL;
				np->vlrunn = bp->vhead;
				bp->vhead->vlrupn = np;
				bp->vhead = np;
			}
		}
		mutex_exit(&bp->lock);
	}

	/* No longer need the node_t (or it's linked node_t(s)) safe */
	np->ref &= ~REF_SAFE;

	if (success) {
		/* Setup xmit list */
		int	sz;

		if ((sz = np->hlen) == 0) {
			sz = np->datasz;
		}
		connp->xmit.np = np;
		connp->xmit.dp = np->data;
		connp->xmit.sz = sz;
		connp->xmit.cp = (np->cksum) + 1;
		connp->xmit.fnp = np->fileback;
		if (np->hashfanout != NULL) {
			connp->fill_mp = dupb(np->mp);
		} else {
			/* Not hashed, so just uses the node_t's pointer */
			connp->fill_mp = np->mp;
			np->mp = NULL;
		}
	}

	if (success && connp->fill_mp == NULL) {
		NCA_DEBUG_COUNTER(&nca_fill_mp_NULL3, 1);
	}

	return (success);
}

/*
 * The HTTP session entry point, as data is received by TCP this function
 * is called, any accumulated receive data is on the TCP tcp_rcv list.
 */

node_t *nca_last_np;

ulong_t nca_missfastnov;
ulong_t nca_missfastnom;

/*
 * HTTP request methods we recognize:
 */

typedef struct {
	char	*s;
	uint_t	v;
} nca_methods_t;

static nca_methods_t nca_methods[] = {
	{"OPTIONS", NCA_OPTIONS},
	{"GET", NCA_GET},
	{"HEAD", NCA_HEAD},
	{"POST", NCA_POST},
	{"PUT", NCA_PUT},
	{"DELETE", NCA_DELETE},
	{"TRACE", NCA_TRACE},
	{0, NCA_UNKNOWN}
};

static nca_methods_t nca_method_unknown = {0, NCA_UNKNOWN};

int
nca_http(conn_t *connp)
{
	int	len;
	mblk_t	*mp1;
	node_t	*np;
	lrub_t	*bp;
	unsigned ui;
	kmutex_t *lock;
	char	*req;
	char	*pp;
	char	c;
	boolean_t cacheit;
	boolean_t persist;
	int	kmflag = connp->inq->sq_isintr ? KM_NOSLEEP : KM_SLEEP;

	mblk_t	*mp;			/* request (full) */
	int	reqsz;			/* size of above */
	char	*reqln;			/* request line */
	int	reqlnsz;		/* size of above */
	nca_methods_t *methodp;		/* request line method */
	uint_t	version = 0;		/* request line version */
	char	*path = NULL;		/* request line URI path */
	int	pathsz = 0;		/* size of above */
	char	*reqhdr = NULL;		/* header field(s) */
	int	reqhdrsz = 0;		/* size of above */
	char	*reqhost = NULL;	/* header field "Host:" */
	int	reqhostsz = 0;		/* size of above */
	char	*reqaccept = NULL;	/* header field "Accept:" */
	int	reqacceptsz = 0;	/* size of above */
	char	*reqacceptl = NULL;	/* header field "Accept-Language:" */
	int	reqacceptlsz = 0;	/* size of above */
	uint_t	reqcontl = 0;		/* header field "Content-Length: */
	time_t	reqifmod = 0;		/* date for "If-Modified-Since:" */


	char	hdr_ifmod[] = "If-Modified-Since:";
	char	hdr_host[] = "Host:";
	char	hdr_refer[] = "Referer:";
	char	hdr_uagent[] = "User-Agent:";
	char	hdr_contl[] = "Content-Length:";
	char	hdr_nocache[] = "Pragma: no-cache";
	char	hdr_auth[] = "Authorization:";
	char	hdr_range[] = "Range:";
	char	hdr_accept[] = "Accept:";
	char	hdr_acceptl[] = "Accept-Language:";
	char	hdr_connka[] = "Connection: Keep-Alive";
	char	hdr_connclose[] = "Connection: close";

	char	http_net[] = "http://";
	char	http_all[] = "*";
	char	http_allall[] = "*/*";

	/*
	 */
	if ((np = connp->req_np) != NULL) {
		/*
		 * HTTP request header parse complete, so only in the case
		 * of a PUT/POST do we expect additional read-side data.
		 */
		mutex_enter(&np->lock);
		switch (np->method) {
		case NCA_POST:
		case NCA_PUT:
			break;
		default:
			/* Not expecting any additiona read-side data? */
			mutex_exit(&np->lock);
			mp1 = connp->tcp_rcv_head;
			connp->tcp_rcv_head = NULL;
			connp->tcp_rcv_cnt = 0;
			connp->tcp_rwnd = connp->tcp_rwnd_max;
			U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);
			freemsg(mp1);
			return (false);
			/* XXX anything else todo ? (like term conn?) */
		}
		if (np->reqcontl <= 0) {
			/* Not expecting any additiona read-side data? */
			mutex_exit(&np->lock);
			mp1 = connp->tcp_rcv_head;
			connp->tcp_rcv_head = NULL;
			connp->tcp_rcv_cnt = 0;
			connp->tcp_rwnd = connp->tcp_rwnd_max;
			U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);
			freemsg(mp1);
			return (false);
			/* XXX anything else todo ? (like term conn?) */
		}
		if (np->connhead) {
			/* Already have a request pending */
			mutex_exit(&np->lock);
			return (false);
		}

		if (connp->req_mp == NULL) {
			/*
			 * Data continues to arrive even after the request
			 * has been responded to by us, so just through away
			 * the data.
			 */
			mutex_exit(&np->lock);
			mp1 = connp->tcp_rcv_head;
			connp->tcp_rcv_head = NULL;
			connp->tcp_rcv_cnt = 0;
			connp->tcp_rwnd = connp->tcp_rwnd_max;
			U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);
			freemsg(mp1);
			return (false);
		}
		len = n_http_buf_size - sizeof (nca_io_t);
		if (len > np->reqcontl) {
			len = np->reqcontl;
		}
		if (connp->tcp_rcv_cnt >= np->reqcontl ||
		    connp->tcp_rcv_cnt >= n_http_chunk_size) {
			/*
			 * Enough data to process, so
			 * hand it off to a miss thread.
			 */
			CONN_REFHOLD(connp);
			connp->nodenext = NULL;
			np->connhead = connp;
			np->conntail = connp;
			mutex_exit(&np->lock);

			mp = dupb(connp->req_mp);
			sqfan_fill(&nca_miss_fanout1, mp, (void *)np);
		} else {
			mutex_exit(&np->lock);
		}
		return (false);
	} else if (connp->req_mp == NULL) {
		/* First time for this conn_t */
		len = 0;
		mp1 = NULL;
		mp = connp->tcp_rcv_head;

		if (nca_debug) {
			prom_printf("nca_http(%p): first time, mp = %p\n",
			    (void *)connp, (void *)mp);
		}
	} else if ((mp = (mp1 = connp->req_mp)->b_cont) != NULL) {
		/* More HTTP request header data */
		len = connp->req_parse;

		if (nca_debug) {
			prom_printf("nca_http(%p): more header data, mp = %p\n",
			    (void *)connp, (void *)mp);
		}
	} else {
		freeb(mp1);
		/* First time for this conn_t */
		len = 0;
		mp1 = NULL;
		mp = connp->tcp_rcv_head;
	}
	/* Look for HTTP request header terminator */
	for (;;) {
		mblk_t	*mp2 = mp->b_cont;
		char	*optr = (char *)mp->b_wptr;

		pp = (char *)mp->b_rptr;
		while (pp < optr) {
			c = *pp++;
			if (c == '\n') {
				len++;
				if (len == 2)
					break;
				continue;
			} else if (c == '\r') {
				continue;
			} else if (len != 0) {
				len = 0;
			}
		}
		if (len != 2) {
			connp->req_mp = mp;
			if (mp2 == NULL) {
				connp->req_parse = len;
				return (false);
			}
			mp1 = mp;
			mp = mp2;
			continue;
		}
		/*
		 * Found it, copy any mblk_t chain data throught the terminator
		 * into a contiguous data area (we use msgpullup()) and save.
		 */
		mp->b_wptr = (unsigned char *)pp;
		mp->b_cont = NULL;
		connp->req_mp = msgpullup(connp->tcp_rcv_head, -1);
		if (connp->req_mp == NULL) {
			/*
			 * XXX if kmflags == KM_SLEEP we could do an
			 * in-line version of pullup() using allocb_wait().
			 */
			connp->req_parse = -xmsgsize(connp->tcp_rcv_head);
			mp->b_wptr = (unsigned char *)optr;
			mp->b_cont = mp2;
			return (false);
		}
		/*
		 * Consume the TCP recv list for any data that was copied.
		 */
		if (pp == optr) {
			/* No tail data in this mblk_t */
			mp = mp2;
			mp1 = connp->tcp_rcv_head;
			connp->tcp_rcv_head = NULL;
			freemsg(mp1);
		} else {
			/* Tail data, leave it on the TCP rcv_list */
			mp->b_rptr = (unsigned char *)pp;
			mp->b_wptr = (unsigned char *)optr;
			mp->b_cont = mp2;
			if (mp1 != NULL) {
				mp1->b_cont = NULL;
				mp1 = connp->tcp_rcv_head;
				connp->tcp_rcv_head = NULL;
				freemsg(mp1);
			}
		}
		if (mp != NULL) {
			connp->tcp_rcv_head = mp;
			connp->tcp_rcv_cnt = msgdsize(mp);
		} else {
			connp->tcp_rcv_head = NULL;
			connp->tcp_rcv_cnt = 0;
			connp->tcp_rwnd = connp->tcp_rwnd_max;
			U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);
		}
		/* Parse the HTTP request header mblk_t *req_mp */
		break;
	}
	mp = connp->req_mp;

	/*
	 * req - full request.
	 *
	 * reqln, reqlnsz - request line.
	 *
	 * reqhdr, reqhdrsz - request header(s).
	 */
	req = (char *)mp->b_rptr;
	len = (mp->b_wptr - (unsigned char *)req);

	/* Find first line and parse request line */
	reqln = NULL;
	pp = req;
	while (pp < &req[len]) {
		if (reqln != NULL && *pp == '\n')
			break;
		if (reqln == NULL && ! isspace(*pp))
			reqln = pp;
		pp++;
	}
	if (pp == &req[len]) {
		/* No line terminated non white-space line ??? */
		methodp = &nca_method_unknown;
		goto badreq;
	}
	if (pp[-1] == '\r') {
		reqlnsz = (pp - reqln) - 1;
	} else {
		reqlnsz = (pp - reqln);
	}
	req = reqln;	/* Skip any white-space before the request line */

	/* Parse request header(s) */
	reqhdr = pp + 1;
	reqhdrsz = (mp->b_wptr - (unsigned char *)reqhdr);
	len = reqlnsz;
	for (methodp = nca_methods; methodp->s != NULL; methodp++) {
		if (len > strlen(methodp->s) &&
			(pp = strnstr(req, methodp->s, len)) == req)
			break;
	}
	if (methodp->v == NCA_GET && !no_caching) {
		/*
		 * We only cache GETs and only if not a query.
		 */
		if (strnchr(reqln, '?', reqlnsz) == NULL)
			cacheit = true;
		else {
			cacheit = false;
			NCA_DEBUG_COUNTER(&nca_nocache1, 1);
		}
	} else {
		cacheit = false;
		NCA_DEBUG_COUNTER(&nca_nocache2, 1);
	}
	if ((pp = strnchr(req, ' ', len)) == NULL) {
		goto badreq;
	}
	pp++;
	if ((len -= pp - req) == 0) {
		goto badreq;
	}
	req = pp;
	if ((pp = strnstr(req, "HTTP/", len)) != NULL) {
		uint_t	major = 0;
		uint_t	minor = 0;
		int	cnt;

		path = req;
		pathsz = (pp - req);
		req = pp;
		len -= pathsz;
		pathsz--;
		pp += 5;
		cnt = len - 5;

		while (cnt-- > 0) {
			c = *pp++;
			if (! isdigit(c))
				break;
			major *= 10;
			major += c - '0';
		}
		if (cnt <= 0) {
			goto badreq;
		}
		while (cnt-- > 0) {
			c = *pp++;
			if (! isdigit(c))
				break;
			minor *= 10;
			minor += c - '0';
		}
		if (cnt != -1) {
			goto badreq;
		}
		version = (major << 16) | minor;
	} else {
		/* No HTTP/M.M, so assume it's 0.9 */
		path = req;
		pathsz = len;
		version = 9;
	}
	if (methodp->v == NCA_UNKNOWN) {
		/* Unknown method */
		goto badreq;
	}
	if ((pp = strnstr(path, http_net, pathsz)) != NULL) {
		/* Path contains a scheme for HTTP and a net location */
		req = pp + sizeof (http_net) - 1;
		len = pathsz - (req - path);
		if ((pp = strnchr(req, '/', len)) != NULL) {
			/* Also contains a path */
			reqhost = req;
			reqhostsz = (pp - req);
			path = pp;
			pathsz = len - (pp - req);
		}
	}
	connp->reqpath = path;
	connp->reqpathsz = pathsz;

	persist = connp->http_persist;
	if (version > HTTP_1_0) {
		/*
		 * HTTP/1.1 or greater, assume persistent connection but have
		 * to check for nonpersistent (i.e. "Connection: close").
		 */
		if (strncasestr(reqhdr, hdr_connclose, reqhdrsz) != NULL) {
			connp->http_persist = false;
		} else {
			connp->http_persist = true;
		}
	} else if (version == HTTP_1_0) {
		/*
		 * HTTP/1.0, check for persistent connection requested
		 * (i.e. "Connection: Keep-Alive").
		 */
		if (strncasestr(reqhdr, hdr_connka, reqhdrsz) != NULL) {
			connp->http_persist = true;
		} else {
			connp->http_persist = false;
		}
	} else {
		/*
		 * Too old (i.e. HTTP/0.9), so no persustent connection.
		 */
		connp->http_persist = false;
	}
	if (persist && ! connp->http_persist) {
		connp->http_was_persist = true;
	}

	if ((pp = strncasestr(reqhdr, hdr_host, reqhdrsz)) != NULL) {
		/*
		 * This request has a "Host: ..." header field,
		 * so save the argument string.
		 */
		req = pp + sizeof (hdr_host);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
			reqhost = req;
			reqhostsz = (pp - req);
		}
	}
	if ((pp = strncasestr(reqhdr, hdr_refer, reqhdrsz)) != NULL) {
		/*
		 * This request has a "Referer: ..." header field,
		 * so save the argument string.
		 */
		req = pp + sizeof (hdr_refer);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
		}
	}
	if ((pp = strncasestr(reqhdr, hdr_uagent, reqhdrsz)) != NULL) {
		/*
		 * This request has a "User-Agent: ..." header field,
		 * so save the argument string.
		 */
		req = pp + sizeof (hdr_uagent);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
		}
	}
	if ((methodp->v == NCA_POST || methodp->v == NCA_PUT) &&
		(pp = strncasestr(reqhdr, hdr_contl, reqhdrsz)) != NULL) {
		/*
		 * This request has a "Content-Length: ..." header field,
		 * so save the argument value.
		 */
		req = pp + sizeof (hdr_contl);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
			len = (pp - req);
			reqcontl = atoin(req, len);
		}
	}
	if (! cacheit)
		goto nocache;

	if (methodp->v == NCA_GET &&
	    (pp = strncasestr(reqhdr, hdr_ifmod, reqhdrsz)) != NULL) {
		/*
		 * "If-Mod ..." header field, so get date value
		 * for compare to a node_t's "Last-Mod ..." value.
		 */
		req = pp + sizeof (hdr_ifmod);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			char	sc;

			if (pp[-1] == '\r')
				pp--;
			sc = *pp;
			len = pp - req;
			*pp = 0;
			reqifmod = nca_http_date(req);
			*pp = sc;
		}
	}
	if (reqifmod == 0 &&
	    (pp = strncasestr(reqhdr, hdr_nocache, reqhdrsz)) != NULL) {
		/*
		 * This request is cacheable, but has a "Pragma: no-cache"
		 * header field, so indicate not cacheable.
		 *
		 * Note: we don't check if this a GET with a "If-Mod ..."
		 *	 as we never cache the "If-Mod ..." response.
		 */
		cacheit = false;
		NCA_DEBUG_COUNTER(&nca_nocache3, 1);
		goto nocache;
	}
	if ((pp = strncasestr(reqhdr, hdr_range, reqhdrsz)) != NULL) {
		/* XXX need to add "Range:" handling */
		cacheit = false;
		NCA_DEBUG_COUNTER(&nca_nocache4, 1);
		goto nocache;
	}
	if ((pp = strncasestr(reqhdr, hdr_auth, reqhdrsz)) != NULL) {
		cacheit = false;
		NCA_DEBUG_COUNTER(&nca_nocache5, 1);
		goto nocache;
	}
	if ((pp = strncasestr(reqhdr, hdr_accept, reqhdrsz)) != NULL) {
		/*
		 * This request has a "Accept: ..." header field,
		 * so save the argument string for cache qual against
		 * a response header "Content-Type: ...".
		 *
		 * Note: we catch the special-case "ANY/ANY" here as this is
		 *	equivalent to no "Accept: ...".
		 */
		req = pp + sizeof (hdr_accept);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
			len = pp - req;
			if (len >= sizeof (http_allall))
				len = sizeof (http_allall) - 1;
			else if (len < sizeof (http_allall) - 1)
				len = 0;
			if (len == 0 || strncmp(http_allall, req, len) != 0) {
				reqaccept = req;
				reqacceptsz = pp - req;
			}
		}
	}
	if ((pp = strncasestr(reqhdr, hdr_acceptl, reqhdrsz)) != NULL) {
		/*
		 * This request has a "Accept-Language: ..." header field,
		 * so save the argument string for cache qual against
		 * a response header "Content-Language: ...".
		 *
		 * Note: we catch the special-case "*" here as this is
		 *	equivalent to no "Accept-Language: ...".
		 */
		req = pp + sizeof (hdr_acceptl);
		len = reqhdrsz - (req - reqhdr);
		if ((pp = strnchr(req, '\n', len)) != NULL) {
			if (pp[-1] == '\r')
				pp--;
			len = pp - req;
			if (strncmp(http_all, req, len) != 0) {
				reqacceptl = req;
				reqacceptlsz = len;
			}
		}
	}

nocache:;
	HASH_IX(path, pathsz, ui);
	lock = &urihash[ui].lock;
	mutex_enter(lock);
	np = urihash[ui].head;
	pp = &path[pathsz];
	while (np) {
		char *a = &np->path[np->pathsz];
		char *b = pp;

		/* Path match ? */
		do {
			if (*--a != *--b)
				break;
		} while (a > np->path && b > path);
		if (b != path || a != np->path)
			goto next;
		/* HTTP version match ? */
		if (np->version != version)
			goto next;
		/* HTTP method match ? */
		if (np->method != methodp->v)
			goto next;
		/* HTTP "Host:" match (if any) ? */
		if (np->reqhostsz != reqhostsz ||
			(reqhostsz != 0 &&
			strncasecmp(np->reqhost, reqhost, reqhostsz) != 0))
			goto next;
		/* HTTP "Accept:" match (if any) ? */
		if (np->reqacceptsz != reqacceptsz ||
			(reqacceptsz != 0 &&
			strncasecmp(np->reqaccept, reqaccept, reqacceptsz)
			!= 0))
			goto next;
		/* HTTP "Accept-Language:" match (if any) ? */
		if (np->reqacceptlsz != reqacceptlsz ||
			(reqacceptlsz != 0 &&
			strncasecmp(np->reqacceptl, reqacceptl, reqacceptlsz)
			!= 0))
			goto next;
		/* HTTP "Connection: ..." match */
		if (np->persist != connp->http_persist ||
		    np->was_persist != connp->http_was_persist)
			goto next;
		/* Full match */
		mutex_enter(&np->lock);
		goto found;
	next:;
		np = np->hashnext;
	}
found:;
	if (np != NULL && np->expire >= 0 && np->expire < lbolt) {
		/* Times up for this node_t, expire it */
		node_del(np, true);
		if (np->mp != NULL) {
			NCA_DEBUG_COUNTER(&nca_nocache6, 1);
			mp1 = np->mp;
			np->mp = NULL;
			freeb(mp1);
		} else {
			NCA_DEBUG_COUNTER(&nca_nocache6nomp, 1);
		}
		np = NULL;
	}

	if (np != NULL && !cacheit) {
		/* Invalidate the cache */
		node_del(np, true);
		mp1 = np->mp;
		np->mp = NULL;
		freeb(mp1);
		np = NULL;
	}

	if (np == NULL) {
		/*
		 * No node, let a miss thread handle it,
		 * first create a a place holder.
		 */
		np = node_add(path, pathsz, (cacheit ? urihash : NULL), kmflag);
		if (np == NULL) {
			/* Node_add failed */
			mutex_exit(lock);
			return (false);
		}
		np->method = methodp->v;
		np->version = version;
		np->reqcontl = reqcontl;
		np->req = dupb(mp);
		np->reqsz = mp->b_wptr - mp->b_rptr;
		np->reqhdr = reqhdr;
		np->reqhdrsz = reqhdrsz;
		np->reqhost = reqhost;
		np->reqhostsz = reqhostsz;
		np->reqaccept = reqaccept;
		np->reqacceptsz = reqacceptsz;
		np->reqacceptl = reqacceptl;
		np->reqacceptlsz = reqacceptlsz;
		np->persist = connp->http_persist;
		np->was_persist = connp->http_was_persist;

		np->expire = -1;
		np->mss = connp->tcp_mss;

		connp->req_np = np;

		if ((methodp->v != NCA_POST && methodp->v != NCA_PUT) ||
		    connp->tcp_rcv_cnt >= np->reqcontl ||
		    connp->tcp_rcv_cnt >= n_http_chunk_size) {
			/*
			 * Either not a POST/PUT or enough data is waiting
			 * to be processed, so pass it on to a miss thread.
			 */
			np->connhead = connp;
			np->conntail = connp;
			mutex_exit(lock);
			connp->nodenext = NULL;
			CONN_REFHOLD(connp);
			mutex_exit(&np->lock);

			NCA_COUNTER(&nca_missed1, 1);
			mp = dupb(mp);

			sqfan_fill(&nca_miss_fanout1, mp, (void *)np);
		} else {
			/*
			 * A POST/PUT and not enough data waiting to be
			 * processed, so defer processing til there is.
			 */
			mutex_exit(lock);
			mutex_exit(&np->lock);
		}
		return (false);
	}
	mutex_exit(lock);
	if (reqifmod != NULL && reqifmod >= np->lastmod) {
		/*
		 * Found a cache node_t and this request has an "If-Mod ..."
		 * header field, and "If-Mod ..." is the same as, or later
		 * then, the node_t's "Last-Mod ...", so just return a 304
		 * "Not Mod ..." response.
		 */
		mutex_exit(&np->lock);
		/* Got a hit */
		NCA_COUNTER(&nca_304hits, 1);
		np = nca_http_response(NULL, connp, nca_resp_304,
		    sizeof (nca_resp_304), path, pathsz, version);
		if (np != NULL)
			return (nca_http_miss(np, connp, kmflag));
		else
			return (NULL);
	}
	connp->req_np = np;
	if (np->conntail) {
		/*
		 * The node_t is already scheduled for, or is being handled
		 * by, nca_http_miss(), so just add our conn_t to the list.
		 */
		connp->nodenext = NULL;
		np->conntail->nodenext = connp;
		np->conntail = connp;
		CONN_REFHOLD(connp);
		mutex_exit(&np->lock);
		NCA_COUNTER(&nca_missed2, 1);
		return (false);
	}
	if ((np->ref & REF_PHYS) == 0) {
		/*
		 * Not PHYS mapped, process as a miss type 1.
		 */
		np->ref |= REF_SAFE;
		connp->nodenext = NULL;
		np->connhead = connp;
		np->conntail = connp;
		CONN_REFHOLD(connp);
		mutex_exit(&np->lock);
		NCA_COUNTER(&nca_missed3, 1);
		mp = dupb(mp);
		sqfan_fill(&nca_miss_fanout1, mp, (void *)np);
		return (false);
	}
	if ((np->ref & REF_SAFE) != 0) {
		/*
		 * Not SAFE to process this node_t for VIRT or MBLK
		 * mapping, so process as a miss type 3. Note list
		 * will be post processed by holder of SAFE.
		 */
		connp->nodenext = NULL;
		np->connhead = connp;
		np->conntail = connp;
		CONN_REFHOLD(connp);
		mutex_exit(&np->lock);
		NCA_COUNTER(&nca_missed5, 1);
		return (false);
	}
	if ((np->ref & (REF_VIRT | REF_MBLK)) != (REF_VIRT | REF_MBLK)) {
		/*
		 * Not VIRT and/or MBLK mapped, so try to handle the miss.
		 */
		np->ref |= REF_SAFE;
		if (! nca_http_miss(np, connp, KM_NOSLEEP)) {
			/*
			 * Miss wasn't fully handled, process as a miss type 2.
			 */
			connp->nodenext = NULL;
			np->connhead = connp;
			np->conntail = connp;
			CONN_REFHOLD(connp);
			mutex_exit(&np->lock);
			NCA_COUNTER(&nca_missed4, 1);
			mp = dupb(mp);
			sqfan_fill(&nca_miss_fanout2, mp, (void *)np);
			return (false);
		}
		/*
		 * Miss was handled, count it and get out'a here.
		 */
		NCA_COUNTER(&nca_missfast, 1);
	}
	if (np->fileback) {
		/*
		 * File node_t, so check for VIRT mapped, if not try to handle
		 * the miss here, else use a miss thread, if handled here or
		 * already mapped add an xmit ref to it's self ref mblk.
		 */
		node_t	*fnp = np->fileback;

		mutex_enter(&fnp->lock);
		if ((fnp->ref & REF_SAFE) != 0) {
			/*
			 * Not SAFE to process this node_t for VIRT or MBLK
			 * mapping, so process as a miss type 3. Note list
			 * will be post processed by holder of SAFE.
			 */
			connp->nodenext = NULL;
			if (fnp->connhead != NULL)
				fnp->conntail->nodenext = connp;
			else
				fnp->connhead = connp;
			fnp->conntail = connp;
			CONN_REFHOLD(connp);
			mutex_exit(&fnp->lock);
			mutex_exit(&np->lock);
			NCA_COUNTER(&nca_missed5, 1);
			return (false);
		}
		if ((fnp->ref & REF_VIRT) == NULL) {
			if (fnp->data != NULL) {
				/*
				 * File node_t was marked for VIRT reclaim,
				 * in the race between freeb()/node_fr() in
				 * vlru() and us we won, so just unmark it
				 * and reuse it.
				 */
				fnp->ref |= REF_VIRT;
			} else if (! nca_http_vmap(fnp, KM_NOSLEEP)) {
				/*
				 * Miss wasn't handled, so
				 * process as a miss type 2.
				 */
				mutex_exit(&fnp->lock);
				connp->nodenext = NULL;
				np->connhead = connp;
				np->conntail = connp;
				CONN_REFHOLD(connp);
				mutex_exit(&np->lock);
				NCA_COUNTER(&nca_missed4, 1);
				mp = dupb(mp);
				sqfan_fill(&nca_miss_fanout2, mp, (void *)np);
				return (false);
			}
			NCA_COUNTER(&nca_filemissfast1, 1);
			NCA_COUNTER(&nca_missed6, 1);
			/* Now mmap() it */
			(void) nca_http_mmap(fnp, KM_NOSLEEP);
			/* Make MRU in both phys and virt LRUs */
			LRU_BP(fnp->size, bp);
			mutex_enter(&bp->lock);
			if (fnp->plrupn != NULL) {
				/* Not phys MRU already, so delete from list */
				if ((fnp->plrupn->plrunn = fnp->plrunn) != NULL)
					fnp->plrunn->plrupn = fnp->plrupn;
				else
					bp->ptail = fnp->plrupn;
				/* Next, insert at head */
				fnp->plrupn = NULL;
				fnp->plrunn = bp->phead;
				bp->phead->plrupn = fnp;
				bp->phead = fnp;
			}
			/* Not in virt LRU, Insert at head */
			fnp->vlrupn = NULL;
			if ((fnp->vlrunn = bp->vhead) != NULL)
				bp->vhead->vlrupn = fnp;
			else
				bp->vtail = fnp;
			bp->vhead = fnp;
			bp->vcount++;
			NCA_DEBUG_COUNTER(&nca_vlrucnt, 1);
			mutex_exit(&bp->lock);
			fnp->ref &= ~REF_SAFE;
		}
		/* Last place a ref on the file node_t */
		connp->fill_fmp = dupb(fnp->mp);
		mutex_exit(&fnp->lock);
	}
	/* Got a hit */
	NCA_COUNTER(&nca_hits, 1);

	/* Make MRU for phys and virt LRUs */
	LRU_BP(np->size, bp);
	mutex_enter(&bp->lock);
	np->bucket = bp;
	/* First, phys LRU */
	if (np->plrupn != NULL) {
		/* Not MRU already, so delete from list */
		if ((np->plrupn->plrunn = np->plrunn) != NULL)
			np->plrunn->plrupn = np->plrupn;
		else
			bp->ptail = np->plrupn;
		/* Next, insert at head */
		np->plrupn = NULL;
		np->plrunn = bp->phead;
		bp->phead->plrupn = np;
		bp->phead = np;
	}
	/* Second, virt LRU */
	if (np->vlrupn != NULL) {
		/* Not MRU already, so delete from list */
		if ((np->vlrupn->vlrunn = np->vlrunn) != NULL)
			np->vlrunn->vlrupn = np->vlrupn;
		else
			bp->vtail = np->vlrupn;
		/* Next, insert at head */
		np->vlrupn = NULL;
		np->vlrunn = bp->vhead;
		bp->vhead->vlrupn = np;
		bp->vhead = np;
	}
	mutex_exit(&bp->lock);

	/* Setup xmit list */
	{
		int	sz;

		if ((sz = np->hlen) == 0) {
			sz = np->datasz;
		}
		connp->xmit.np = np;
		connp->xmit.dp = np->data;
		connp->xmit.sz = sz;
		connp->xmit.cp = (np->cksum) + 1;
		connp->xmit.fnp = np->fileback;
		if (np->hashfanout != NULL) {
			connp->fill_mp = dupb(np->mp);
		} else {
			/* Not hashed, so just uses the node_t's pointer */
			connp->fill_mp = np->mp;
			np->mp = NULL;
		}
	}

	if (connp->fill_mp == NULL) {
		NCA_COUNTER(&nca_fill_mp_NULL1, 1);
	}

	mutex_exit(&np->lock);
	/*
	 * Log the request, we want to do it as close to the end of
	 * putting the response on the wire as possible!!! For now,
	 * we're about to put the first bit on the wire (when we go
	 * into tcp TIME_WAIT would be better?).
	 */
	nca_http_logit(connp);

	return (true);

badreq:
	/*
	 * Bad request, send an appropriate response back !!!
	 *
	 */
	if (methodp->v == NCA_UNKNOWN) {
		req = nca_resp_501;
		reqsz = sizeof (nca_resp_501);
	} else {
		req = nca_resp_400;
		reqsz = sizeof (nca_resp_400);
	}
	np = nca_http_response(NULL, connp, req, reqsz, path, pathsz, version);
	if (np != NULL)
		return (nca_http_miss(np, connp, kmflag));
	else
		return (NULL);
}

/*
 * miss_conn_fr() is called to freeup any resource, remove the conn_t from
 * the miss node_t list it's a member of, and last to send a CONN_REFRELE()
 * mblk through the conn_t's if_t squeue for reference release and xmit
 * kickoff if needed (this is done so no additional locking is required).
 *
 * Note: must be called with np->lock held and will return with it not.
 */

static void
nca_miss_conn_fr(node_t *np, conn_t *connp)
{
	mblk_t *mp;

	/* Unlink the conn_t from the node_t list */

	ASSERT(np->connhead == connp);

	if ((np->connhead = connp->nodenext) == NULL)
		np->conntail = NULL;
	mutex_exit(&np->lock);

	mp = squeue_ctl(NULL, connp, CONN_MISS_DONE);
	squeue_fill(connp->inq, mp, connp->ifp);
}

static void
/*ARGSUSED*/
nca_miss_init(void *arg)
{
	static uint32_t once = 0;

	if (atomic_add_32_nv(&once, 1) != 1)
		return;

	if (nca_door_upcall_init(&nca_httpd_door_hand,
	    nca_httpd_door_path) != 0) {
		cmn_err(CE_WARN, "NCA: cannot open miss door \"%s\"",
		    nca_httpd_door_path);
	}
}

static void
nca_missed(node_t *np, mblk_t *mp, squeue_t *sqp)
{
	conn_t	*connp;
	node_t	*clone = NULL;

	/*
	 * Have a node_t miss to handle; walk the node_t's conn_t list.
	 */

	ASSERT(np != NULL);

	if (np->connhead == NULL) {
		/*
		 * Race condition between http() and missed(); it's
		 * better to let it happen and catch it here than to
		 * prevent it.
		 */
		return;
	}

	freeb(mp);
	mp = NULL;
	for (;;) {
		if (clone != NULL) {
			np = clone;
			clone = NULL;
		}
		mutex_enter(&np->lock);
		np->sqp = sqp;
		if ((connp = np->connhead) == NULL) {
			/* End of node_t conn_t walk */
			if (np->size == -1) {
				/* Cache fill failed */
				mutex_exit(&np->lock);
				node_del(np, false);
				if (np->mp != NULL) {
					freeb(np->mp);
				}
				NCA_COUNTER(&nca_missbad, 1);
				/* XXX Need to send an HTTP response */
			} else {
				/* End of conn_t list */
				np->sqp = NULL;
				mutex_exit(&np->lock);
			}
			break;
		}
		if (! connp->tcp_refed) {
			/*
			 * We hold the last ref to the conn_t, so not
			 * much point in doing the miss processing as
			 * the connection has gone away.
			 */
			NCA_COUNTER(&nca_miss1, 1);
			nca_miss_conn_fr(np, connp);
			continue;
		}

		ASSERT(connp->req_mp != NULL);

		if (! nca_http_miss(np, connp, KM_SLEEP)) {
			/*
			 * Some sort of internal error (resource, httpd, ...)
			 * so send back an appropriate HTTP response.
			 */
			mblk_t	*mp = np->mp;

			np->mp = NULL;
			NCA_COUNTER(&nca_miss2, 1);
			mutex_exit(&np->lock);
			(void) nca_http_response(np, connp, nca_resp_500,
			    sizeof (nca_resp_500), NULL, NULL, NULL);
			while ((connp = np->connhead) != NULL) {
				mutex_enter(&np->lock);
				(void) nca_http_miss(np, connp, KM_SLEEP);
				nca_miss_conn_fr(np, connp);
			}
			if (mp != NULL) {
				freemsg(mp);
			}
			return;
		}
		if (np->hashfanout == NULL && connp->nodenext != NULL) {
			/*
			 * The node_t is no longer hashed and at least one
			 * conn_t is linked for processing for the node_t
			 * (e.g. was not cacheable), so this node_t will
			 * disappear when the current conn_t is done xmiting
			 * the response.
			 *
			 * Clone the node_t and attach the conn_t list.
			 */
			conn_t	*next = connp->nodenext;

			connp->nodenext = NULL;
			np->conntail = connp;
			mp = next->req_mp;
			clone = node_add(next->reqpath, next->reqpathsz,
				false, KM_SLEEP);
			clone->method = np->method;
			clone->version = np->version;
			if (mp != NULL) {
				clone->req = dupb(mp);
				clone->reqsz = mp->b_wptr - mp->b_rptr;
			}
			clone->expire = -1;
			clone->mss = connp->tcp_mss;
			clone->connhead = next;
			mutex_exit(&clone->lock);
			/*
			 * XXX not reseting req_np for all conn_t's
			 * as they should all go through this path
			 * one at a time?
			 */
			next->req_np = clone;
		}
		if (! connp->tcp_refed) {
			/*
			 * We hold the last ref to the conn_t, so not
			 * much point in doing the xmit kickoff as
			 * the connection has gone away.
			 */
			NCA_COUNTER(&nca_missnot, 1);
			nca_miss_conn_fr(np, connp);
		} else if (np->size > 0) {
			/*
			 * Log the request, we want to do it as close to the
			 * end of putting the response on the wire as possible!
			 *
			 * For now, we're about to put the first bit on the wire
			 * (when we go into tcp TIME_WAIT would be better?).
			 */
			NCA_COUNTER(&nca_miss, 1);
			nca_http_logit(connp);
			/* Kickoff xmit processing */
			nca_miss_conn_fr(np, connp);
		} else {
			/* Must be a PUT/POST chunck */
			nca_miss_conn_fr(np, connp);
		}
	}
}

/* initializes the door handle */
static int
nca_door_upcall_init(door_handle_t *dh, char *door_path)
{
	int	error;

	if (nca_debug)
		prom_printf("nca_door_upcall_init: init door %s\n", door_path);

	if (*dh != NULL)	/* Release our previous hold (if any) */
		door_ki_rele(*dh);
	*dh = NULL;

	/*
	 * Locate the door used for upcalls
	 */
	error = door_ki_open(door_path, dh);
	if (nca_debug && error != 0)
		prom_printf("nca_door_upcall_init: cannot open door \"%s\" "
		    "(error %d)\n", door_path, error);

	if (nca_debug)
		prom_printf("nca_door_upcall_init: end\n");

	return (error);
}

/*
 * Result comes back in the same buffer as we sent to the HTTP
 * server. If it doesn't, we have a NCA HTTP protocol error as
 * httpd sent us more than it should have in a single chunk.
 *
 * Note: *res_size should be set by the caller to the max size
 * of the buffer.
 *
 */
static int
nca_door_upcall(door_handle_t *dh, char *buf, int bufsize, int *res_size,
    squeue_t *sqp)
{
	door_arg_t	da, sda;
	int		error = 0;
	int		backoff = 1;

	/*
	 * Incase initialization failed at startup (httpd wasn't started?)
	 * but is fixed now try the door init again.
	 */
	if (*dh == NULL) {
		error = nca_door_upcall_init(dh, nca_httpd_door_path);
		if (error != 0)
			return (error);
		cmn_err(CE_NOTE, "NCA: opened door \"%s\"",
		    nca_httpd_door_path);
	}

	da.data_ptr = buf;
	da.data_size = bufsize;
	da.desc_ptr = NULL;
	da.desc_num = 0;
	da.rbuf = buf;		/* web server should not alloc memory */
	da.rsize = *res_size;
	sda = da;

again:
	if ((error = door_ki_upcall(*dh, &da)) == EBADF) {
		/* Server may have died. Try rebinding */
		cmn_err(CE_WARN, "NCA: bad door descriptor -- reopening door "
		    "and retrying upcall");
		error = nca_door_upcall_init(dh, nca_httpd_door_path);
		if (error != 0) {
			cmn_err(CE_WARN, "NCA: cannot reopen door \"%s\" "
			    "(error %d)", nca_httpd_door_path, error);
			return (error);
		}
		da = sda;
		error = door_ki_upcall(*dh, &da);
	} else if (error == EAGAIN) {
		/*
		 * Door server is not available (doing a fork()?)
		 *
		 * We are called by an upcall worker thread so we
		 * can block here, do an exponential back-off.
		 */
		if (sqp != NULL) {
			squeue_pause(sqp, NULL, NULL, backoff, true);
			backoff <<= 1;
			if (backoff > 1000)
				/* 1 sec max backoff */
				backoff = 1000;
			da = sda;
			goto again;
		}
	}
	if (error) {
		if (nca_debug)
			prom_printf("nca_door_upcall: failed (error %d)\n",
			    error);
		return (error);
	}

	if (da.data_ptr != buf) {
		cmn_err(CE_WARN, "nca_door_upcall: passed up buffer %p, got "
		    "back buffer %p", (void *)buf, (void *)da.data_ptr);
		return (-1);
	}

	*res_size = da.data_size;

	if (nca_debug)
		prom_printf("nca_door_upcall: got back %d bytes\n", *res_size);

	return (error);
}

static int	nca_lbhiwat = 256;
static int	nca_lblowat = 16;
boolean_t	nca_logger_help = false;
ulong_t		nca_logger_help_wanted = 0;
ulong_t		nca_logger_help_given = 0;

#ifdef	LOG_HELP
int32_t		*nca_logger_wait;
#endif	/* LOG_HELP */

void
/*ARGSUSED*/
nca_logger_init(void *arg)
{
	int i, j, error;
	char buf[MAXPATHLEN + 1];
	caddr_t	ptr;
	int perm = 0600;
	int filemode = FCREAT | FWRITE | FTRUNC;
	vnode_t	*dvp = NULL;
	vnode_t	*vp = NULL;
	vnode_t *svp = NULL;
	struct vattr attrp;
	struct uio uio;
	struct iovec iov;

	if (nca_fio_cnt(&logfio) == 0)
		return;

	/*
	 * See if we can start logging from where we left off last time.
	 * Check if the symlink exists.
	 */
	ptr = buf;
	bcopy(nca_log_dir, ptr, strlen(nca_log_dir));
	ptr += strlen(nca_log_dir);
	bcopy(nca_current_log, ptr, strlen(nca_current_log));
	ptr += strlen(nca_current_log) + 1;
	*ptr = '\0';

	error = lookupname(buf, UIO_SYSSPACE, NO_FOLLOW, &dvp, &svp);
	if (error || svp == NULL)
		goto fresh_start;

	/* save the vnode of nca_log_dir */
	nca_fio_dvp(&logfio) = dvp;

	/* Check if the file pointed by the symlink exists */
	error = lookupname(buf, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error || vp == NULL)
		goto fresh_start;

	/* The logfile also exists. Lets resume logging from here */
	iov.iov_len = MAXPATHLEN;
	iov.iov_base = buf;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_resid = iov.iov_len;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_loffset = 0;
	uio.uio_fmode = 0;
	error = VOP_READLINK(svp, &uio, CRED());
	if (error) {
		(void) VOP_REMOVE(dvp, nca_current_log, CRED());
		goto fresh_start;
	}

	/* Null terminate the buf */
	buf[MAXPATHLEN - (int)uio.uio_resid] = '\0';

	/* See if we can find the logfile */
	nca_fio_ix(&logfio) = 0;
	for (i = 0; i < nca_fio_cnt(&logfio); i++) {
		if (bcmp(nca_fio_name(&logfio), buf, strlen(buf)) == 0) {

			nca_fio_ix(&logfio) = 0;

			/* initialize logfio */
			for (j = 0; j < nca_fio_cnt(&logfio); j++) {
				nca_fio_size(&logfio) = nca_log_size;
				nca_fio_offset(&logfio) = 0;
				nca_fio_file(&logfio) = j;
				nca_fio_vp(&logfio) = NULL;
				nca_fio_ix(&logfio)++;
			}

			nca_fio_ix(&logfio) = i;

			error = vn_open(nca_fio_name(&logfio), UIO_SYSSPACE,
					FCREAT | FAPPEND, perm,
					&nca_fio_vp(&logfio), 0, 0);
			if (error) {
				cmn_err(CE_WARN, "nca_logger_init: vn_open of "
				    "%s failed (error %d)",
				    nca_fio_name(&logfio), error);
				goto error;
			}

			attrp.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE
						| AT_SIZE;
			error = VOP_GETATTR(nca_fio_vp(&logfio), &attrp, 0, 0);
			if (error) {
				cmn_err(CE_WARN, "nca_logger_init: getattr of "
				    "%s failed", nca_fio_name(&logfio));
				goto error;
			}

			nca_fio_offset(&logfio) = (off64_t)attrp.va_size;

			/* Turn on directio */
			(void) VOP_IOCTL(nca_fio_vp(&logfio), _FIODIRECTIO,
					DIRECTIO_ON, 0, CRED(), NULL);

#ifdef	LOG_HELP
			nca_logger_wait = kmem_zalloc(sizeof (int32_t) * CPUS,
							KM_SLEEP);
#endif	/* LOG_HELP */

			/* Start logger timeout flush */
			nca_logit_flush();
			return;
		}

		nca_fio_ix(&logfio)++;
	}

fresh_start:
	nca_fio_ix(&logfio) = 0;

	for (i = 0; i < nca_fio_cnt(&logfio); i++) {
		nca_fio_size(&logfio) = nca_log_size;
		nca_fio_offset(&logfio) = 0;
		nca_fio_file(&logfio) = i;
		nca_fio_vp(&logfio) = NULL;
		nca_fio_ix(&logfio)++;
	}

	/* Open the first log file for writing */
	nca_fio_ix(&logfio) = 0;
	error = vn_open(nca_fio_name(&logfio), UIO_SYSSPACE, filemode,
				perm, &nca_fio_vp(&logfio), 0, 0);
	if (error) {
		cmn_err(CE_WARN, "nca_logger_init: vn_open of %s failed "
		    "(error %d)", nca_fio_name(&logfio), error);
		goto error;
	}

	/* Turn on directio */
	(void) VOP_IOCTL(nca_fio_vp(&logfio), _FIODIRECTIO, DIRECTIO_ON, 0,
				CRED(), NULL);

	/* We need the vnode of the parent directory for creating symlink */
	error = lookupname(nca_log_dir, UIO_SYSSPACE, FOLLOW, NULLVPP, &dvp);
	if (error || dvp == NULL) {
		cmn_err(CE_WARN, "nca_logger_init: lookupname of %s failed",
		    nca_fio_name(&logfio));
		goto error;
	}

	nca_fio_dvp(&logfio) = dvp;

	/* Get the attribute for the directory */
	error = VOP_GETATTR(nca_fio_dvp(&logfio), &attrp, 0, 0);
	if (error) {
		cmn_err(CE_WARN, "nca_logger_init: getattr of %s failed",
		    nca_fio_name(&logfio));
		goto error;
	}
	attrp.va_mask = AT_MODE | AT_TYPE;
	attrp.va_mode = 0777;
	attrp.va_type = VLNK;

	/*
	 * Make the given nca_log_file_link as a symlink to the real
	 * log file. But before remove if a symlink with same name
	 * existed from before.
	 */
	(void) VOP_REMOVE(nca_fio_dvp(&logfio), nca_current_log, CRED());
	error = VOP_SYMLINK(nca_fio_dvp(&logfio), nca_current_log, &attrp,
				nca_fio_name(&logfio), CRED());
	if (error) {
		cmn_err(CE_WARN, "nca_logger_init: symlink of %s to %s failed",
		    nca_current_log, nca_fio_name(&logfio));
		goto error;
	}

#ifdef	LOG_HELP
	nca_logger_wait = kmem_zalloc(sizeof (int32_t) * CPUS, KM_SLEEP);
#endif	/* LOG_HELP */

	/* Start logger timeout flush */
	nca_logit_flush();

	return;

error:
	nca_logging_on = 0;
	nca_logit_off();
}

void
/*ARGSUSED*/
nca_logger(void *arg, mblk_t *mp)
{
	log_buf_t	*lbp;
	int		rc;
	nca_log_buf_hdr_t *hdr;
	nca_log_stat_t	*sts;
	int		size;
	vnode_t		*vp;
	uio_t		uioin;
	iovec_t		iov;
	int		rval;
	boolean_t	noretry = false;
	struct vattr	attrp;

	if (!nca_logging_on) {
		freemsg(mp);
		return;
	}

	lbp = (log_buf_t *)mp->b_rptr;

	hdr = (nca_log_buf_hdr_t *)lbp->buffer;
	sts = &hdr->nca_logstats;
	size = sts->n_log_size + sizeof (*hdr);

	if (size & (DEV_BSIZE - 1)) {
		/* Not appropriately sized for directio(), so add some filler */
		sts->n_log_size += DEV_BSIZE - (size & (DEV_BSIZE - 1));
		size = sts->n_log_size + sizeof (*hdr);
	}

retry:
	/* Check if current logfile has sufficient space */
	if (nca_fio_offset(&logfio) + size > nca_fio_size(&logfio)) {

		/* Close the current log file */
		if (nca_fio_vp(&logfio) != NULL) {
			rc = VOP_CLOSE(nca_fio_vp(&logfio),
					FCREAT | FAPPEND | FTRUNC, 1,
					(offset_t)0, CRED());
			if (rc) {
				cmn_err(CE_WARN, "nca_logger: close of %s "
				    "failed", nca_fio_name(&logfio));
				nca_logging_on = 0;
				nca_logit_off();
				freemsg(mp);
				return;
			}
			nca_fio_vp(&logfio) = NULL;
		}

		/* Go to next file */
		nca_fio_ix(&logfio)++;
		if (nca_fio_ix(&logfio) == nca_fio_cnt(&logfio)) {
			/*
			 * We have reached the last file. If cycling
			 * is not on, disable logging and bailout.
			 */
			if (!nca_log_cycle) {
				nca_fio_ix(&logfio)--;
				nca_logging_on = 0;
				nca_logit_off();
				freemsg(mp);
				return;
			} else {
				/* Start from the first file */
				nca_fio_ix(&logfio) = 0;
			}
		}

		/* Open the next log file */
		rc = vn_open(nca_fio_name(&logfio), UIO_SYSSPACE,
				FCREAT | FWRITE | FTRUNC, 0600,
				&nca_fio_vp(&logfio), 0, 0);
		if (rc) {
			cmn_err(CE_WARN, "nca_logger: vn_open of %s failed "
			    "(error %d)", nca_fio_name(&logfio), rc);
			nca_logging_on = 0;
			nca_logit_off();
			freemsg(mp);
			return;
		}

		/* Turn on directio */
		(void) VOP_IOCTL(nca_fio_vp(&logfio), _FIODIRECTIO,
					DIRECTIO_ON, 0, CRED(), NULL);

		/* start writing from the begining of the file */
		nca_fio_offset(&logfio) = 0;

		/* Remove the current symlink also */
		(void) VOP_REMOVE(nca_fio_dvp(&logfio),
					nca_current_log, CRED());

		attrp.va_mask = AT_MODE | AT_TYPE;
		attrp.va_mode = 0777;
		attrp.va_type = VLNK;
		rc = VOP_GETATTR(nca_fio_dvp(&logfio), &attrp, 0, 0);
		if (rc) {
			cmn_err(CE_WARN, "nca_logger: getattr of %s failed",
			    nca_fio_name(&logfio));
			nca_logging_on = 0;
			nca_logit_off();
			freemsg(mp);
			return;
		}

		/* ..... and make it point to the new log file */
		rc = VOP_SYMLINK(nca_fio_dvp(&logfio), nca_current_log,
					&attrp, nca_fio_name(&logfio), CRED());
		if (rc) {
			cmn_err(CE_WARN, "nca_logger: symlink of %s to %s "
			    "failed", nca_current_log, nca_fio_name(&logfio));
			nca_logging_on = 0;
			nca_logit_off();
			freemsg(mp);
			return;
		}
		if (nca_logger_debug)
			prom_printf("nca_logger: cycle to %p(%s) @ %lld\n",
			    (void *)nca_fio_vp(&logfio), nca_fio_name(&logfio),
			    nca_fio_offset(&logfio));
	}

	vp = nca_fio_vp(&logfio);

	if (nca_logger_debug)
		prom_printf("nca_logger: write to %d, %p(%s) %d bytes @ %lld\n",
		    nca_fio_ix(&logfio), (void *)nca_fio_vp(&logfio),
		    nca_fio_name(&logfio), size, nca_fio_offset(&logfio));

	VOP_RWLOCK(vp, 1);
	iov.iov_base = lbp->buffer;
	iov.iov_len = size;
	uioin.uio_iov = &iov;
	uioin.uio_iovcnt = 1;
	uioin.uio_segflg = UIO_SYSSPACE;
	uioin.uio_fmode = 0;
	uioin.uio_loffset = (u_offset_t)nca_fio_offset(&logfio);
	uioin.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	uioin.uio_resid = size;
	rval = VOP_WRITE(vp, &uioin, 0, kcred);
	VOP_RWUNLOCK(vp, 1);
	if (rval != 0) {
		if (rval == EFBIG) {
			/* Out of space for this file, retry with the next */
			nca_fio_size(&logfio) = nca_fio_offset(&logfio);
			if (noretry) {
				nca_logging_on = false;
				goto done;
			} else
				goto retry;
		}
	}

	nca_fio_offset(&logfio) = uioin.uio_loffset;

done:
	freemsg(mp);

	if (! nca_logging_on && nca_lbmem < sizeof (log_buf_t) * nca_lblowat) {
		nca_logging_on = 1;
	}
}

static ulong_t log_buf_alloc_fail = 0;
static ulong_t log_buf_alloc_dup = 0;

void
log_buf_alloc(int kmflag)
{
	log_buf_t		*p;
	nca_log_buf_hdr_t	*hdr;

	ASSERT(MUTEX_HELD(&logbuf_lock));

	logbuf = NULL;
	mutex_exit(&logbuf_lock);
	p = kmem_cache_alloc(logbuf_cache, kmflag);
	mutex_enter(&logbuf_lock);
	if (p == NULL) {
		NCA_COUNTER(&log_buf_alloc_fail, 1);
		return;
	}
	if (logbuf != NULL) {
		mutex_exit(&logbuf_lock);
		kmem_cache_free(logbuf_cache, p);
		NCA_COUNTER(&log_buf_alloc_dup, 1);
		mutex_enter(&logbuf_lock);
		return;
	}
	NCA_COUNTER((ulong_t *)&nca_lbmem, sizeof (*p));

	p->size = NCA_DEFAULT_LOG_BUF_SIZE;
	p->cur_pos = sizeof (*hdr);

	hdr = (nca_log_buf_hdr_t *)&p->buffer;
	hdr->nca_loghdr.nca_version = NCA_LOG_VERSION1;
	hdr->nca_loghdr.nca_op = log_op;
	hdr->nca_logstats.n_log_size = NCA_DEFAULT_LOG_BUF_SIZE - sizeof (*hdr);
	hdr->nca_logstats.n_log_recs = 0;

	if (nca_debug)
		prom_printf("log_buf_alloc: kernel log buf %lu\n",
		    nca_logit_noupcall);

	hdr->nca_logstats.n_log_upcall = nca_logit_noupcall++;
	logbuf = p;
}


/* free routine to pass to desballoc to free log_buf_t's */
static void
log_buf_free(void *arg)
{
	log_buf_t *p = arg;

	if (p) {
		kmem_cache_free(logbuf_cache, p);

		NCA_COUNTER((ulong_t *)&nca_lbmem, -sizeof (*p));
	}
}

/*
 * XXX - logging should NOT be done till after the response is written
 * on the wire. This will give better latency numbers.
 *
 * NOTE: will move this code - later in the processing of the HTTP op.
 */

static void
nca_http_logit(conn_t *connp)
{
	mblk_t		*lmp;
	nca_request_log_t *req;
	char		*wp;
	char		*pep;
	int		sz;
	log_buf_t	*lbp;

	node_t		*np = connp->req_np;
	uint32_t	off = 0;
	int kmflag = connp->inq->sq_isintr ? KM_NOSLEEP : KM_SLEEP;

	if (connp->req_mp == NULL) {
		NCA_COUNTER(&nca_logit_nomp, 1);
		return;
	}
	if (!nca_logging_on) {
		return;
	}

	NCA_COUNTER(&nca_logit, 1);

	mutex_enter(&logbuf_lock);
again:
	if (logbuf == NULL) {
		mutex_exit(&logbuf_lock);
		NCA_COUNTER(&nca_logit_fail, 1);
		return;
	}
	/*
	 * Fill in a req in the current logbuf, if go past the end of the
	 * current logbuf cleanup current, allocate another, and redo.
	 */
	pep = &((char *)logbuf)[logbuf->size];
	wp = (logbuf->buffer + logbuf->cur_pos);
	wp = NCA_LOG_ALIGN(wp);
	req = (nca_request_log_t *)wp;
	wp += sizeof (*req);
	if (wp >= pep) goto full;

	sz = MIN(np->pathsz, MAX_URL_LEN);
	if ((wp + sz + 1) >= pep) goto full;
	bcopy(np->path, wp, sz);
	wp += sz;
	*wp++ = 0;
	sz++;
	req->request_url_len = sz;
	req->request_url = off;
	off += sz;

	if ((sz = connp->reqrefersz) > 0) {
		if ((wp + sz + 1) >= pep) goto full;
		bcopy(connp->reqrefer, wp, sz);
		wp += sz;
		*wp++ = 0;
		sz++;
		req->referer_len = sz;
		req->referer = off;
		off += sz;
	} else {
		req->referer_len = 0;
		req->referer = 0;
	}

	if ((sz = connp->requagentsz) > 0) {
		if ((wp + sz + 1) >= pep) goto full;
		bcopy(connp->requagent, wp, sz);
		wp += sz;
		*wp++ = 0;
		sz++;
		req->useragent_len = sz;
		req->useragent = off;
		off += sz;
	} else {
		req->useragent_len = 0;
		req->useragent = 0;
	}

	/* XXX no remote_user */
	req->remote_user_len = 0;
	req->remote_user = 0;

	/* XXX no auth_user */
	req->auth_user_len = 0;
	req->auth_user = 0;

	logbuf->cur_pos = wp - logbuf->buffer;

	req->response_status = HS_OK;

	req->response_len = (uint_t)np->datasz;

	req->start_process_time = (time32_t)(hrestime.tv_sec -
					(lbolt - connp->create));

	/* XXX Need to get near the end of xmit ? */
	req->end_process_time = (time32_t)hrestime.tv_sec;

	req->method = np->method;

	/* Fill in the request members */
	req->remote_host = connp->faddr;

	if (np->version >= HTTP_1_1)
		req->version = HTTP_1_1;
	else if (np->version >= HTTP_1_0)
		req->version = HTTP_1_0;
	else if (np->version >= HTTP_0_9)
		req->version = HTTP_0_9;
	else
		req->version = HTTP_0_0;

	((nca_log_buf_hdr_t *)logbuf)->nca_logstats.n_log_recs++;

	mutex_exit(&logbuf_lock);
	return;

full:
	wp = (logbuf->buffer + logbuf->cur_pos);
	sz = pep - wp;
	bzero(wp, sz);

	lbp = logbuf;
	lbp->ft.free_func = log_buf_free;
	lbp->ft.free_arg = (caddr_t)lbp;
	lmp = desballoc((unsigned char *)lbp, (size_t)sizeof (*lbp),
			BPRI_HI, &lbp->ft);
	log_buf_alloc(kmflag);
	if (!lmp) {
		/*
		 * The desballoc() failed, for now just cleanup and ...
		 */
		mutex_exit(&logbuf_lock);
		kmem_cache_free(logbuf_cache, lbp);
		return;
	}
	squeue_fill(&nca_log_squeue, lmp, NULL);

	if (nca_lbhiwat) {
		if (nca_lbmem > sizeof (log_buf_t) * nca_lbhiwat) {
			/*
			 * Flow-control logging.
			 *
			 * For now we just punt and turn off logging until
			 * the logger thread(s) consume some logbufs, then
			 * once below nca_lblowat logging will be turned
			 * back on.
			 */
			nca_logging_on = 0;
#ifdef	LOG_HELP
		} else if (nca_lbmem > sizeof (log_buf_t) *
				(nca_lbhiwat >> 1) &&
				(! SQ_STATE_IS(connp->inq, SQS_NOINTR))) {
			/*
			 * Half way to flow-control and not an interrupt
			 * thread, so switch the calling if_t squeue_t to
			 * no interrupt mode (i.e. deferred worker thread
			 * processing) for awhile.
			 *
			 * This will either give the system a break from
			 * interrupt processing to run the logger thread
			 * or a subsequent if_t worker thread will do a
			 * squeue_proxy() call to process the backlog.
			 */
			int	wait = TICK_TO_MSEC(connp->inq->sq_wait);
			int	ms;

			if ((ms = nca_logger_wait[CPU->cpu_seqid]) == 0) {
				ms = wait;
			} else {
				if (ms < (wait << 7)) {
					ms <<= 1;
				}
			}
			nca_logger_wait[CPU->cpu_seqid] = ms;

			nca_logger_help = true;
			NCA_COUNTER(&nca_logger_help_wanted, 1);

			if (nca_logger_debug)
				prom_printf("nca_http_logit: nointr(%p, "
				    "%d(%d:%d,%ld))\n", (void *)connp->inq, ms,
				    wait, wait << 7, lbolt);

			squeue_nointr(connp->inq, NULL, NULL, ms);
#endif	/* LOG_HELP */
		}
	}

	goto again;
}

static void
nca_logit_flush()
{
	static log_buf_t *lastlbp = NULL;
	static int	lastpos;
	mblk_t	*lmp;

	nca_logit_flush_tid = 0;

	mutex_enter(&logbuf_lock);
	if (logbuf == NULL) {
		/*
		 * No global logbuf, this is due to a log_buf_alloc()
		 * failure, all logging since has been failed, so here
		 * we try a log_buf_alloc() again.
		 */
		NCA_COUNTER(&nca_logit_flush_NULL1, 1);
		log_buf_alloc(KM_NOSLEEP);
		if (logbuf == NULL) {
			/* Still no logbuf, try again later */
			mutex_exit(&logbuf_lock);
			NCA_COUNTER(&nca_logit_flush_NULL2, 1);
			goto out;
		}
		lastlbp = NULL;
	}
	if (lastlbp != NULL && logbuf->cur_pos > (sizeof (nca_log_buf_hdr_t)) &&
		lastlbp == logbuf && lastpos == logbuf->cur_pos) {
		/*
		 * We have a logbuf and it has log data and it's the
		 * Same logbuf and pos as last time and after lock
		 * still true, so flush.
		 */
		nca_log_stat_t	*sp;
		log_buf_t	*lbp = logbuf;

		lbp->ft.free_func = log_buf_free;
		lbp->ft.free_arg = (caddr_t)lbp;

		/* XXX - we need fixed size log buffers for now !!! */
		sp = &(((nca_log_buf_hdr_t *)logbuf)->nca_logstats);
		sp->n_log_size = logbuf->cur_pos;

		if (nca_logger_debug) {
			printf("nca_logit_flush: buffer # : %d # of log recs :"
				"%d log buffer size used : %d\n",
				sp->n_log_upcall, sp->n_log_recs,
				sp->n_log_size);
		}

		lmp = desballoc((unsigned char *)lbp, (size_t)sizeof (*lbp),
				BPRI_HI, &lbp->ft);
		log_buf_alloc(KM_NOSLEEP);
		if (!lmp) {
			/*
			 * The desballoc() failed, for now just cleanup and ...
			 */
			mutex_exit(&logbuf_lock);
			kmem_cache_free(logbuf_cache, lbp);
			goto out;
		}
		squeue_fill(&nca_log_squeue, lmp, NULL);
	}
	if ((lastlbp = logbuf) != NULL)
		lastpos = logbuf->cur_pos;
	else {
		NCA_COUNTER(&nca_logit_flush_NULL3, 1);
	}
	mutex_exit(&logbuf_lock);
out:
	/* Check again in 1 second */
	nca_logit_flush_tid = timeout((pfv_t)nca_logit_flush, NULL, hz);
}

void
nca_logit_off()
{
	/*
	 * close all files.
	 */
	nca_fio_ix(&logfio) = 0;
	while (nca_fio_ix(&logfio) < nca_fio_cnt(&logfio)) {
		if (nca_fio_vp(&logfio) != NULL) {
			VOP_CLOSE(nca_fio_vp(&logfio),
					FCREAT | FWRITE | FTRUNC, 1,
					(offset_t)0, CRED());
			nca_fio_vp(&logfio) = NULL;
		}
		if (nca_fio_name(&logfio) != NULL) {
			kmem_free(nca_fio_name(&logfio),
					strlen(nca_fio_name(&logfio)) + 1);
			nca_fio_name(&logfio) = NULL;
		}
		nca_fio_dvp(&logfio) = NULL;
		nca_fio_ix(&logfio)++;
	}
	nca_fio_cnt(&logfio) = 0;
	nca_fio_ix(&logfio) = 0;
}

/*
 * find_http_buffer is overly simplistic as we assume the number
 * threads doing HTTP cache miss call handling is small (e.g. < 8)
 * If this is not true, our linear search of the g_http_buf_table
 * will impact performance. Also, the caller of this routine should
 * consider caching the buffer once it has obtained it.
 */
static char *
find_http_buffer()
{
	int	i;
	http_buf_table_t *p = g_http_buf_table;

	for (i = 0; i < n_http_buf_table; i++, p++) {
		if (p->tid == 0) {
			p->tid = (uint_t)curthread;
			return (p->buf);
		} else if (p->tid == (uint_t)curthread) {
			return (p->buf);
		}
	}

	return (NULL);
}
