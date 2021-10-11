/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipsecconf.c	1.5	99/09/07 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <stropts.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <locale.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/sockio.h>
#include <net/pfkeyv2.h>
#include <inet/ipsec_conf.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/systeminfo.h>
#include <nss_dbdefs.h>					/* NSS_BUFLEN_HOSTS */
#include <netinet/in.h>

/*
 * Buffer length to read in pattern/properties.
 */
#define	MAXLEN			1024

/*
 * Used by parse_one and parse/parse_action to communicate
 * the errors. -1 is failure, which is not defined here.
 */
enum parse_errors {PARSE_SUCCESS, PARSE_EOF};

#define	IPV4_ADDR_LEN		(sizeof (ipaddr_t))
#define	IPV4_MASK_SIZE		(IPV4_ADDR_LEN * NBBY)

/*
 * Define CURL here so that while you are reading
 * this code, it does not affect "vi" in pattern
 * matching.
 */
#define	CURL_BEGIN		'{'
#define	CURL_END		'}'
#define	MAXARGS			20

/*
 * IPSEC_CONF_ADD should start with 1, so that when multiple commands
 * are given, we can fail the request.
 */

static enum ipsec_cmds {IPSEC_CONF_ADD = 1, IPSEC_CONF_DEL, IPSEC_CONF_VIEW,
    IPSEC_CONF_FLUSH, IPSEC_CONF_LIST};

#define	POLICY_CONF_FILE	"/etc/inet/ipsecpolicy.conf"
#define	LOCK_FILE		"/var/run/.ipsecconf.lock"

/*
 * Valid algorithm length.
 */
#define	VALID_ALG_LEN		10

/* Types of Error messages */
typedef enum error_tpye {BAD_ERROR, DUP_ERROR} error_type_t;

int cmd;
char *filename;
char lo_buf[MAXLEN];				/* Leftover buffer */

/* Error reporting stuff */
#define	CBUF_LEN		4096		/* Maximum size of the cmd */
/*
 * Following are used for reporting errors with arguments.
 * We store the line numbers of each argument as we parse them,
 * so that the error reporting is more specific. We can have only
 * MAXARGS -1 for pattern and properties and one for action.
 */
#define	ARG_BUF_LEN		((2 * (MAXARGS - 1)) + 1)
int arg_indices[ARG_BUF_LEN];
int argindex;
int linecount;
char cbuf[CBUF_LEN];				/* Command buffer */
int cbuf_offset;

/*
 * Used for multi-homed source/dest hosts.
 */
struct hostent src_hent;
struct hostent dst_hent;
char src_hbuf[NSS_BUFLEN_HOSTS];
char dst_hbuf[NSS_BUFLEN_HOSTS];
boolean_t src_multi_home;
boolean_t dst_multi_home;

/*
 * We currently list 500 Policy entries when ipsecconf -l is used.
 * This should be sufficiently enough.
 */
#define	MAX_CONF_ENTRIES	500
ipsec_conf_t glob_conf[MAX_CONF_ENTRIES];

int nflag;					/* Used only with -l option */
int qflag;					/* Use only with -a option */

static int	parse_int(char *);
static int	valid_algorithm(char *, char *);
static void	usage(void);
static int	ipsec_conf_del(int, int);
static int	ipsec_conf_add(int);
static int	ipsec_conf_flush(int);
static int	ipsec_conf_view(void);
static void	ipsec_conf_list(int);
static int	lock(void);
static int	unlock(int);
static int 	parse_one(FILE *, char *[], char **, char *[]);
static void	reconfigure();

typedef struct str_val {
	char *string;
	int value;
} str_val_t;

str_val_t esp_algs[] = {
	{"ANY", 		SADB_EALG_NONE},
	{"DES", 		SADB_EALG_DESCBC},
	{"DES-CBC", 		SADB_EALG_DESCBC},
	{"3DES", 		SADB_EALG_3DESCBC},
	{"3DES-CBC", 		SADB_EALG_3DESCBC},
	{"NULL", 		SADB_EALG_NULL},
	{NULL,			0}
};

str_val_t ah_algs[] = {
	{"ANY", 		SADB_AALG_NONE},
	{"MD5", 		SADB_AALG_MD5HMAC},
	{"HMAC-MD5", 		SADB_AALG_MD5HMAC},
	{"SHA1", 		SADB_AALG_SHA1HMAC},
	{"HMAC-SHA1", 		SADB_AALG_SHA1HMAC},
	{"SHA", 		SADB_AALG_SHA1HMAC},
	{"HMAC-SHA", 		SADB_AALG_SHA1HMAC},
	{NULL, 			0}
};

str_val_t pattern_table[] = {
	{"saddr", 		IPSEC_CONF_SRC_ADDRESS},
	{"daddr", 		IPSEC_CONF_DST_ADDRESS},
	{"sport", 		IPSEC_CONF_SRC_PORT},
	{"dport", 		IPSEC_CONF_DST_PORT},
	{"smask", 		IPSEC_CONF_SRC_MASK},
	{"dmask", 		IPSEC_CONF_DST_MASK},
	{"ulp", 		IPSEC_CONF_ULP},
	{NULL, 			0},
};

str_val_t action_table[] = {
	{"apply", 		IPSEC_POLICY_APPLY},
	{"permit", 		IPSEC_POLICY_DISCARD},
	{"bypass", 		IPSEC_POLICY_BYPASS},
	{NULL, 			0},
};

str_val_t property_table[] = {
	{"auth_algs", 		IPSEC_CONF_IPSEC_AALGS},
	{"encr_algs", 		IPSEC_CONF_IPSEC_EALGS},
	{"encr_auth_algs",	IPSEC_CONF_IPSEC_EAALGS},
	{"sa",			IPSEC_CONF_IPSEC_SA},
	{"dir",			IPSEC_CONF_IPSEC_DIR},
	{NULL,			0},
};

sigset_t set, oset;

static int
block_all_signals()
{
	if (sigfillset(&set) == -1) {
		perror("sigfillset");
		return (-1);
	}
	if (sigprocmask(SIG_SETMASK, &set, &oset) == -1) {
		perror("sigprocmask");
		return (-1);
	}
	return (0);
}

static int
restore_all_signals()
{
	if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1) {
		perror("sigprocmask");
		return (-1);
	}
	return (0);
}

int
str_ioctl(int fd, int ipsec_cmd, ipsec_conf_t *conf, int num)
{
	struct strioctl stri;

	stri.ic_cmd = ipsec_cmd;
	stri.ic_timout = 0;
	stri.ic_len = num * sizeof (ipsec_conf_t);
	stri.ic_dp = (char *)conf;

	if (ioctl(fd, I_STR, &stri) == -1) {
		perror("ioctl");
		return (-1);
	}
	return (stri.ic_len);
}

int
load_ipsec(void)
{
	int s;

	s = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
	if (s == -1) {
		perror(gettext("(loading IPsec) socket:"));
		return (errno);
	}
	(void) close(s);
	return (0);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ret;
	int c;
	int index;
	int fd;
	int lfd;
#if 0
	/* See the next #if 0 for why. */
	struct passwd *pwd;
	uid_t uid;
#endif

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	openlog("ipsecconf", LOG_CONS, LOG_AUTH);
	if (getuid() != 0) {
		fprintf(stderr, gettext
		    ("You must be root to run ipsecconf.\n"));
#if 0
		/*
		 * While a good idea in theory, logging this error can
		 * open up a denial-of-service hole.
		 *
		 * When a better auditing facility is available, this sort
		 * of reporting should be in this if statement.
		 */
		uid = getuid();
		pwd = getpwuid(uid);
		if (pwd != NULL) {
			syslog((LOG_NOTICE|LOG_AUTH),
			    gettext("User %s tried to run ipsecconf.\n"),
			    pwd->pw_name);
		} else {
			syslog((LOG_NOTICE|LOG_AUTH),
			    gettext("User id %d tried to run ipsecconf. "
			    "getpwuid(%d) failed\n"), uid, uid);
		}
#endif
		exit(1);
	}
	if (argc == 1) {
		cmd = IPSEC_CONF_VIEW;
		goto done;
	}
	while ((c = getopt(argc, argv, "nlfa:qd:")) != EOF) {
		switch (c) {
		case 'f':
			/* Only one command at a time */
			if (cmd != 0) {
				usage();
				exit(1);
			}
			cmd = IPSEC_CONF_FLUSH;
			break;
		case 'l':
			/* Only one command at a time */
			if (cmd != 0) {
				usage();
				exit(1);
			}
			cmd = IPSEC_CONF_LIST;
			break;
		case 'a':
			/* Only one command at a time */
			if (cmd != 0) {
				usage();
				exit(1);
			}
			cmd = IPSEC_CONF_ADD;
			filename = optarg;
			break;
		case 'd':
			/* Only one command at a time */
			if (cmd != 0) {
				usage();
				exit(1);
			}
			cmd = IPSEC_CONF_DEL;
			index = parse_int(optarg);
			break;
		case 'n' :
			nflag++;
			break;
		case 'q' :
			qflag++;
			break;
		default :
			usage();
			exit(1);
		}
	}

	if ((fd = open("/dev/ip", O_RDWR)) == -1) {
		perror("open");
		exit(1);
	}
done:
	ret = 0;
	lfd = lock();
	if (lfd == -1) {
		exit(1);
	}
	/*
	 * ADD, FLUSH, DELETE needs to do two operations.
	 *
	 * 1) Update/delete/empty the POLICY_CONF_FILE.
	 * 2) Make an ioctl and tell IP to update its state.
	 *
	 * We already lock()ed so that only one instance of this
	 * program runs. We also need to make sure that the above
	 * operations are atomic i.e we don't want to update the file
	 * and get interrupted before we could tell IP. To make it
	 * atomic we block all the signals and restore them.
	 */
	switch (cmd) {
	case IPSEC_CONF_LIST:
		if ((ret = load_ipsec()) != 0)
			break;
		ipsec_conf_list(fd);
		break;
	case IPSEC_CONF_FLUSH:
		if ((ret = block_all_signals()) == -1) {
			break;
		}
		ret = ipsec_conf_flush(fd);
		(void) restore_all_signals();
		break;
	case IPSEC_CONF_VIEW:
		ret = ipsec_conf_view();
		break;
	case IPSEC_CONF_DEL:
		if (index == -1) {
			fprintf(stderr, gettext("Invalid index\n"));
			ret = -1;
			break;
		}
		if ((ret = load_ipsec()) != 0)
			break;
		if ((ret = block_all_signals()) == -1) {
			break;
		}
		ret = ipsec_conf_del(fd, index);
		(void) restore_all_signals();
		break;
	case IPSEC_CONF_ADD:
		if ((ret = load_ipsec()) != 0)
			break;
		if ((ret = block_all_signals()) == -1) {
			break;
		}
		ret = ipsec_conf_add(fd);
		(void) restore_all_signals();
		break;
	default :
		/* If no argument is given but a "-" */
		usage();
		exit(1);
	}
	(void) unlock(lfd);
	if (ret != 0)
		ret = 1;
	return (ret);
}

static int
lock()
{
	int fd;
	struct stat sbuf1;
	struct stat sbuf2;

	/*
	 * Open the file with O_CREAT|O_EXCL. If it exists already, it
	 * will fail. If it already exists, check whether it looks like
	 * the one we created.
	 */
	(void) umask(0077);
	if ((fd = open(LOCK_FILE, O_EXCL|O_CREAT|O_RDWR, S_IRUSR|S_IWUSR))
	    == -1) {
		if (errno != EEXIST) {
			/* Some other problem. */
			fprintf(stderr, gettext("Cannot open lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
		/*
		 * File exists. make sure it is OK. We need to lstat()
		 * as fstat() stats the file pointed to by the symbolic
		 * link.
		 */
		if (lstat(LOCK_FILE, &sbuf1) == -1) {
			perror("lstat");
			fprintf(stderr, gettext("Cannot lstat lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
		/*
		 * Check whether it is a regular file and not a symbolic
		 * link. Its link count should be 1. The owner should be
		 * root and the file should be empty.
		 */
		if (((sbuf1.st_mode & (S_IFREG|S_IFLNK)) != S_IFREG) ||
		    sbuf1.st_nlink != 1 ||
		    sbuf1.st_uid != 0 ||
		    sbuf1.st_size != 0) {
			fprintf(stderr, gettext("Bad lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
		if ((fd = open(LOCK_FILE, O_CREAT|O_RDWR,
		    S_IRUSR|S_IWUSR)) == -1) {
			perror("open");
			fprintf(stderr, gettext("Cannot open lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
		/*
		 * Check whether we opened the file that we lstat()ed.
		 */
		if (fstat(fd, &sbuf2) == -1) {
			perror("fstat");
			fprintf(stderr, gettext("Cannot fstat lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
		if (sbuf1.st_dev != sbuf2.st_dev ||
		    sbuf1.st_ino != sbuf2.st_ino) {
			/* File changed after we did the lstat() above */
			fprintf(stderr, gettext("Bad lock file %s\n"),
			    LOCK_FILE);
			return (-1);
		}
	}
	if (lockf(fd, F_LOCK, 0) == -1) {
		perror("lockf");
		return (-1);
	}
	return (fd);
}

static int
unlock(int fd)
{
	if (lockf(fd, F_ULOCK, 0) == -1) {
		perror("lockf");
		return (-1);
	}
	return (0);
}

static void
print_pattern_string(int type)
{
	int j;

	for (j = 0; pattern_table[j].string != NULL; j++) {
		if (type == pattern_table[j].value) {
			printf("%s ", pattern_table[j].string);
			return;
		}
	}
}

static void
print_dir_string(ipsec_conf_t *cptr)
{
	if (cptr->ipsc_dir == IPSEC_TYPE_OUTBOUND) {
		printf("dir out ");
	} else {
		printf("dir in ");
	}
}

static void
print_action_string(ipsec_conf_t *cptr)
{
	int j;

	for (j = 0; action_table[j].string != NULL; j++) {
		if (cptr->ipsc_policy == action_table[j].value) {
			printf("%s ", action_table[j].string);
			return;
		}
	}
}

static void
print_property_string(int type)
{
	int j;

	for (j = 0; property_table[j].string != NULL; j++) {
		if (type == property_table[j].value) {
			printf("%s ", property_table[j].string);
			return;
		}
	}
}

static void
print_auth_algs(ipsec_conf_t *cptr, int type)
{
	int alg_value, is_algs;
	int i;

	if (type == IPSEC_CONF_IPSEC_AALGS) {
		is_algs = cptr->ipsc_no_of_ah_algs;
		alg_value = cptr->ipsc_ah_algs[0];
	} else {
		is_algs = cptr->ipsc_no_of_esp_auth_algs;
		alg_value = cptr->ipsc_esp_auth_algs[0];
	}
	if (is_algs == 0)
		return;

	print_property_string(type);

	for (i = 0; ah_algs[i].string != NULL; i++) {
		if (ah_algs[i].value ==  alg_value) {
			printf("%s ", ah_algs[i].string);
			return;
		}
	}
}

static void
print_encr_algs(ipsec_conf_t *cptr)
{
	int i, alg_value;

	if (cptr->ipsc_no_of_esp_algs == 0)
		return;

	alg_value = cptr->ipsc_esp_algs[0];

	print_property_string(IPSEC_CONF_IPSEC_EALGS);

	for (i = 0; esp_algs[i].string != NULL; i++) {
		if (esp_algs[i].value == alg_value) {
			printf("%s ", esp_algs[i].string);
			break;
		}
	}
	print_auth_algs(cptr, IPSEC_CONF_IPSEC_EAALGS);
}

static void
print_sa(ipsec_conf_t *cptr)
{
	/*
	 * There are no bypass policy for bypass entries.
	 */
	if (cptr->ipsc_policy == IPSEC_POLICY_BYPASS)
		return;

	if (cptr->ipsc_sa_attr == IPSEC_SHARED_SA) {
		printf("sa shared ");
	} else {
		printf("sa unique ");
	}
}

static void
print_ulp(ipsec_conf_t *cptr)
{
	struct protoent *pe;

	if (cptr->ipsc_ulp_prot == 0)
		return;

	print_pattern_string(IPSEC_CONF_ULP);
	pe = NULL;
	if (!nflag) {
		pe = getprotobynumber(cptr->ipsc_ulp_prot);
	}
	if (pe) {
		printf("%s ", pe->p_name);
	} else {
		printf("%d ", cptr->ipsc_ulp_prot);
	}
}

static void
print_port(ipsec_conf_t *cptr, int type)
{
	in_port_t port;
	struct servent *sp;

	if (type == IPSEC_CONF_SRC_PORT) {
		port = htons(cptr->ipsc_src_port);
	} else {
		port = htons(cptr->ipsc_dst_port);
	}

	if (port == 0)
		return;

	print_pattern_string(type);
	sp = NULL;
	if (!nflag) {
		sp = getservbyport(port, NULL);
	}
	if (sp) {
		printf("%s ", sp->s_name);
	} else {
		printf("%d ", port);
	}
}

static void
print_mask(ipsec_conf_t *cptr, int type)
{
	in_addr_t addr;

	if (type == IPSEC_CONF_SRC_MASK) {
		bcopy(cptr->ipsc_src_addr, (char *)&addr, IPV4_ADDR_LEN);
	} else {
		bcopy(cptr->ipsc_dst_addr, (char *)&addr, IPV4_ADDR_LEN);
	}
	/*
	 * If the address is INADDR_ANY, don't print the mask.
	 */

	if (addr == INADDR_ANY)
		return;

	print_pattern_string(type);

	if (type == IPSEC_CONF_SRC_MASK) {
		printf("%d.%d.%d.%d ",
		    cptr->ipsc_src_mask[0],
		    cptr->ipsc_src_mask[1],
		    cptr->ipsc_src_mask[2],
		    cptr->ipsc_src_mask[3]);
	} else {
		printf("%d.%d.%d.%d ",
		    cptr->ipsc_dst_mask[0],
		    cptr->ipsc_dst_mask[1],
		    cptr->ipsc_dst_mask[2],
		    cptr->ipsc_dst_mask[3]);
	}
}

/*
 * Print the address and mask.
 */
static void
print_address(ipsec_conf_t *cptr, int type)
{
	char  *cp;
	struct hostent *hp;
	char	domain[MAXHOSTNAMELEN + 1];
	in_addr_t addr;

	if (type == IPSEC_CONF_SRC_ADDRESS) {
		bcopy(cptr->ipsc_src_addr, (char *)&addr, IPV4_ADDR_LEN);
	} else {
		bcopy(cptr->ipsc_dst_addr, (char *)&addr, IPV4_ADDR_LEN);
	}

	/*
	 * Check for INADDR_ANY for, which we don't print.
	 */
	if (addr == INADDR_ANY)
		return;

	print_pattern_string(type);

	cp = NULL;
	if (!nflag) {
		if (sysinfo(SI_HOSTNAME, domain, MAXHOSTNAMELEN) != -1 &&
			(cp = strchr(domain, '.')) != NULL) {
			(void) strcpy(domain, cp + 1);
		} else {
			domain[0] = 0;
		}
		/* IPv6 : Fix needed for IPv6 */
		if (type == IPSEC_CONF_SRC_ADDRESS) {
			hp = gethostbyaddr((char *)cptr->ipsc_src_addr,
			    IPV4_ADDR_LEN, AF_INET);
		} else {
			hp = gethostbyaddr((char *)cptr->ipsc_dst_addr,
			    IPV4_ADDR_LEN, AF_INET);
		}
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) != 0 &&
					strcasecmp(cp + 1, domain) == 0)
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (cp) {
		printf("%s ", cp);
	} else {
		/* IPv6 : Fix needed for IPv6 */
		if (type == IPSEC_CONF_SRC_ADDRESS) {
			printf("%d.%d.%d.%d ",
			    cptr->ipsc_src_addr[0],
			    cptr->ipsc_src_addr[1],
			    cptr->ipsc_src_addr[2],
			    cptr->ipsc_src_addr[3]);
		} else {
			printf("%d.%d.%d.%d ",
			    cptr->ipsc_dst_addr[0],
			    cptr->ipsc_dst_addr[1],
			    cptr->ipsc_dst_addr[2],
			    cptr->ipsc_dst_addr[3]);
		}
	}
}

static void
ipsec_conf_list(int fd)
{
	int ret;
	int nconf;
	int i;

	ret = str_ioctl(fd, SIOCLIPSECONFIG, (ipsec_conf_t *)&glob_conf,
		    MAX_CONF_ENTRIES);
	nconf = ret/sizeof (ipsec_conf_t);
	for (i = 0; i < nconf; i++) {

		printf("%c ", CURL_BEGIN);
		print_address(&glob_conf[i], IPSEC_CONF_SRC_ADDRESS);
		print_mask(&glob_conf[i], IPSEC_CONF_SRC_MASK);
		print_address(&glob_conf[i], IPSEC_CONF_DST_ADDRESS);
		print_mask(&glob_conf[i], IPSEC_CONF_DST_MASK);
		print_port(&glob_conf[i], IPSEC_CONF_SRC_PORT);
		print_port(&glob_conf[i], IPSEC_CONF_DST_PORT);
		print_ulp(&glob_conf[i]);
		printf("%c ", CURL_END);

		print_action_string(&glob_conf[i]);

		printf("%c ", CURL_BEGIN);
		print_auth_algs(&glob_conf[i], IPSEC_CONF_IPSEC_AALGS);
		print_encr_algs(&glob_conf[i]);
		print_sa(&glob_conf[i]);
		print_dir_string(&glob_conf[i]);
		printf("%c\n", CURL_END);
	}
}

static int
ipsec_conf_view()
{
	char buf[MAXLEN];
	FILE *fp;

	fp = fopen(POLICY_CONF_FILE, "r");
	if (fp == NULL) {
		perror("fopen");
		fprintf(stderr, gettext("%s cannot be opened\n"),
		    POLICY_CONF_FILE);
		return (-1);
	}
	while (fgets(buf, MAXLEN, fp) != NULL) {
		/* Don't print removed entries */
		if (*buf == ';')
			continue;
		if (strlen(buf) != 0)
			buf[strlen(buf) - 1] = '\0';
		puts(buf);
	}
	return (0);
}

/*
 * Delete nlines from start in the POLICY_CONF_FILE.
 */
static int
delete_from_file(int start, int nlines)
{
	FILE *fp;
	char ibuf[MAXLEN];
	int len;

	if ((fp = fopen(POLICY_CONF_FILE, "r+b")) == NULL) {
		perror("fopen");
		fprintf(stderr, gettext("%s cannot be opened\n"),
		    POLICY_CONF_FILE);
		return (-1);
	}

	/*
	 * Insert a ";", read the line and discard it. Repeat
	 * this logic nlines - 1 times. For the last line there
	 * is just a newline character. We can't just insert a
	 * single ";" character instead of the newline character
	 * as it would affect the next line. Thus when we comment
	 * the last line we seek one less and insert a ";"
	 * character, which will replace the newline of the
	 * penultimate line with ; and newline of the last line
	 * will become part of the previous line.
	 */
	do {
		/*
		 * It is not enough to seek just once and expect the
		 * subsequent fgets below to take you to the right
		 * offset of the next line. fgets below seems to affect
		 * the offset. Thus we need to seek, replace with ";",
		 * and discard a line using fgets for every line.
		 */
		if (fseek(fp, start, SEEK_SET) == -1) {
			perror("fseek");
			return (-1);
		}
		if (fputc(';', fp) < 0) {
			perror("fputc");
			return (-1);
		}
		/*
		 * Flush the above ";" character before we do the fgets().
		 * Without this, fgets() gets confused with offsets.
		 */
		fflush(fp);
		len = 0;
		while (fgets(ibuf, MAXLEN, fp) != NULL) {
			len += strlen(ibuf);
			if (ibuf[len - 1] == '\n') {
				/*
				 * We have read a complete line.
				 */
				break;
			}
		}
		/*
		 * We read the line after ";" character has been inserted.
		 * Thus len does not count ";". To advance to the next line
		 * increment by 1.
		 */
		start += (len + 1);
		/*
		 * If nlines == 2, we will be commenting out the last
		 * line next, which has only one newline character.
		 * If we blindly replace it with ";", it will  be
		 * read as part of the next line which could have
		 * a INDEX string and thus confusing ipsec_conf_view.
		 * Thus, we seek one less and replace the previous
		 * line's newline character with ";", and the
		 * last line's newline character will become part of
		 * the previous line.
		 */
		if (nlines == 2)
			start--;
	} while (--nlines != 0);
	fclose(fp);
	if (nlines != 0)
		return (-1);
	else
		return (0);
}

/*
 * Delete an entry from the file by inserting a ";" at the
 * beginning of the lines to be removed.
 */
static int
ipsec_conf_del(int fd, int policy_index)
{
	char *pattern[MAXARGS + 1];
	char *action;
	char *properties[MAXARGS + 1];
	char *buf;
	struct strioctl stri;
	FILE *fp;
	char ibuf[MAXLEN];
	int ibuf_len, index_len, index;
	int ret, offset, prev_offset;
	int nlines;

	fp = fopen(POLICY_CONF_FILE, "r");
	if (fp == NULL) {
		perror("fopen");
		fprintf(stderr, gettext("%s cannot be opened\n"),
		    POLICY_CONF_FILE);
		return (-1);
	}

	index_len = strlen("#INDEX");
	index = 0;
	offset = prev_offset = 0;
	while (fgets(ibuf, MAXLEN, fp) != NULL) {
		prev_offset = offset;
		ibuf_len = strlen(ibuf);

		if (strncmp(ibuf, "#INDEX", index_len) == 0) {
			/*
			 * This line contains #INDEX
			 */
			buf = ibuf + index_len;
			buf++;			/* Skip the space */
			index = parse_int(buf);
			if (index == -1) {
				fprintf(stderr, gettext("Invalid index in"
				    " the file\n"));
				return (-1);
			}
			if (index == policy_index) {
				ret = parse_one(fp, pattern, &action,
				    properties);
				if (ret == -1) {
					fprintf(stderr, gettext("Invalid "
					    "policy entry in the file\n"));
					return (-1);
				}
				/*
				 * nlines is the number of lines we should
				 * comment out. linecount tells us how many
				 * lines this command spans. And we need to
				 * remove the line with INDEX and an extra
				 * line we added during ipsec_conf_add.
				 *
				 * NOTE : If somebody added a policy entry
				 * which does not have a newline,
				 * ipsec_conf-add() fills in the newline.
				 * Hence, there is always 2 extra lines
				 * to delete.
				 */
				nlines = linecount + 2;
				goto delete;
			}
		}
		offset += ibuf_len;
	}

	fprintf(stderr, gettext("Invalid index\n"));
	fclose(fp);
	return (-1);
delete:
	/* Delete nlines from prev_offset */
	fclose(fp);
	ret = delete_from_file(prev_offset, nlines);

	if (ret != 0) {
		fprintf(stderr, gettext("Deletion incomplete. Please "
		    "flush all the entries and re-configure :\n"));
		reconfigure();
		return (ret);
	}

	stri.ic_cmd = SIOCDIPSECONFIG;
	stri.ic_timout = 0;
	stri.ic_len = sizeof (int);
	stri.ic_dp = (char *)&policy_index;

	if (ioctl(fd, I_STR, &stri) == -1) {
		perror("ioctl");
		fprintf(stderr, gettext("Deletion incomplete, Please "
		    "flush all the entries and re-configure :\n"));
		reconfigure();
		return (-1);
	}
	return (0);
}

static int
ipsec_conf_flush(int fd)
{
	ipsec_conf_t conf;
	int pfd;
	int ret;

	/* Truncate the file */
	if ((pfd = open(POLICY_CONF_FILE, O_TRUNC|O_RDWR)) == -1) {
		perror("open");
		fprintf(stderr, gettext("%s cannot be truncated\n"),
		    POLICY_CONF_FILE);
		return (-1);
	}

	/*
	 * Pass a dummy argument. IP expects an argument
	 * always. This is better than fixing IP.
	 */
	ret = str_ioctl(fd, SIOCFIPSECONFIG, &conf, 1);
	if (ret == -1) {
		perror("ioctl");
		close(pfd);
		fprintf(stderr, gettext("Flush failed\n"));
		return (-1);
	}
	close(pfd);
	return (0);
}

static void
reconfigure()
{
	fprintf(stderr, gettext(
		"\tipsecconf -f \n "
		"\tipsecconf -a policy_file\n"));
}

static void
usage(void)
{
	fprintf(stderr, gettext(
		"Usage:	ipsecconf\n"
		"\tipsecconf -a <filename> [-q]\n"
		"\tipsecconf -d <index>\n"
		"\tipsecconf -l [-n]\n"
		"\tipsecconf -f\n"));
}

int
setmask(int prefix_len)
{
	int i;
	int j = 0;

	prefix_len = IPV4_MASK_SIZE - prefix_len;

	for (i = prefix_len; i < IPV4_MASK_SIZE; i++)
		j |= (1 << i);

	return (j);
}

static int
parse_int(char *str)
{
	char *end;
	int res;

	res = strtol(str, &end, 0);
	if (end == str)
		return (-1);
	return (res);
}

/*
 * Form a mask by looking at whether any part of the
 * address has zeroes. This is used only if the user
 * has not given the mask.
 */
static in_addr_t
in_makemask(in_addr_t addr)
{
	in_addr_t mask;

	if ((addr & IN_CLASSA_HOST) == 0)
		mask =  IN_CLASSA_NET;
	else if ((addr & IN_CLASSB_HOST) == 0)
		mask =  IN_CLASSB_NET;
	else if ((addr & IN_CLASSC_HOST) == 0)
		mask =  IN_CLASSC_NET;
	else
		mask = (in_addr_t)-1;

	return (mask);
}

static int
parse_address(int type, char *addr_str, ipsec_conf_t *cptr)
{
	char *ptr;
	in_addr_t addr;
	in_addr_t mask;
	int prefix_len;
	struct netent *ne;
	struct hostent *hp;
	int h_errno;

	ptr = strchr(addr_str, '/');
	if (ptr != NULL) {
		*ptr++ = NULL;
		prefix_len = parse_int(ptr);

		/* XXX Fix needed for IPv6 */
		if (prefix_len == (unsigned int)-1 ||
		    (int)prefix_len <= 0 || prefix_len > 32) {
			return (-1);
		}
		if (type == IPSEC_CONF_SRC_ADDRESS) {
			mask = htonl(setmask(prefix_len));
			bcopy((uint8_t *)&mask, cptr->ipsc_src_mask,
			    IPV4_ADDR_LEN);
		} else {
			mask = htonl(setmask(prefix_len));
			bcopy((uint8_t *)&mask, cptr->ipsc_dst_mask,
			    IPV4_ADDR_LEN);
		}
	}
	/*
	 * We use gethostbyname_r because we need to keep the source
	 * address around when we parse the destination address.
	 */
	if (type == IPSEC_CONF_SRC_ADDRESS) {
		hp = gethostbyname_r(addr_str, &src_hent, src_hbuf,
		    NSS_BUFLEN_HOSTS, &h_errno);
	} else {
		hp = gethostbyname_r(addr_str, &dst_hent, dst_hbuf,
		    NSS_BUFLEN_HOSTS, &h_errno);
	}

	if (hp == NULL) {
		/*
		 * Check whether we overflowed the buffers. For all other
		 * errors we try and look for a network name.
		 */
		if (errno == ERANGE) {
			perror("gethostbyname_r");
			return (-1);
		}
	}

	if (hp != NULL) {
		/*
		 * We come here for both a hostname and
		 * any host address /network address.
		 */
		switch (hp->h_addrtype) {
		case AF_INET:
			bcopy(hp->h_addr_list[0], (uint8_t *)&addr,
			    hp->h_length);
			break;
		default:
			fprintf(stderr,
			    "Address type %d not supported.\n",
			    hp->h_addrtype);
			return (-1);
		}
		if (hp->h_addr_list[1] != NULL) {
			if (type == IPSEC_CONF_SRC_ADDRESS) {
				src_multi_home = B_TRUE;
			} else {
				dst_multi_home = B_TRUE;
			}
		}
	} else if ((ne = getnetbyname(addr_str)) != NULL) {
		switch (ne->n_addrtype) {
		case AF_INET:
			bcopy((uint8_t *)&ne->n_net,
			    (uint8_t *)&addr, IPV4_ADDR_LEN);
			break;
		default:
			fprintf(stderr,
			    "Address type %d not supported.\n",
			    ne->n_addrtype);
			return (-1);
		}
	} else {
		return (-1);
	}
done:
	/* XXX Fix needed for IPv6 */
	if (type == IPSEC_CONF_SRC_ADDRESS) {
		bcopy((uint8_t *)&addr, cptr->ipsc_src_addr, IPV4_ADDR_LEN);
	} else {
		bcopy((uint8_t *)&addr, cptr->ipsc_dst_addr, IPV4_ADDR_LEN);
	}
	return (0);
}

/*
 * Called at the end to add policy if saddr or daddr
 * resolved to a multi-homed address. We add the policy
 * for all other interfaces.
 */
static void
do_multi_home(int fd, ipsec_conf_t *cptr)
{
	int i;
	int j;
	int ret = 0;
	in_addr_t addr;
	char *ptr[2];
	struct hostent hent;
	struct hostent *shp, *dhp;

	shp = &src_hent;
	dhp = &dst_hent;
	if (!dst_multi_home && src_multi_home) {
		fprintf(stderr, gettext("Multiple Source "
		    " addresses found for %s.\n Adding multiple "
		    " Policy entries for them.\n"), shp->h_name);
		/*
		 * dst_hent may not be initialized if a destination
		 * address was not given. It will be initalized with just
		 * one address if a destination address was given. In both
		 * the cases, we initialize here with ipsc_dst_addr and enter
		 * the loop below.
		 */
		hent.h_addr_list = ptr;
		ptr[0] = (char *)&addr;
		ptr[1] = NULL;
		dhp = &hent;
		bcopy(cptr->ipsc_dst_addr, (uint8_t *)&addr, IPV4_ADDR_LEN);
	}
	if (dst_multi_home && !src_multi_home) {
		fprintf(stderr, gettext("Multiple Destination "
		    " addresses found for %s.\n Adding multiple "
		    " Policy entries for them.\n"), dhp->h_name);
		/*
		 * src_hent may not be initialized if a source
		 * address was not given. It will be initalized with just
		 * one address if a source address was given. In both
		 * the cases, we initialize here with ipsc_src_addr and enter
		 * the loop below.
		 */
		hent.h_addr_list = ptr;
		ptr[0] = (char *)&addr;
		ptr[1] = NULL;
		shp = &hent;
		bcopy(cptr->ipsc_src_addr, (uint8_t *)&addr, IPV4_ADDR_LEN);
	}
	if (dst_multi_home && src_multi_home) {
		fprintf(stderr, gettext("Multiple Source and Destination "
		    " addresses found for %s and %s.\n Adding multiple "
		    " Policy entries for them.\n"), shp->h_name, dhp->h_name);
	}
	for (i = 0; shp->h_addr_list[i] != NULL; i++) {
		bcopy(shp->h_addr_list[i], cptr->ipsc_src_addr,
		    IPV4_ADDR_LEN);
		/*
		 * We have already added the pair for (i=0, j=0).
		 */
		(i == 0) ? (j = 1) : (j = 0);
		for (; dhp->h_addr_list[j] != NULL; j++) {
			bcopy(dhp->h_addr_list[j], cptr->ipsc_dst_addr,
			    IPV4_ADDR_LEN);
			ret = str_ioctl(fd, SIOCSIPSECONFIG, cptr, 1);
			if (ret == -1) {
				fprintf(stderr, gettext("Could not add "
				    "between %d.%d.%d.%d and %d.%d.%d.%d\n"),
				    cptr->ipsc_src_addr[0],
				    cptr->ipsc_src_addr[1],
				    cptr->ipsc_src_addr[2],
				    cptr->ipsc_src_addr[3],
				    cptr->ipsc_dst_addr[0],
				    cptr->ipsc_dst_addr[1],
				    cptr->ipsc_dst_addr[2],
				    cptr->ipsc_dst_addr[3]);
			}
		}
	}
}

static int
parse_mask(int type, char *mask_str, ipsec_conf_t *cptr)
{
	in_addr_t mask;

	if ((strncasecmp(mask_str, "0x", 2) == 0) &&
	    (strchr(mask_str, '.') == NULL)) {
		/* Is it in the form 0xff000000 ? */
		char *end;

		mask = strtoul(mask_str, &end, 0);
		if (end == mask_str) {
			return (-1);
		}
		mask = htonl(mask);
		/* XXX Should we check for non-contiguous masks ? */
		if (mask == 0)
			return (-1);
		if (type == IPSEC_CONF_SRC_MASK) {
			bcopy((uint8_t *)&mask, cptr->ipsc_src_mask,
			    IPV4_ADDR_LEN);
		} else {
			bcopy((uint8_t *)&mask, cptr->ipsc_dst_mask,
			    IPV4_ADDR_LEN);
		}
		return (0);
	} else {
		if (strcmp(mask_str, "255.255.255.255") == 0) {
			mask = 0xffffffff;
		} else {
			mask = inet_addr(mask_str);
			if (mask == (unsigned int)-1)
				return (-1);
		}
		/* XXX Should we check for non-contiguous masks ? */
		if (mask == 0)
			return (-1);
		/* mask is in network order already */
		if (type == IPSEC_CONF_SRC_MASK) {
			bcopy((uint8_t *)&mask, cptr->ipsc_src_mask,
			    IPV4_ADDR_LEN);
		} else {
			bcopy((uint8_t *)&mask, cptr->ipsc_dst_mask,
			    IPV4_ADDR_LEN);
		}
		return (0);
	}
}

static int
parse_port(int type, char *port_str, ipsec_conf_t *conf)
{
	struct servent *sent;
	in_port_t port;
	int ret;

	sent = getservbyname(port_str, NULL);
	if (sent == NULL) {
		ret = parse_int(port_str);
		if (ret < 0 || ret >= 65536) {
			return (-1);
		}
		port = htons((in_port_t)ret);
	} else {
		port = sent->s_port;
	}
	if (type == IPSEC_CONF_SRC_PORT) {
		conf->ipsc_src_port = port;
	} else {
		conf->ipsc_dst_port = port;
	}
	return (0);
}

static int
valid_algorithm(char *str, char *prot)
{
	str_val_t *valgs;
	char *tmp;
	int ret;
	int i;

	if (strcmp(prot, "AH") == 0) {
		valgs = ah_algs;
	} else if (strcmp(prot, "ESP") == 0) {
		valgs = esp_algs;
	} else {
		return (-1);
	}

	for (i = 0; valgs[i].string != NULL; i++) {
		if (strcasecmp(valgs[i].string, str) == 0) {
			return (valgs[i].value);
		}
	}
	/*
	 * Look whether it could be a valid number.
	 * We support numbers also so that users can
	 * load algorithms as they need it. We can't
	 * check for validity of numbers here. It will
	 * be checked when the SA is negotiated/looked up.
	 * parse_int uses strtol(str), which converts 3DES
	 * to a valid number i.e looks only at initial
	 * number part. If we come here we should expect
	 * only a decimal number.
	 */
	tmp = str;
	while (*tmp) {
		if (!isdigit(*tmp))
			return (-1);
		tmp++;
	}

	ret = parse_int(str);
	if (ret > 0 && ret <= 255)
		return (ret);
	else
		return (-1);
}

static int
parse_ipsec_alg(char *str, ipsec_conf_t *conf, int alg_type)
{
	int alg_value;
	char tstr[VALID_ALG_LEN];

	/*
	 * Make sure that we get a null terminated string.
	 * For a bad input, we truncate at VALID_ALG_LEN.
	 */
	strncpy(tstr, str, VALID_ALG_LEN - 1);
	tstr[VALID_ALG_LEN - 1] = '\0';
	if (alg_type == IPSEC_CONF_IPSEC_AALGS ||
	    alg_type == IPSEC_CONF_IPSEC_EAALGS) {
		alg_value = valid_algorithm(tstr, "AH");
	} else {
		alg_value = valid_algorithm(tstr, "ESP");
	}
	if (alg_value == -1) {
		return (-1);
	}
	if (alg_type == IPSEC_CONF_IPSEC_AALGS) {
		conf->ipsc_no_of_ah_algs++;
		conf->ipsc_ah_algs[0] = alg_value;
	} else if (alg_type == IPSEC_CONF_IPSEC_EALGS) {
		conf->ipsc_no_of_esp_algs++;
		conf->ipsc_esp_algs[0] = alg_value;
	} else {
		conf->ipsc_no_of_esp_auth_algs++;
		conf->ipsc_esp_auth_algs[0] = alg_value;
	}
	return (0);
}

static void
error_message(error_type_t error, int type, int line)
{
	char *mesg;

	switch (type) {
	case IPSEC_CONF_SRC_ADDRESS :
		mesg = gettext("Source Address");
		break;
	case IPSEC_CONF_DST_ADDRESS:
		mesg = gettext("Destination Address");
		break;
	case IPSEC_CONF_SRC_PORT:
		mesg = gettext("Source Port");
		break;
	case IPSEC_CONF_DST_PORT:
		mesg = gettext("Destination Port");
		break;
	case IPSEC_CONF_SRC_MASK:
		mesg = gettext("Source Mask");
		break;
	case IPSEC_CONF_DST_MASK:
		mesg = gettext("Destination Mask");
		break;
	case IPSEC_CONF_ULP:
		mesg = gettext("Upper Layer Protocol");
		break;
	case IPSEC_CONF_IPSEC_AALGS:
		mesg = gettext("Authentication Algorithm");
		break;
	case IPSEC_CONF_IPSEC_EALGS:
		mesg = gettext("Encryption Algorithm");
		break;
	case IPSEC_CONF_IPSEC_EAALGS:
		mesg = gettext("ESP Authentication Algorithm");
		break;
	case IPSEC_CONF_IPSEC_SA:
		mesg = gettext("SA");
		break;
	case IPSEC_CONF_IPSEC_DIR:
		mesg = gettext("Direction");
		break;
	default :
		return;
	}
	if (error == BAD_ERROR) {
		fprintf(stderr, "Bad %s", mesg);
	} else {
		fprintf(stderr, "Duplicate %s", mesg);
	}
	/*
	 * If we never read a newline character, we don't want
	 * to print 0.
	 */
	fprintf(stderr, gettext(" on line: %d\n"), (arg_indices[line] == 0) ?
	    1 : arg_indices[line]);
}

static int
validate_properties(ipsec_conf_t *cptr, boolean_t dir, boolean_t is_alg)
{
	if (cptr->ipsc_policy == IPSEC_POLICY_BYPASS) {
		if (!dir) {
			fprintf(stderr, gettext("dir string "
			    "not found for bypass policy\n"));
		}
		if (cptr->ipsc_sa_attr != 0) {
			fprintf(stderr, gettext("SA attributes "
			    "found for bypass policy\n"));
		}
		if (is_alg) {
			fprintf(stderr, gettext("Algorithms "
			    "found for bypass policy\n"));
			return (-1);
		}
		return (0);
	}
	if (!is_alg) {
		fprintf(stderr, gettext("No IPSEC algorithms given\n"));
		return (-1);
	}
	if (cptr->ipsc_sa_attr == 0) {
		fprintf(stderr, gettext("No SA attribute\n"));
		return (-1);
	}
	return (0);
}

/*
 * This function is called only to parse a single action string.
 * This is called after parsing pattern and before parsing properties.
 * Thus we may have something in the leftover buffer while parsing
 * the pattern, which we need to handle here.
 */
static int
parse_action(FILE *fp, char **action, char **leftover)
{
	char *cp;
	char ibuf[MAXLEN];
	char *tmp_buf;
	char *buf;
	boolean_t new_stuff;

	if (*leftover != NULL) {
		buf = *leftover;
		new_stuff = B_FALSE;
		goto scan;
	}
	while (fgets(ibuf, MAXLEN, fp) != NULL) {
		new_stuff = B_TRUE;
		if (ibuf[strlen(ibuf) - 1] == '\n')
			linecount++;
		buf = ibuf;
scan:
		/* Truncate at the beginning of a comment */
		cp = strchr(buf, '#');
		if (cp != NULL)
			*cp = NULL;

		/* Skip any whitespace */
		while (*buf != NULL && isspace(*buf))
			buf++;

		/* Empty line */
		if (*buf == NULL)
			continue;

		/*
		 * Store the command for error reporting
		 * and ipsec_conf_add().
		 */
		if (new_stuff) {
			/*
			 * Check for buffer overflow including the null
			 * terminating character.
			 */
			int len = strlen(ibuf);
			if ((cbuf_offset + len + 1) >= CBUF_LEN)
				return (-1);
			strcpy(cbuf + cbuf_offset, ibuf);
			cbuf_offset += len;
		}
		/*
		 * Start of the non-empty non-space character.
		 */
		tmp_buf = buf++;

		/* Skip until next whitespace or CURL_BEGIN */
		while (*buf != NULL && !isspace(*buf) &&
		    *buf != CURL_BEGIN)
			buf++;


		if (*buf != NULL) {
			if (*buf == CURL_BEGIN) {
				*buf = NULL;
				/* Allocate an extra byte for the null also */
				if ((*action = malloc(strlen(tmp_buf) + 1)) ==
				    NULL) {
					perror("malloc");
					return (ENOMEM);
				}
				strcpy(*action, tmp_buf);
				*buf = CURL_BEGIN;
			} else {
				/* We have hit a space */
				*buf++ = NULL;
				/* Allocate an extra byte for the null also */
				if ((*action = malloc(strlen(tmp_buf) + 1)) ==
				    NULL) {
					perror("malloc");
					return (ENOMEM);
				}
				strcpy(*action, tmp_buf);
			}
			/*
			 * Copy the rest of the line into the
			 * leftover buffer.
			 */
			if (*buf != NULL) {
				strcpy(lo_buf, buf);
				*leftover = lo_buf;
			} else {
				*leftover = NULL;
			}
		} else {
			/* Allocate an extra byte for the null also */
			if ((*action = malloc(strlen(tmp_buf) + 1)) ==
			    NULL) {
				perror("malloc");
				return (ENOMEM);
			}
			strcpy(*action, tmp_buf);
			*leftover = NULL;
		}
		if (argindex >= ARG_BUF_LEN)
			return (-1);
		arg_indices[argindex++] = linecount;
		return (PARSE_SUCCESS);
	}
	/*
	 * Return error, on an empty action field.
	 */
	return (-1);
}

/*
 * This is called to parse pattern or properties that is enclosed
 * between CURL_BEGIN and CURL_END.
 */
static int
parse_pattern_or_prop(FILE *fp, char *argvec[], char **leftover)
{
	char *cp;
	int i = 0;
	boolean_t curl_begin_seen = B_FALSE;
	char ibuf[MAXLEN];
	char *tmp_buf;
	char *buf;
	boolean_t new_stuff;

	/*
	 * When parsing properties, leftover buffer could have the
	 * leftovers of the previous fgets().
	 */
	if (*leftover != NULL) {
		buf = *leftover;
		new_stuff = B_FALSE;
		goto scan;
	}
	while (fgets(ibuf, MAXLEN, fp) != NULL) {
		new_stuff = B_TRUE;
		if (ibuf[strlen(ibuf) - 1] == '\n')
			linecount++;
		buf = ibuf;
scan:
		/* Truncate at the beginning of a comment */
		cp = strchr(buf, '#');
		if (cp != NULL)
			*cp = NULL;

		/* Skip any whitespace */
		while (*buf != NULL && isspace(*buf))
			buf++;

		/* Empty line */
		if (*buf == NULL)
			continue;
		/*
		 * Store the command for error reporting
		 * and ipsec_conf_add().
		 */
		if (new_stuff) {
			/*
			 * Check for buffer overflow including the null
			 * terminating character.
			 */
			int len = strlen(ibuf);
			if ((cbuf_offset + len + 1) >= CBUF_LEN)
				return (-1);
			strcpy(cbuf + cbuf_offset, ibuf);
			cbuf_offset += len;
		}
		/*
		 * First non-space character should be
		 * a curly bracket.
		 */
		if (!curl_begin_seen) {
			if (*buf != CURL_BEGIN) {
				/*
				 * If we never read a newline character,
				 * we don't want to print 0.
				 */
				fprintf(stderr, gettext("Bad start"
				    " on line %d :\n"),
				(linecount == 0) ? 1 : linecount);
				return (-1);
			}
			buf++;
			curl_begin_seen = B_TRUE;
		}
		/*
		 * Arguments are separated by white spaces or
		 * newlines. Scan till you see a CURL_END.
		 */
		while (*buf != NULL) {
			if (*buf == CURL_END) {
ret:
				*buf++ = NULL;
				/*
				 * Copy the rest of the line into the
				 * leftover buffer if any.
				 */
				if (*buf != NULL) {
					strcpy(lo_buf, buf);
					*leftover = lo_buf;
				} else {
					*leftover = NULL;
				}
				return (PARSE_SUCCESS);
			}
			/*
			 * Skip any trailing whitespace until we see a
			 * non white-space character.
			 */
			while (*buf != NULL && isspace(*buf))
				buf++;

			if (*buf == CURL_END)
				goto ret;

			/* Scan the next line as this buffer is empty */
			if (*buf == NULL)
				break;

			if (i >= MAXARGS) {
				fprintf(stderr,
				    gettext("Number of Arguments exceeded "
				    "%d\n"), i);
				return (-1);
			}
			/*
			 * Non-empty, Non-space buffer.
			 */
			tmp_buf = buf++;
			/*
			 * Real scan of the argument takes place here.
			 * Skip past till space or CURL_END.
			 */
			while (*buf != NULL && !isspace(*buf) &&
			    *buf != CURL_END) {
				buf++;
			}
			/*
			 * Either a space or we have hit the CURL_END or
			 * the real end.
			 */
			if (*buf != NULL) {
				if (*buf == CURL_END) {
					*buf++ = NULL;
					if ((argvec[i] = malloc(strlen(tmp_buf)
					    + 1)) == NULL) {
						perror("malloc");
						return (ENOMEM);
					}
					/*
					 * Copy the rest of the line into the
					 * leftover buffer.
					 */
					if (*buf != NULL) {
						strcpy(lo_buf, buf);
						*leftover = lo_buf;
					} else {
						*leftover = NULL;
					}
					if (strlen(tmp_buf) != 0) {
						strcpy(argvec[i], tmp_buf);
						if (argindex >= ARG_BUF_LEN)
							return (-1);
						arg_indices[argindex++] =
						    linecount;
					}
					return (PARSE_SUCCESS);
				} else {
					*buf++ = NULL;
				}
			}
			/*
			 * Copy this argument and scan for the buffer more
			 * if it is non-empty. If it is empty scan for
			 * the next line.
			 */
			if ((argvec[i] = malloc(strlen(tmp_buf) + 1)) ==
			    NULL) {
				perror("malloc");
				return (ENOMEM);
			}
			strcpy(argvec[i++], tmp_buf);
			if (argindex >= ARG_BUF_LEN)
				return (-1);
			arg_indices[argindex++] = linecount;
		}
	}
	/*
	 * If nothing is given in the file, it is okay.
	 * If something is given in the file and it is
	 * not CURL_BEGIN, we would have returned error
	 * above. If curl_begin_seen and we are here,
	 * something is wrong.
	 */
	if (curl_begin_seen)
		return (-1);
	return (PARSE_EOF);		/* Nothing more in the file */
}

/*
 * Parse one command i.e {pattern} action {properties}.
 */
static int
parse_one(FILE *fp, char *pattern[], char **action, char *properties[])
{
	char *leftover;
	int ret;
	int i;

	memset(pattern, 0, ((MAXARGS + 1) * sizeof (char *)));
	memset(properties, 0, ((MAXARGS + 1) * sizeof (char *)));

	ret = 0;
	*action = NULL;
	leftover = NULL;
	argindex = 0;
	cbuf_offset = 0;
	src_multi_home = B_FALSE;
	dst_multi_home = B_FALSE;

	ret = parse_pattern_or_prop(fp, pattern, &leftover);
	if (ret == PARSE_EOF) {
		/* EOF reached */
		return (0);
	}
	if (ret != 0) {
		goto err;
	}
	ret = parse_action(fp, action, &leftover);
	if (ret != 0) {
		goto err;
	}
	/*
	 * Validate action now itself so that we don't
	 * proceed too much into the bad world.
	 */
	for (i = 0; action_table[i].string; i++) {
		if (strcmp(*action, action_table[i].string) == 0)
			break;
	}
	if (action_table[i].string == NULL) {
		/*
		 * If we never read a newline character, we don't want
		 * to print 0.
		 */
		fprintf(stderr, gettext("Invalid action on line "
		    "%d: %s\n"), (linecount == 0) ? 1 : linecount, action);
		return (-1);
	}

	ret = parse_pattern_or_prop(fp, properties, &leftover);
	if (ret != 0) {
		goto err;
	}
	if (leftover != NULL) {
		/* Accomodate spaces at the end */
		while (*leftover != NULL) {
			if (!isspace(*leftover)) {
				ret = -1;
				goto err;
			}
			leftover++;
		}
	}
	return (ret);
err:
	if (ret == -1) {
		/*
		 * If we never read a newline character, we don't want
		 * to print 0.
		 */
		fprintf(stderr, gettext("Error before or at line %d\n"),
		    (linecount == 0) ? 1 : linecount);
	}
	return (ret);
}

static int
form_ipsec_conf(char *pattern[], char *action, char *properties[],
    ipsec_conf_t *cptr)
{
	int i, j;
	struct protoent *pent;
	boolean_t saddr, daddr, ipsec_aalg, ipsec_ealg, ipsec_eaalg, dir;
	in_addr_t mask;
	int line_no;
	int ret;

#ifdef DEBUG
	for (i = 0; pattern[i] != NULL; i++)
		printf("%s\n", pattern[i]);
	if (action != NULL)
		printf("%s\n", action);
	for (i = 0; properties[i] != NULL; i++)
		printf("%s\n", properties[i]);
#endif

	memset(cptr, 0, sizeof (ipsec_conf_t));
	saddr = daddr = ipsec_aalg = ipsec_ealg = ipsec_eaalg = dir = B_FALSE;
	/*
	 * Get the Pattern. NULL pattern is valid.
	 */
	for (i = 0, line_no = 0; pattern[i]; i++, line_no++) {
		for (j = 0; pattern_table[j].string; j++) {
			if (strcmp(pattern[i], pattern_table[j].string) == 0)
				break;
		}
		if (pattern_table[j].string == NULL) {
			/*
			 * If we never read a newline character, we don't want
			 * to print 0.
			 */
			fprintf(stderr, gettext("Invalid pattern on line "
			    "%d: %s\n"), (arg_indices[line_no] == 0) ? 1 :
			    arg_indices[line_no], pattern[i]);
			return (-1);
		}
		switch (pattern_table[j].value) {
		case IPSEC_CONF_SRC_ADDRESS :
			if (saddr) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_SRC_ADDRESS, line_no);
				return (-1);
			}
			/*
			 * Use this to detect duplicates rather
			 * than 0 like other cases, because 0 for
			 * address means INADDR_ANY.
			 */
			saddr = B_TRUE;
			/*
			 * Advance to the string containing
			 * the address.
			 */
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			if (parse_address(IPSEC_CONF_SRC_ADDRESS,
			    pattern[i], cptr) != 0) {
				error_message(BAD_ERROR,
				    IPSEC_CONF_SRC_ADDRESS, line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_DST_ADDRESS :
			if (daddr) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_DST_ADDRESS, line_no);
				return (-1);
			}
			/*
			 * Use this to detect duplicates rather
			 * than 0 like other cases, because 0 for
			 * address means INADDR_ANY.
			 */
			daddr = B_TRUE;
			/*
			 * Advance to the string containing
			 * the address.
			 */
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			if (parse_address(IPSEC_CONF_DST_ADDRESS,
			    pattern[i], cptr) != 0) {
				error_message(BAD_ERROR,
				    IPSEC_CONF_DST_ADDRESS, line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_SRC_PORT :
			if (cptr->ipsc_src_port != 0) {
				error_message(DUP_ERROR, IPSEC_CONF_SRC_PORT,
				    line_no);
				return (-1);
			}
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			ret = parse_port(IPSEC_CONF_SRC_PORT, pattern[i],
			    cptr);
			if (ret != 0) {
				error_message(BAD_ERROR, IPSEC_CONF_SRC_PORT,
				    line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_DST_PORT :
			if (cptr->ipsc_dst_port != 0) {
				error_message(DUP_ERROR, IPSEC_CONF_DST_PORT,
				    line_no);
				return (-1);
			}
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			ret = parse_port(IPSEC_CONF_DST_PORT, pattern[i],
			    cptr);
			if (ret != 0) {
				error_message(BAD_ERROR, IPSEC_CONF_DST_PORT,
				    line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_SRC_MASK :
			bcopy(cptr->ipsc_src_mask, (uint8_t *)&mask,
			    IPV4_ADDR_LEN);
			if (mask != 0) {
				error_message(DUP_ERROR, IPSEC_CONF_SRC_MASK,
				    line_no);
				return (-1);
			}
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			ret = parse_mask(IPSEC_CONF_SRC_MASK, pattern[i],
			    cptr);
			if (ret != 0) {
				error_message(BAD_ERROR, IPSEC_CONF_SRC_MASK,
				    line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_DST_MASK :
			bcopy(cptr->ipsc_dst_mask, (uint8_t *)&mask,
			    IPV4_ADDR_LEN);
			if (mask != 0) {
				error_message(DUP_ERROR, IPSEC_CONF_DST_MASK,
				    line_no);
				return (-1);
			}
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			ret = parse_mask(IPSEC_CONF_DST_MASK, pattern[i],
			    cptr);
			if (ret != 0) {
				error_message(BAD_ERROR, IPSEC_CONF_DST_MASK,
				    line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_ULP :
			if (cptr->ipsc_ulp_prot != 0) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_ULP, line_no);
				return (-1);
			}
			i++, line_no++;
			if (pattern[i] == NULL)
				return (-1);
			pent = getprotobyname(pattern[i]);
			if (pent == NULL) {
				int ulp;
				ulp = parse_int(pattern[i]);
				if (ulp == -1) {
					error_message(BAD_ERROR,
					    IPSEC_CONF_ULP, line_no);
					return (-1);
				}
				cptr->ipsc_ulp_prot = ulp;
			} else {
				cptr->ipsc_ulp_prot = pent->p_proto;
			}
			break;
		}
	}
	/*
	 * See whether we have a valid mask in the case of
	 * valid address.
	 */
	if (saddr) {
		in_addr_t addr;

		bcopy(cptr->ipsc_src_mask, (uint8_t *)&mask, IPV4_ADDR_LEN);
		if (mask == 0) {
			/*
			 * Neither the prefix len nor the smask was given.
			 * Set the mask by looking at zero bytes in the
			 * address.
			 */

			bcopy(cptr->ipsc_src_addr, (uint8_t *)&addr,
			    IPV4_ADDR_LEN);
			mask = htonl(in_makemask(ntohl(addr)));
			bcopy((uint8_t *)&mask, cptr->ipsc_src_mask,
			    IPV4_ADDR_LEN);
		}
	}
	if (daddr) {
		in_addr_t addr;

		bcopy(cptr->ipsc_dst_mask, (uint8_t *)&mask, IPV4_ADDR_LEN);
		if (mask == 0) {
			/*
			 * Neither the prefix len nor the dmask was given.
			 * Set the mask by looking at zero bytes in the
			 * address.
			 */

			bcopy(cptr->ipsc_dst_addr, (uint8_t *)&addr,
			    IPV4_ADDR_LEN);
			mask = htonl(in_makemask(ntohl(addr)));
			bcopy((uint8_t *)&mask, cptr->ipsc_dst_mask,
			    IPV4_ADDR_LEN);
		}
	}

	/*
	 * Get the action.
	 */
	for (j = 0; action_table[j].string; j++) {
		if (strcmp(action, action_table[j].string) == 0)
			break;
	}
	/*
	 * The following thing should never happen as
	 * we have already tested for its validity in parse.
	 */
	if (action_table[j].string == NULL) {
		fprintf(stderr, gettext("Invalid action on line "
		    "%d: %s\n"), (arg_indices[line_no] == 0) ? 1 :
		    arg_indices[line_no], action);
		return (-1);
	}
	cptr->ipsc_policy = action_table[j].value;

	if (cptr->ipsc_policy == IPSEC_POLICY_APPLY) {
		cptr->ipsc_dir = IPSEC_TYPE_OUTBOUND;
	} else if (cptr->ipsc_policy == IPSEC_POLICY_DISCARD) {
		cptr->ipsc_dir = IPSEC_TYPE_INBOUND;
	}

	line_no++;
	/*
	 * Get the properties. NULL properties is not valid.
	 * Later checks will catch it.
	 */
	for (i = 0; properties[i]; i++, line_no++) {
		for (j = 0; property_table[j].string; j++) {
			if (strcmp(properties[i],
			    property_table[j].string) == 0) {
				break;
			}
		}
		if (property_table[j].string == NULL) {
			fprintf(stderr, gettext("Invalid properties on "
			    "line %d: %s\n"), (arg_indices[line_no] == 0) ?
			    1 : arg_indices[line_no], properties[i]);
			return (-1);
		}
		switch (property_table[j].value) {
		case IPSEC_CONF_IPSEC_AALGS:
			if (ipsec_aalg) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_IPSEC_AALGS, line_no);
				return (-1);
			}
			i++, line_no++;
			if (properties[i] == NULL)
				return (-1);
			ret = parse_ipsec_alg(properties[i], cptr,
			    IPSEC_CONF_IPSEC_AALGS);
			if (ret != 0) {
				error_message(BAD_ERROR,
				    IPSEC_CONF_IPSEC_AALGS, line_no);
				return (-1);
			}
			ipsec_aalg = B_TRUE;
			break;
		case IPSEC_CONF_IPSEC_EALGS:
			/*
			 * If this option was not given and encr_auth_algs
			 * was given, we provide null-encryption. We do the
			 * setting after we parse all the options.
			 */
			if (ipsec_ealg) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_IPSEC_EALGS, line_no);
				return (-1);
			}
			i++, line_no++;
			if (properties[i] == NULL)
				return (-1);
			ret = parse_ipsec_alg(properties[i], cptr,
			    IPSEC_CONF_IPSEC_EALGS);
			if (ret != 0) {
				error_message(BAD_ERROR,
				    IPSEC_CONF_IPSEC_EALGS, line_no);
				return (-1);
			}
			ipsec_ealg = B_TRUE;
			break;
		case IPSEC_CONF_IPSEC_EAALGS:
			/*
			 * If this option was not given and encr_algs
			 * option was given, we still pass a default
			 * value in ipsc_esp_auth_algs. This is to
			 * encourage the use of authentication with
			 * ESP.
			 */
			if (ipsec_eaalg) {
				error_message(DUP_ERROR,
				    IPSEC_CONF_IPSEC_EAALGS, line_no);
				return (-1);
			}
			i++, line_no++;
			if (properties[i] == NULL)
				return (-1);
			ret = parse_ipsec_alg(properties[i], cptr,
			    IPSEC_CONF_IPSEC_EAALGS);
			if (ret != 0) {
				error_message(BAD_ERROR,
				    IPSEC_CONF_IPSEC_EAALGS, line_no);
				return (-1);
			}
			ipsec_eaalg = B_TRUE;
			break;
		case IPSEC_CONF_IPSEC_SA:
			if (cptr->ipsc_sa_attr != 0) {
				error_message(DUP_ERROR, IPSEC_CONF_IPSEC_SA,
				    line_no);
				return (-1);
			}
			i++, line_no++;
			if (properties[i] == NULL)
				return (-1);
			if (strcmp(properties[i], "shared") == 0) {
				cptr->ipsc_sa_attr = IPSEC_SHARED_SA;
			} else if (strcmp(properties[i], "unique") == 0) {
				cptr->ipsc_sa_attr = IPSEC_UNIQUE_SA;
			} else {
				error_message(BAD_ERROR,
				    IPSEC_CONF_IPSEC_SA, line_no);
				return (-1);
			}
			break;
		case IPSEC_CONF_IPSEC_DIR:
			if (dir) {
				error_message(DUP_ERROR, IPSEC_CONF_IPSEC_DIR,
				    line_no);
				return (-1);
			}
			dir = B_TRUE;
			i++, line_no++;
			if (properties[i] == NULL)
				return (-1);
			if (strcmp(properties[i], "out") == 0) {
				cptr->ipsc_dir = IPSEC_TYPE_OUTBOUND;
			} else if (strcmp(properties[i], "in") == 0) {
				cptr->ipsc_dir = IPSEC_TYPE_INBOUND;
			} else {
				error_message(BAD_ERROR, IPSEC_CONF_IPSEC_DIR,
				    line_no);
				return (-1);
			}
			/* Check for conflict with action. */
			if (cptr->ipsc_dir == IPSEC_TYPE_OUTBOUND &&
			    cptr->ipsc_policy == IPSEC_POLICY_DISCARD) {
				fprintf(stderr, gettext("Direction in "
				    "conflict with action\n"));
				return (-1);
			}
			if (cptr->ipsc_dir == IPSEC_TYPE_INBOUND &&
			    cptr->ipsc_policy == IPSEC_POLICY_APPLY) {
				fprintf(stderr, gettext("Direction in "
				    "conflict with action\n"));
				return (-1);
			}
			break;
		}
	}

	/*
	 * Default SA attribute for inbound is shared as
	 * we can't verify the uniqueness on inbound.
	 */
	if (cptr->ipsc_policy == IPSEC_POLICY_DISCARD &&
	    cptr->ipsc_sa_attr == 0) {
		cptr->ipsc_sa_attr = IPSEC_SHARED_SA;
	}

	if (!ipsec_ealg && ipsec_eaalg) {
		/*
		 * If the user has specified the auth alg to be used
		 * with encryption and did not provide a encryption
		 * algorithm, provide null encryption.
		 */
		cptr->ipsc_no_of_esp_algs++;
		cptr->ipsc_esp_algs[0] = SADB_EALG_NULL;
		ipsec_ealg = B_TRUE;
	}

	/* Set the level of IPSEC protection we want */
	if (ipsec_aalg && (ipsec_ealg || ipsec_eaalg)) {
		cptr->ipsc_ipsec_prot = IPSEC_AH_ESP;
	} else if (ipsec_aalg) {
		cptr->ipsc_ipsec_prot = IPSEC_AH_ONLY;
	} else if (ipsec_ealg || ipsec_eaalg) {
		cptr->ipsc_ipsec_prot = IPSEC_ESP_ONLY;
	}

	/* Validate the properties */
	return (validate_properties(cptr, dir,
	    (ipsec_aalg || ipsec_ealg || ipsec_eaalg)));
}

static int
print_cmd_buf(FILE *fp)
{
	*(cbuf + cbuf_offset) = '\0';

	if (fp == stderr) {
		if (errno == EEXIST) {
			fprintf(fp, gettext("Command:\n%s"), cbuf);
		} else {
			fprintf(fp, gettext("Malformed command:\n%s"), cbuf);
		}
	} else {
		if (fprintf(fp, "%s", cbuf) == -1) {
			perror("fprintf");
			return (-1);
		}
	}
	return (0);
}

#ifdef	DEBUG
static void
dump_conf(ipsec_conf_t *conf)
{
	printf("Source Addr is %d.%d.%d.%d\n", conf->ipsc_src_addr[0],
	    conf->ipsc_src_addr[1], conf->ipsc_src_addr[2],
	    conf->ipsc_src_addr[3]);
	printf("Dest Addr is %d.%d.%d.%d\n", conf->ipsc_dst_addr[0],
	    conf->ipsc_dst_addr[1], conf->ipsc_dst_addr[2],
	    conf->ipsc_dst_addr[3]);
	printf("Source Mask is %d.%d.%d.%d\n", conf->ipsc_src_mask[0],
	    conf->ipsc_src_mask[1], conf->ipsc_src_mask[2],
	    conf->ipsc_src_mask[3]);
	printf("Dest Mask is %d.%d.%d.%d\n", conf->ipsc_dst_mask[0],
	    conf->ipsc_dst_mask[1], conf->ipsc_dst_mask[2],
	    conf->ipsc_dst_mask[3]);
	printf("Source port %d\n", conf->ipsc_src_port);
	printf("Dest port %d\n", conf->ipsc_dst_port);
	printf("IPSEc prot is %d\n", conf->ipsc_ipsec_prot);
	printf("AH algs is %d\n", conf->ipsc_no_of_ah_algs);
	printf("ESP algs is %d\n", conf->ipsc_no_of_esp_algs);
	printf("ESP Auth algs is %d\n", conf->ipsc_no_of_esp_auth_algs);
	printf("Sa attr is %d\n", conf->ipsc_sa_attr);
	printf("------------------------------------\n");
}
#endif	/* DEBUG */

static int
ipsec_conf_add(int fd)
{
	char *pattern[MAXARGS + 1];
	char *action;
	char *properties[MAXARGS + 1];
	ipsec_conf_t conf;
	FILE *fp, *policy_fp;
	int ret, i;
	char *warning = gettext(
		"\tWARNING : New policy entries that are being added may\n "
		"\taffect the existing connections. Existing connections\n"
		"\tthat are not subjected to policy constraints, may be\n"
		"\tsubjected to policy constraints because of the new\n"
		"\tpolicy. This can disrupt the communication of the\n"
		"\texisting connections.\n");


	fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("fopen");
		fprintf(stderr, gettext("%s : Input file cannot be "
		    "opened\n"), filename);
		usage();
		return (-1);
	}
	/*
	 * This will create the file if it does not exist.
	 * Make sure the umask is right.
	 */
	(void) umask(0022);
	policy_fp = fopen(POLICY_CONF_FILE, "a");
	if (policy_fp == NULL) {
		perror("fopen");
		fprintf(stderr, gettext("%s cannot be opened\n"),
		    POLICY_CONF_FILE);
		return (-1);
	}

	if (!qflag) {
		printf("%s", warning);
	}

	/*
	 * Pattern, action, and properties are allocated in
	 * parse_pattern_or_prop and in parse_action (called by
	 * parse_one) as we parse arguments.
	 */
	while ((ret = parse_one(fp, pattern, &action, properties)) == 0) {
		/*
		 * If there is no action and parse returned success,
		 * it means that there is nothing to add.
		 */
		if (action == NULL)
			break;

		ret = form_ipsec_conf(pattern, action, properties, &conf);
		if (ret != 0) {
			break;
		}
#ifdef DEBUG
		dump_conf(&conf);
		ret = 0;
#endif
		ret = str_ioctl(fd, SIOCSIPSECONFIG, &conf, 1);
		if (ret == -1) {
			if (errno == EEXIST) {
				(void) print_cmd_buf(stderr);
				goto next;
			} else {
				break;
			}
		}

		/*
		 * Set in form_ipsec_conf while parsing a multi-homed
		 * address.
		 */
		if (src_multi_home || dst_multi_home) {
			do_multi_home(fd, &conf);
		}

		/*
		 * The # should help re-using the ipsecpolicy.conf
		 * for input again as # will be treated as comment.
		 */
		if (fprintf(policy_fp, "#INDEX %d \n",
		    conf.ipsc_policy_index) == -1) {
			perror("fprintf");
			fprintf(stderr, gettext("Addition incomplete, Please "
			    "flush all the entries and re-configure :\n"));
			reconfigure();
			ret = -1;
			break;
		}
		if (print_cmd_buf(policy_fp) == -1) {
			fprintf(stderr, gettext("Addition incomplete. Please "
			    "flush all the entries and re-configure :\n"));
			reconfigure();
			ret = -1;
			break;
		}
		/*
		 * We add one newline by default to separate out the
		 * entries. If the last character is not a newline, we
		 * insert a newline for free. This makes sure that all
		 * entries look consistent in the file.
		 */
		if (*(cbuf + cbuf_offset - 1) == '\n') {
			if (fprintf(policy_fp, "\n") == -1) {
				perror("fprintf");
				fprintf(stderr, gettext("Addition incomplete. "
				    "Please flush all the entries and "
				    "re-configure :\n"));
				reconfigure();
				ret = -1;
				break;
			}
		} else {
			if (fprintf(policy_fp, "\n\n") == -1) {
				perror("fprintf");
				fprintf(stderr, gettext("Addition incomplete. "
				    "Please flush all the entries and "
				    "re-configure :\n"));
				reconfigure();
				ret = -1;
				break;
			}
		}
		/*
		 * Make sure this gets to the disk before
		 * we parse the next entry.
		 */
		fflush(policy_fp);
next:
		for (i = 0; pattern[i] != NULL; i++)
			free(pattern[i]);
		free(action);
		for (i = 0; properties[i] != NULL; i++)
			free(properties[i]);
	}
	if (ret == -1) {
		(void) print_cmd_buf(stderr);
		for (i = 0; pattern[i] != NULL; i++)
			free(pattern[i]);
		if (action != NULL)
			free(action);
		for (i = 0; properties[i] != NULL; i++)
			free(properties[i]);
	}
	return (ret);
}
