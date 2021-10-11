/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 * NOTE:I'm trying to use "struct sadb_foo" instead of "sadb_foo_t"
 *	as a maximal PF_KEY portability test.
 *
 *	Also, this is a deliberately single-threaded app, also for portability
 *	to systems without POSIX threads.
 */

#pragma ident	"@(#)ipseckey.c	1.5	99/09/07 SMI"


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/fcntl.h>
#include <net/pfkeyv2.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <pwd.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <fcntl.h>
#include <strings.h>
#include <ctype.h>

#include <setjmp.h>

/* Globals... */

#define	NBUF_SIZE	16
#define	IBUF_SIZE	512
#define	COMMENT_CHAR	'#'
#define	CONT_CHAR	'\\'

char numprint[NBUF_SIZE];
int keysock;
uint32_t seq;
pid_t mypid;
boolean_t nflag = B_FALSE;	/* Avoid nameservice? */
boolean_t vflag = B_FALSE;	/* Verbose? */
boolean_t pflag = B_FALSE;	/* Paranoid w.r.t. printing keying material? */
boolean_t interactive = B_FALSE;
boolean_t readfile = B_FALSE;
uint_t lineno = 0;

#define	MAX_GET_SIZE	1024
/* Defined as a uint64_t array for alignment purposes. */
uint64_t get_buffer[MAX_GET_SIZE];

/* For error recovery in interactive or read-file mode. */
jmp_buf env;
int val;

/*
 * When a syscall or library call fails, print errno, and exit if
 * command-line, or reset state if reading from a file.
 */

/* Localization macro... */
#define	Bail(s)	bail(gettext(s))

void
bail(char *what)
{
	perror(what);
	if (readfile) {
		fprintf(stderr, gettext("System error on line %u.\n"), lineno);
	}
	if (interactive && !readfile)
		longjmp(env, 2);
	exit(1);
}

/*
 * When something syntactically bad happens while reading commands,
 * print it.  For command line, exit.  For reading from a file, exit, and
 * print the offending line number.  For interactive, just print the error
 * and reset the program state with the longjmp().
 */
void
usage(void)
{
	if (readfile) {
		fprintf(stderr, gettext("Parse error on line %u.\n"), lineno);
	}
	if (!interactive) {
		fprintf(stderr, gettext("Usage:\t"
		    "ipseckey [ -nvp ] | cmd [sa_type] [extfield value]*\n"));
		fprintf(stderr, gettext("\tipseckey [ -nvp ] -f infile\n"));
		fprintf(stderr, gettext("\tipseckey [ -nvp ] -s outfile\n"));
		exit(1);
	} else {
		longjmp(env, 1);
	}
}

/*
 * dump_XXX functions produce ASCII output from various structures.
 *
 * Because certain errors need to do this to stderr, dump_XXX functions take
 * a FILE pointer.
 */

/*
 * Dump a sockaddr.  This is separate from dump_addr, because more than
 * one diagnostic (dump/get, and name resolver weirdnesses) requires the
 * whole sockaddr.
 */
void
dump_sockaddr(struct sockaddr *sa, FILE *where)
{
	struct sockaddr_in *sin;
	/* XXX IPv6 :  Don't forget IPv6. */
	struct hostent *hp;
	char *printable_addr;

	switch (sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		/* I assume this is in network byte order. */
		printable_addr = inet_ntoa(sin->sin_addr);
		if (printable_addr == NULL)
			printable_addr = "<inet_ntoa() failed>";
		fprintf(where, gettext("AF_INET:  port = %d, %s"),
		    ntohs(sin->sin_port), printable_addr);
		if (!nflag) {
			/* Do reverse name lookup. */
			hp = gethostbyaddr((char *)(&sin->sin_addr),
			    sizeof (struct in_addr), AF_INET);
			if (hp != NULL) {
				fprintf(where, " (%s)", hp->h_name);
			} else if (sin->sin_addr.s_addr == INADDR_ANY) {
				fprintf(where, gettext(" <unspecified>"));
			} else {
				fprintf(where, gettext(" <lookup failed>"));
			}
		}
		printf(".\n");
		break;
		/* XXX IPv6 */
	default:
		break;
	}
}

/*
 * Dump a key and bitlen to the form
 */
void
dump_key(uint8_t *keyp, uint_t bitlen, FILE *where)
{
	int numbytes;

	numbytes = SADB_1TO8(bitlen);
	/* The & 0x7 is to check for leftover bits. */
	if ((bitlen & 0x7) != 0)
		numbytes++;
	while (numbytes-- != 0)
		if (pflag)
			fprintf(where, "XX");	/* Print no keys if paranoid. */
		else
			fprintf(where, "%02x", *keyp++);
	fprintf(where, "/%u", bitlen);
}

/* Print an authentication algorithm */

void
dump_aalg(uint8_t aalg, FILE *where)
{
	switch (aalg) {
	case SADB_AALG_MD5HMAC:
		fputs("HMAC-MD5", where);
		break;
	case SADB_AALG_SHA1HMAC:
		fputs("HMAC-SHA-1", where);
		break;
	default:
		fprintf(where, gettext("<unknown %u>"), aalg);
		break;
	}
}

/*
 * Print an encryption algorithm
 */
void
dump_ealg(uint8_t ealg, FILE *where)
{
	switch (ealg) {
	case SADB_EALG_DESCBC:
		fputs("DES-CBC", where);
		break;
	case SADB_EALG_3DESCBC:
		fputs("3DES-CBC", where);
		break;
	case SADB_EALG_NULL:
		fputs(gettext("NULL"), where);
		break;
	default:
		fprintf(where, gettext("<unknown %u>"), ealg);
		break;
	}
}

/*
 * Initialize a PF_KEY base message.
 */
void
msg_init(struct sadb_msg *msg, uint8_t type, uint8_t satype)
{
	msg->sadb_msg_version = PF_KEY_V2;
	msg->sadb_msg_type = type;
	msg->sadb_msg_errno = 0;
	msg->sadb_msg_satype = satype;
	/* For starters... */
	msg->sadb_msg_len = SADB_8TO64(sizeof (*msg));
	msg->sadb_msg_reserved = 0;
	msg->sadb_msg_seq = ++seq;
	msg->sadb_msg_pid = mypid;
}

/*
 * parseXXX and rparseXXX commands parse input and convert them to PF_KEY
 * field values, or do the reverse for the purposes of saving the SA tables.
 * (See the save_XXX functions.)
 */

#define	CMD_NONE	0
#define	CMD_UPDATE	2
#define	CMD_ADD		3
#define	CMD_DELETE	4
#define	CMD_GET		5
#define	CMD_FLUSH	9
#define	CMD_DUMP	10
#define	CMD_MONITOR	11
#define	CMD_PMONITOR	12
#define	CMD_QUIT	13
#define	CMD_SAVE	14
#define	CMD_HELP	15

/*
 * Parse the command.
 */
int
parsecmd(char *cmdstr)
{
	static struct cmdtable {
		char *cmd;
		int token;
	} table[] = {
		/*
		 * Q: Do we want to do GETSPI?
		 * A: No, it's for automated key mgmt. only.  Either that,
		 *    or it isn't relevant until we support non IPsec SA types.
		 */
		{"update",		CMD_UPDATE},
		{"add",			CMD_ADD},
		{"delete", 		CMD_DELETE},
		{"get", 		CMD_GET},
		/*
		 * Q: And ACQUIRE and REGISTER and EXPIRE?
		 * A: not until we support non IPsec SA types.
		 */
		{"flush",		CMD_FLUSH},
		{"dump",		CMD_DUMP},
		{"monitor",		CMD_MONITOR},
		{"passive_monitor",	CMD_PMONITOR},
		{"pmonitor",		CMD_PMONITOR},
		{"quit",		CMD_QUIT},
		{"exit",		CMD_QUIT},
		{"save",		CMD_SAVE},
		{"help",		CMD_HELP},
		{"?",			CMD_HELP},
		{NULL,			CMD_NONE}
	};
	struct cmdtable *ct = table;

	while (ct->cmd != NULL && strcmp(ct->cmd, cmdstr) != 0)
		ct++;
	return (ct->token);
}

/*
 * Convert a number from a command line.  I picked "u_longlong_t" for the
 * number because we need the largest number available.  Also, the strto<num>
 * calls don't deal in units of uintNN_t.
 */
u_longlong_t
parsenum(char *num, boolean_t bail)
{
	u_longlong_t rc;
	char *end = NULL;

	if (num == NULL) {
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	errno = 0;
	rc = strtoull(num, &end, 0);
	if (end == num)
		if (bail) {
			fprintf(stderr,
			    gettext("Expecting a number, but got %s.\n"),
			    strerror(errno));
			usage();
		} else {
			/* XXX What's a "good" value to return here? */
			return ((u_longlong_t)-1);
		}

	return (rc);
}

/*
 * Parse and reverse parse a specific SA type (AH, ESP, etc.).
 */
static struct typetable {
	char *type;
	int token;
} type_table[] = {
	{"all",	SADB_SATYPE_UNSPEC},
	{"ah",	SADB_SATYPE_AH},
	{"esp",	SADB_SATYPE_ESP},
	/* XXX PF_KEY :  I'll put more in here later. */
	{NULL,	0}	/* Token value is irrelevant for this entry. */
};

char *
rparsesatype(int type)
{
	struct typetable *tt = type_table;

	while (tt->type != NULL && type != tt->token)
		tt++;

	if (tt->type == NULL) {
		snprintf(numprint, NBUF_SIZE, "%d", type);
	} else {
		return (tt->type);
	}

	return (numprint);
}

int
parsesatype(char *type)
{
	struct typetable *tt = type_table;

	if (type == NULL)
		return (SADB_SATYPE_UNSPEC);

	while (tt->type != NULL && strcasecmp(tt->type, type) != 0)
		tt++;

	/*
	 * New SA types (including ones keysock maintains for user-land
	 * protocols) may be added, so parse a numeric value if possible.
	 */
	if (tt->type == NULL) {
		tt->token = (int)parsenum(type, B_FALSE);
		if (tt->token == -1) {
			fprintf(stderr, gettext("Unknown SA type (%s).\n"),
			    type);
			usage();
		}
	}

	return (tt->token);
}

#define	NEXTEOF		0
#define	NEXTNONE	1
#define	NEXTNUM		2
#define	NEXTSTR		3
#define	NEXTNUMSTR	4
#define	NEXTADDR	5
#define	NEXTHEX		6
#define	NEXTIDENT	7

#define	TOK_EOF			0
#define	TOK_UNKNOWN		1
#define	TOK_SPI			2
#define	TOK_REPLAY		3
#define	TOK_STATE		4
#define	TOK_AUTHALG		5
#define	TOK_ENCRALG		6
#define	TOK_FLAGS		7
#define	TOK_SOFT_ALLOC		8
#define	TOK_SOFT_BYTES		9
#define	TOK_SOFT_ADDTIME	10
#define	TOK_SOFT_USETIME	11
#define	TOK_HARD_ALLOC		12
#define	TOK_HARD_BYTES		13
#define	TOK_HARD_ADDTIME	14
#define	TOK_HARD_USETIME	15
#define	TOK_CURRENT_ALLOC	16
#define	TOK_CURRENT_BYTES	17
#define	TOK_CURRENT_ADDTIME	18
#define	TOK_CURRENT_USETIME	19
#define	TOK_SRCADDR		20
#define	TOK_DSTADDR		21
#define	TOK_PROXYADDR		22
#define	TOK_AUTHKEY		23
#define	TOK_ENCRKEY		24
#define	TOK_SRCIDTYPE		25
#define	TOK_DSTIDTYPE		26
#define	TOK_DPD			27
#define	TOK_SENS_LEVEL		28
#define	TOK_SENS_MAP		29
#define	TOK_INTEG_LEVEL		30
#define	TOK_INTEG_MAP		31

/*
 * Q:	Do I need stuff for proposals, combinations, supported algorithms,
 *	or SPI ranges?
 *
 * A:	Probably not, but you never know.
 *
 * Parse out extension header type values.
 */
int
parseextval(char *value, int *next)
{
	static struct toktable {
		char *string;
		int token;
		int next;
	} tokens[] = {
		/* "String",		token value,		next arg is */
		{"spi",			TOK_SPI,		NEXTNUM},
		{"replay",		TOK_REPLAY,		NEXTNUM},
		{"state",		TOK_STATE,		NEXTNUMSTR},
		{"auth_alg",		TOK_AUTHALG,		NEXTNUMSTR},
		{"authalg",		TOK_AUTHALG,		NEXTNUMSTR},
		{"encr_alg",		TOK_ENCRALG,		NEXTNUMSTR},
		{"encralg",		TOK_ENCRALG,		NEXTNUMSTR},
		{"flags",		TOK_FLAGS,		NEXTNUM},
		{"soft_alloc",		TOK_SOFT_ALLOC,		NEXTNUM},
		{"soft_bytes",		TOK_SOFT_BYTES,		NEXTNUM},
		{"soft_addtime",	TOK_SOFT_ADDTIME,	NEXTNUM},
		{"soft_usetime",	TOK_SOFT_USETIME,	NEXTNUM},
		{"hard_alloc",		TOK_HARD_ALLOC,		NEXTNUM},
		{"hard_bytes",		TOK_HARD_BYTES,		NEXTNUM},
		{"hard_addtime",	TOK_HARD_ADDTIME,	NEXTNUM},
		{"hard_usetime",	TOK_HARD_USETIME,	NEXTNUM},
		{"current_alloc",	TOK_CURRENT_ALLOC,	NEXTNUM},
		{"current_bytes",	TOK_CURRENT_BYTES,	NEXTNUM},
		{"current_addtime",	TOK_CURRENT_ADDTIME,	NEXTNUM},
		{"current_usetime",	TOK_CURRENT_USETIME,	NEXTNUM},
		{"srcaddr",		TOK_SRCADDR,		NEXTADDR},
		{"src",			TOK_SRCADDR,		NEXTADDR},
		{"dstaddr",		TOK_DSTADDR,		NEXTADDR},
		{"dst",			TOK_DSTADDR,		NEXTADDR},
		{"proxyaddr",		TOK_PROXYADDR,		NEXTADDR},
		{"proxy",		TOK_PROXYADDR,		NEXTADDR},
		{"authkey",		TOK_AUTHKEY,		NEXTHEX},
		{"encrkey",		TOK_ENCRKEY,		NEXTHEX},
		{"srcidtype",		TOK_SRCIDTYPE,		NEXTIDENT},
		{"dstidtype",		TOK_DSTIDTYPE,		NEXTIDENT},
		{"dpd",			TOK_DPD,		NEXTNUM},
		{"sens_level",		TOK_SENS_LEVEL,		NEXTNUM},
		{"sens_map",		TOK_SENS_MAP,		NEXTHEX},
		{"integ_level",		TOK_INTEG_LEVEL,	NEXTNUM},
		{"integ_map",		TOK_INTEG_MAP,		NEXTHEX},
		{NULL,			TOK_UNKNOWN,		NEXTEOF}
	};
	struct toktable *tp;

	if (value == NULL)
		return (TOK_EOF);

	for (tp = tokens; tp->string != NULL; tp++)
		if (strcmp(value, tp->string) == 0)
			break;

	/*
	 * Since the OS controls what extensions are available, we don't have
	 * to parse numeric values here.
	 */

	*next = tp->next;
	return (tp->token);
}

/*
 * Parse possible state values.
 */
uint8_t
parsestate(char *state)
{
	struct states {
		char *state;
		uint8_t retval;
	} states[] = {
		{"larval",	SADB_SASTATE_LARVAL},
		{"mature",	SADB_SASTATE_MATURE},
		{"dying",	SADB_SASTATE_DYING},
		{"dead",	SADB_SASTATE_DEAD},
		{NULL,		0}
	};
	struct states *sp;

	if (state == NULL) {
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	for (sp = states; sp->state != NULL; sp++) {
		if (strcmp(sp->state, state) == 0)
			return (sp->retval);
	}
	fprintf(stderr, gettext("Unknown state type %s.\n"), state);
	usage();
	/* NOTREACHED */
}

/*
 * Parse and reverse parse possible encrypt algorithm values, include numbers.
 */
struct alg {
	char *alg_name;
	uint8_t alg_val;
};

struct alg encr_algs[] = {
	{"des-cbc",	SADB_EALG_DESCBC},
	{"des",		SADB_EALG_DESCBC},
	{"3des-cbc",	SADB_EALG_3DESCBC},
	{"3des",	SADB_EALG_3DESCBC},
	{"null",	SADB_EALG_NULL},
	{NULL,		0}
};

struct alg auth_algs[] = {
	{"hmac-md5",	SADB_AALG_MD5HMAC},
	{"md5",		SADB_AALG_MD5HMAC},
	{"hmac-sha1",	SADB_AALG_SHA1HMAC},
	{"hmac-sha",	SADB_AALG_SHA1HMAC},
	{"sha1",	SADB_AALG_SHA1HMAC},
	{"sha",		SADB_AALG_SHA1HMAC},
	{NULL,		0}
};

char *
rparsealg(uint8_t alg, struct alg *table)
{
	struct alg *ep;

	for (ep = table; ep->alg_name != NULL; ep++) {
		if (alg == ep->alg_val)
			return (ep->alg_name);
	}

	snprintf(numprint, NBUF_SIZE, "%d", alg);
	return (numprint);
}

uint8_t
parsealg(char *alg, struct alg *table)
{
	struct alg *ep;
	u_longlong_t invalue;

	if (alg == NULL) {
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	for (ep = table; ep->alg_name != NULL; ep++) {
		if (strcasecmp(ep->alg_name, alg) == 0)
			return (ep->alg_val);
	}

	/*
	 * Since algorithms can be loaded during kernel run-time, check for
	 * numeric algorithm values too.  PF_KEY can catch bad ones with EINVAL.
	 */
	invalue = parsenum(alg, B_FALSE);
	if (invalue != (u_longlong_t)-1 &&
	    (u_longlong_t)(invalue & (u_longlong_t)0xff) == invalue)
		return ((uint8_t)invalue);

	fprintf(stderr, gettext("Unknown encryption algorithm type %s.\n"),
	    alg);
	usage();
	/* NOTREACHED */
}

/*
 * Parse and reverse parse out a source/destination ID type.
 */
struct idtypes {
	char *idtype;
	uint8_t retval;
} idtypes[] = {
	{"prefix",	SADB_IDENTTYPE_PREFIX},
	{"fqdn",	SADB_IDENTTYPE_FQDN},
	{"domain",	SADB_IDENTTYPE_FQDN},
	{"domainname",	SADB_IDENTTYPE_FQDN},
	{"user_fqdn",	SADB_IDENTTYPE_USER_FQDN},
	{"mailbox",	SADB_IDENTTYPE_USER_FQDN},
	{NULL,		0}
};

char *
rparseidtype(uint16_t type)
{
	struct idtypes *idp;

	for (idp = idtypes; idp->idtype != NULL; idp++) {
		if (type == idp->retval)
			return (idp->idtype);
	}

	snprintf(numprint, NBUF_SIZE, "%d", type);
	return (numprint);
}

uint16_t
parseidtype(char *type)
{
	struct idtypes *idp;
	u_longlong_t invalue;

	if (type == NULL) {
		/* XXX shouldn't reach here, see callers for why. */
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	for (idp = idtypes; idp->idtype != NULL; idp++) {
		if (strcasecmp(idp->idtype, type) == 0)
			return (idp->retval);
	}
	/*
	 * Since identity types are almost arbitrary, check for numeric
	 * algorithm values too.  PF_KEY can catch bad ones with EINVAL.
	 */
	invalue = parsenum(type, B_FALSE);
	if (invalue != (u_longlong_t)-1 &&
	    (u_longlong_t)(invalue & (u_longlong_t)0xffff) == invalue)
		return ((uint16_t)invalue);


	fprintf(stderr, gettext("Unknown identity type %s.\n"), type);
	usage();
	/* NOTREACHED */
}

/*
 * Parse an address off the command line.  Return length of sockaddr,
 * and write address information into *sa.
 */
int
parseaddr(char *addr, struct sockaddr *sa)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;
	/* XXX IPv6 :  IPv6 and sockaddr_in6 coming soon! */
	struct hostent *hp;
	int rc;

	if (addr == NULL) {
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	/* XXX IPv6 initalization too?... */
	sin->sin_family = AF_INET;
	sin->sin_port = 0;
	bzero(sin->sin_zero, sizeof (sin->sin_zero));

	if (!nflag) {
		/*
		 * Try name->address first.
		 * XXX IPv6 :  We need IPv6 stuff in here too!
		 */
		hp = gethostbyname(addr);
		if (hp != NULL) {
			switch (hp->h_addrtype) {
			case AF_INET:
				rc = sizeof (struct sockaddr_in);
				memcpy(&sin->sin_addr, hp->h_addr_list[0],
				    hp->h_length);
				break;
			default:
				fprintf(stderr,
				    gettext("Address type %d not supported.\n"),
				    hp->h_addrtype);
				usage();
				break;
			}
			if (hp->h_addr_list[1] != NULL) {
				fprintf(stderr, gettext(
				    "WARNING: Many addresses for %s.  "), addr);
				fprintf(stderr,
				    gettext("  Using the following:\n"), addr);
				dump_sockaddr(sa, stderr);
			}
			return (rc);
		}
	}

	/* Try a normal address conversion. */
	sin->sin_addr.s_addr = inet_addr(addr);
	if (sin->sin_addr.s_addr == (in_addr_t)-1) {
		fprintf(stderr, gettext("Unknown address %s.\n"), addr);
		usage();
	}

	/* XXX IPv6. */
	return (sizeof (struct sockaddr_in));
}

/*
 * Parse a hex character for a key.  A string will take the form:
 *	xxxxxxxxx/nn
 * where
 *	xxxxxxxxx == a string of hex characters ([0-9][a-f][A-F])
 *	nn == an optional decimal "mask".  If it is not present, it
 *	is assumed that the hex string will be rounded to the nearest
 *	byte, where odd nibbles, like 123 will become 0x0123.
 *
 * NOTE:Unlike the expression of IP addresses, I will not allow an
 *	excessive "mask".  For example 2112/50 is very illegal.
 * NOTE2:	This key should be in canonical order.  Consult your man
 *		pages per algorithm about said order.
 */

#define	hd2num(hd) (((hd) >= '0' && (hd) <= '9') ? ((hd) - '0') : \
	(((hd) >= 'a' && (hd) <= 'f') ? ((hd) - 'a' + 10) : ((hd) - 'A' + 10)))

struct sadb_key *
parsekey(char *input)
{
	struct sadb_key *retval;
	uint_t i, hexlen = 0, bits, alloclen;
	uint8_t *key;

	if (input == NULL) {
		fprintf(stderr, gettext("Unexpected end of command line.\n"));
		usage();
	}

	for (i = 0; input[i] != '\0' && input[i] != '/'; i++)
		hexlen++;

	if (input[i] == '\0') {
		bits = 0;
	} else {
		/* Have /nn. */
		input[i] = '\0';
		if (sscanf((input + i + 1), "%u", &bits) != 1) {
			fprintf(stderr, gettext("%s is not a bit specifier.\n"),
			    (input + i + 1));
			usage();
		}
		/* hexlen in nibbles */
		if (((bits + 3) >> 2) > hexlen) {
			fprintf(stderr,
			    gettext("bit length %d is too big for %s.\n"),
			    bits, input);
			usage();
		}
		/*
		 * Adjust hexlen down if user gave us too small of a bit
		 * count.
		 */
		if ((hexlen << 2) > bits + 3) {
			fprintf(stderr, gettext("WARNING: "
			    "Lower bits will be truncated for:\n\t%s/%d.\n"),
			    input, bits);
			hexlen = (bits + 3) >> 2;
			input[hexlen] = '\0';
		}
	}

	/*
	 * Allocate.  Remember, hexlen is in nibbles.
	 */

	alloclen = sizeof (*retval) + roundup((hexlen/2 + (hexlen & 0x1)), 8);
	retval = malloc(alloclen);

	if (retval == NULL)
		Bail("malloc(parsekey)");
	retval->sadb_key_len = SADB_8TO64(alloclen);
	retval->sadb_key_reserved = 0;
	if (bits == 0)
		retval->sadb_key_bits = (hexlen + (hexlen & 0x1)) << 2;
	else
		retval->sadb_key_bits = bits;

	/*
	 * Read in nibbles.  Read in odd-numbered as shifted high.
	 * (e.g. 123 becomes 0x1230).
	 */

	key = (uint8_t *)(retval + 1);
	for (i = 0; input[i] != '\0'; i += 2) {
		boolean_t second = (input[i + 1] != '\0');

		if (!isxdigit(input[i]) ||
		    (!isxdigit(input[i + 1]) && second)) {
			fprintf(stderr,
			    gettext("string '%s' not a hex string.\n"), input);
			usage();
		}
		*key = (hd2num(input[i]) << 4);
		if (second)
			*key |= hd2num(input[i + 1]);
		else
			break;	/* out of for loop. */
		key++;
	}

	/* bzero the remaining bits if we're a non-octet amount. */
	if (bits & 0x7)
		*((input[i] == '\0') ? key - 1 : key) &=
		    0xff << (8 - (bits & 0x7));

	return (retval);
}

/*
 * Prints the base PF_KEY message.
 */
void
print_sadb_msg(struct sadb_msg *samsg)
{
	printf(gettext("Base message (version %u) type "),
	    samsg->sadb_msg_version);
	switch (samsg->sadb_msg_type) {
	case SADB_RESERVED:
		printf(gettext("RESERVED (warning: set to 0)"));
		break;
	case SADB_GETSPI:
		printf("GETSPI");
		break;
	case SADB_UPDATE:
		printf("UPDATE");
		break;
	case SADB_ADD:
		printf("ADD");
		break;
	case SADB_DELETE:
		printf("DELETE");
		break;
	case SADB_GET:
		printf("GET");
		break;
	case SADB_ACQUIRE:
		printf("ACQUIRE");
		break;
	case SADB_REGISTER:
		printf("REGISTER");
		break;
	case SADB_EXPIRE:
		printf("EXPIRE");
		break;
	case SADB_FLUSH:
		printf("FLUSH");
		break;
	case SADB_DUMP:
		printf("DUMP");
		break;
	case SADB_X_PROMISC:
		printf("X_PROMISC");
		break;
	default:
		printf(gettext("Unknown (%u)"), samsg->sadb_msg_type);
		break;
	}
	printf(gettext(", SA type "));

	switch (samsg->sadb_msg_satype) {
	case SADB_SATYPE_UNSPEC:
		printf(gettext("<unspecified/all>"));
		break;
	case SADB_SATYPE_AH:
		printf("AH");
		break;
	case SADB_SATYPE_ESP:
		printf("ESP");
		break;
	case SADB_SATYPE_RSVP:
		printf("RSVP");
		break;
	case SADB_SATYPE_OSPFV2:
		printf("OSPFv2");
		break;
	case SADB_SATYPE_RIPV2:
		printf("RIPv2");
		break;
	case SADB_SATYPE_MIP:
		printf(gettext("Mobile IP"));
		break;
	default:
		printf(gettext("<unknown %u>"), samsg->sadb_msg_satype);
		break;
	}

	printf(".\n");

	if (samsg->sadb_msg_errno != 0) {
		printf(gettext("Error %s from PF_KEY.\n"),
		    strerror(samsg->sadb_msg_errno));
	}

	printf(gettext("Message length %u bytes, seq=%u, pid=%u.\n"),
	    SADB_64TO8(samsg->sadb_msg_len), samsg->sadb_msg_seq,
	    samsg->sadb_msg_pid);
}

/*
 * Print the SA extension for PF_KEY.
 */
void
print_sa(char *prefix, struct sadb_sa *assoc)
{
	if (assoc->sadb_sa_len != SADB_8TO64(sizeof (*assoc))) {
		fprintf(stderr,
		    gettext("WARNING: SA info extension length (%u) is bad.\n"),
		    SADB_64TO8(assoc->sadb_sa_len));
	}

	printf(gettext("%sSADB_ASSOC spi=0x%x, replay=%u, state="), prefix,
	    ntohl(assoc->sadb_sa_spi), assoc->sadb_sa_replay);
	switch (assoc->sadb_sa_state) {
	case SADB_SASTATE_LARVAL:
		printf(gettext("LARVAL"));
		break;
	case SADB_SASTATE_MATURE:
		printf(gettext("MATURE"));
		break;
	case SADB_SASTATE_DYING:
		printf(gettext("DYING"));
		break;
	case SADB_SASTATE_DEAD:
		printf(gettext("DEAD"));
		break;
	default:
		printf(gettext("<unknown %u>"), assoc->sadb_sa_state);
	}

	if (assoc->sadb_sa_auth != SADB_AALG_NONE) {
		printf(gettext("\n%sAuthentication algorithm = "), prefix);
		dump_aalg(assoc->sadb_sa_auth, stdout);
	}

	if (assoc->sadb_sa_encrypt != SADB_EALG_NONE) {
		printf(gettext("\n%sEncryption algorithm = "), prefix);
		dump_ealg(assoc->sadb_sa_encrypt, stdout);
	}

	printf(gettext("\n%sflags=0x%x < "), prefix, assoc->sadb_sa_flags);
	if (assoc->sadb_sa_flags & SADB_SAFLAGS_PFS)
		printf("PFS ");
	if (assoc->sadb_sa_flags & SADB_SAFLAGS_NOREPLAY)
		printf("NOREPLAY ");

	/* BEGIN Solaris-specific flags. */
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_USED)
		printf("X_USED ");
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_UNIQUE)
		printf("X_UNIQUE ");
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_AALG1)
		printf("X_AALG1 ");
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_AALG2)
		printf("X_AALG2 ");
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_EALG1)
		printf("X_EALG1 ");
	if (assoc->sadb_sa_flags & SADB_X_SAFLAGS_EALG2)
		printf("X_EALG2 ");
	/* END Solaris-specific flags. */

	printf(">\n");
}

/*
 * This is pretty arbitrary.  Perhaps it shouldn't be.  I need to print
 * the time in a locale-independent way.
 */
#define	TBUF_SIZE 50

/* There is no system-defined TIME_MAX.  I'll make one here. */
#define	TIME_MAX LONG_MAX

/*
 * Print the SA lifetime information.  (An SADB_EXT_LIFETIME_* extension.)
 */
void
print_lifetimes(struct sadb_lifetime *current, struct sadb_lifetime *hard,
    struct sadb_lifetime *soft)
{
	time_t wallclock, scratch;	/* For localtime() call. */
	char tbuf[TBUF_SIZE]; /* For strftime() call (or its failure). */
	char *soft_prefix = gettext("SLT: ");
	char *hard_prefix = gettext("HLT: ");
	char *current_prefix = gettext("CLT: ");

	if (current != NULL &&
	    current->sadb_lifetime_len != SADB_8TO64(sizeof (*current))) {
		fprintf(stderr, gettext("WARNING: CURRENT lifetime extension "
		    "length (%u) is bad.\n"),
		    SADB_64TO8(current->sadb_lifetime_len));
	}

	if (hard != NULL &&
	    hard->sadb_lifetime_len != SADB_8TO64(sizeof (*hard))) {
		fprintf(stderr, gettext("WARNING: HARD lifetime extension "
		    "length (%u) is bad.\n"),
		    SADB_64TO8(hard->sadb_lifetime_len));
	}

	if (soft != NULL &&
	    soft->sadb_lifetime_len != SADB_8TO64(sizeof (*soft))) {
		fprintf(stderr, gettext("WARNING: SOFT lifetime extension "
		    "length (%u) is bad.\n"),
		    SADB_64TO8(soft->sadb_lifetime_len));
	}

	printf(" LT: Lifetime information\n");

	time(&wallclock);

	if (current != NULL) {
		/* Express values as current values. */
		printf(gettext(
		    "%s%llu bytes protected, %u allocations used.\n"),
		    current_prefix, current->sadb_lifetime_bytes,
		    current->sadb_lifetime_allocations);
		scratch = (time_t)current->sadb_lifetime_addtime;
		if (strftime(tbuf, TBUF_SIZE, NULL, localtime(&scratch)) == 0)
			strcpy(tbuf, gettext("<time conversion failed>"));
		/* Assume time_t is ulong_t, and print %lu */
		printf(gettext("%sSA added at time %s\n"), current_prefix,
		    tbuf);
		if (vflag)
			printf(gettext("%s\t(raw time value %lu)\n"),
			    current_prefix, scratch);
		if (current->sadb_lifetime_usetime != 0) {
			scratch = (time_t)current->sadb_lifetime_usetime;
			if (strftime(tbuf, TBUF_SIZE, NULL,
			    localtime(&scratch)) == 0) {
				strcpy(tbuf,
				    gettext("<time conversion failed>"));
			}
			printf(gettext("%sSA first used at time %s\n"),
			    current_prefix, tbuf);
			if (vflag)
				printf(gettext("%s\t(raw time value %lu)\n"),
				    current_prefix, scratch);
		}
		scratch = wallclock;
		if (strftime(tbuf, TBUF_SIZE, NULL, localtime(&scratch)) == 0)
			strcpy(tbuf, gettext("<time conversion failed>"));
		printf(gettext("%sTime now is %s\n"), current_prefix, tbuf);
		if (vflag)
			printf(gettext("%s\t(raw time value %lu)\n"),
			    current_prefix, scratch);
	}

	if (soft != NULL) {
		printf(gettext("%sSoft lifetime information:  "), soft_prefix);
		printf(gettext("%llu bytes of lifetime, %u allocations.\n"),
		    soft->sadb_lifetime_bytes, soft->sadb_lifetime_allocations);
		printf(gettext("%s%llu seconds of post-add lifetime.\n"),
		    soft_prefix, soft->sadb_lifetime_addtime);
		printf(gettext("%s%llu seconds of post-use lifetime.\n"),
		    soft_prefix, soft->sadb_lifetime_usetime);
		/* If possible, express values as time remaining. */
		if (current != NULL) {
			if (soft->sadb_lifetime_bytes != 0)
				printf(gettext(
				    "%s%llu more bytes can be protected.\n"),
				    soft_prefix,
				    (soft->sadb_lifetime_bytes >
					current->sadb_lifetime_bytes) ?
				    (soft->sadb_lifetime_bytes -
					current->sadb_lifetime_bytes) : (0));
			if (soft->sadb_lifetime_addtime != 0 ||
			    (soft->sadb_lifetime_usetime != 0 &&
				current->sadb_lifetime_usetime != 0)) {
				time_t adddelta, usedelta;

				if (soft->sadb_lifetime_addtime != 0) {
					adddelta =
					    current->sadb_lifetime_addtime +
					    soft->sadb_lifetime_addtime -
					    wallclock;
				} else {
					adddelta = TIME_MAX;
				}

				if (soft->sadb_lifetime_usetime != 0 &&
				    current->sadb_lifetime_usetime != 0) {
					usedelta =
					    current->sadb_lifetime_usetime +
					    soft->sadb_lifetime_usetime -
					    wallclock;
				} else {
					usedelta = TIME_MAX;
				}
				printf("%s", soft_prefix);
				scratch = (adddelta < usedelta) ? adddelta :
				    usedelta;
				if (scratch >= 0) {
					printf(gettext("Soft expiration "
					    "occurs in %ld seconds, "),
					    scratch);
				} else {
					printf(gettext(
					    "Soft expiration occurred "));
				}
				scratch += wallclock;
				if (strftime(tbuf, TBUF_SIZE, NULL,
				    localtime(&scratch)) == 0) {
					strcpy(tbuf, gettext(
					    "<time conversion failed>"));
				}
				printf(gettext("at %s.\n"), tbuf);
				if (vflag)
					printf(gettext("%s\t"
					    "(raw time value %lu)\n"),
					    soft_prefix, scratch);
			}
		}
	}

	if (hard != NULL) {
		printf(gettext("%sHard lifetime information:  "), hard_prefix);
		printf(gettext("%llu bytes of lifetime, %u allocations.\n"),
		    hard->sadb_lifetime_bytes, hard->sadb_lifetime_allocations);
		printf(gettext("%s%llu seconds of post-add lifetime.\n"),
		    hard_prefix, hard->sadb_lifetime_addtime);
		printf(gettext("%s%llu seconds of post-use lifetime.\n"),
		    hard_prefix, hard->sadb_lifetime_usetime);
		/* If possible, express values as time remaining. */
		if (current != NULL) {
			if (hard->sadb_lifetime_bytes != 0)
				printf(gettext(
				    "%s%llu more bytes can be protected.\n"),
				    hard_prefix,
				    (hard->sadb_lifetime_bytes >
					current->sadb_lifetime_bytes) ?
				    (hard->sadb_lifetime_bytes -
					current->sadb_lifetime_bytes) : (0));
			if (hard->sadb_lifetime_addtime != 0 ||
			    (hard->sadb_lifetime_usetime != 0 &&
				current->sadb_lifetime_usetime != 0)) {
				time_t adddelta, usedelta;

				if (hard->sadb_lifetime_addtime != 0) {
					adddelta =
					    current->sadb_lifetime_addtime +
					    hard->sadb_lifetime_addtime -
					    wallclock;
				} else {
					adddelta = TIME_MAX;
				}

				if (hard->sadb_lifetime_usetime != 0 &&
				    current->sadb_lifetime_usetime != 0) {
					usedelta =
					    current->sadb_lifetime_usetime +
					    hard->sadb_lifetime_usetime -
					    wallclock;
				} else {
					usedelta = TIME_MAX;
				}
				printf("%s", hard_prefix);
				scratch = (adddelta < usedelta) ? adddelta :
				    usedelta;
				if (scratch >= 0) {
					printf(gettext("Hard expiration "
					    "occurs in %ld seconds, "),
					    scratch);
				} else {
					printf(gettext(
					    "Hard expiration occured "));
				}
				scratch += wallclock;
				if (strftime(tbuf, TBUF_SIZE, NULL,
				    localtime(&scratch)) == 0) {
					strcpy(tbuf, gettext(
					    "<time conversion failed>"));
				}
				printf(gettext("at %s.\n"), tbuf);
				if (vflag)
					printf(gettext("%s\t"
					    "(raw time value %lu)\n"),
					    hard_prefix, scratch);
			}
		}
	}
}

/*
 * Print an SADB_EXT_ADDRESS_* extension.
 */
void
print_address(char *prefix, struct sadb_address *addr)
{
	struct protoent *pe;

	printf(prefix);
	switch (addr->sadb_address_exttype) {
	case SADB_EXT_ADDRESS_SRC:
		printf(gettext("Source address "));
		break;
	case SADB_EXT_ADDRESS_DST:
		printf(gettext("Destination address "));
		break;
	case SADB_EXT_ADDRESS_PROXY:
		printf(gettext("Proxy address "));
		break;
	}

	printf(gettext("(proto=%d/"), addr->sadb_address_proto);
	if (addr->sadb_address_proto == 0) {
		printf(gettext("<unspecified>)"));
	} else if ((pe = getprotobynumber(addr->sadb_address_proto)) != NULL) {
		printf("%s)", pe->p_name);
	} else {
		printf(gettext("<unknown>)"));
	}
	printf(gettext("\n%s"), prefix);
	dump_sockaddr((struct sockaddr *)(addr + 1), stdout);
}

/*
 * Print an SADB_EXT_KEY extension.
 */
void
print_key(char *prefix, struct sadb_key *key)
{
	printf(prefix);

	switch (key->sadb_key_exttype) {
	case SADB_EXT_KEY_AUTH:
		printf(gettext("Authentication"));
		break;
	case SADB_EXT_KEY_ENCRYPT:
		printf(gettext("Encryption"));
		break;
	}

	printf(gettext(" key.\n%s"), prefix);
	dump_key((uint8_t *)(key + 1), key->sadb_key_bits, stdout);
	putchar('\n');
}

/*
 * Print an SADB_EXT_IDENTITY_* extension.
 */
void
print_ident(char *prefix, struct sadb_ident *id)
{
	printf(prefix);
	switch (id->sadb_ident_exttype) {
	case SADB_EXT_IDENTITY_SRC:
		printf(gettext("Source"));
		break;
	case SADB_EXT_IDENTITY_DST:
		printf(gettext("Destination"));
		break;
	}

	printf(gettext(" identity, uid=%d, type "), id->sadb_ident_id);
	switch (id->sadb_ident_type) {
	case SADB_IDENTTYPE_PREFIX:
		printf(gettext("prefix"));
		break;
	case SADB_IDENTTYPE_FQDN:
		printf(gettext("FQDN"));
		break;
	case SADB_IDENTTYPE_USER_FQDN:
		printf(gettext("user-FQDN (mbox)"));
		break;
	default:
		printf(gettext("<unknown %u>"), id->sadb_ident_type);
		break;
	}
	printf("\n%s", prefix);
	printf("%s\n", (char *)(id + 1));
}

/*
 * Print an SADB_SENSITIVITY extension.
 */
void
print_sens(char *prefix, struct sadb_sens *sens)
{
	uint64_t *bitmap = (uint64_t *)(sens + 1);
	int i;

	printf(gettext("%sSensitivity DPD %d, sens level=%d, integ level=%d\n"),
	    prefix, sens->sadb_sens_dpd, sens->sadb_sens_sens_level,
	    sens->sadb_sens_integ_level);
	for (i = 0; sens->sadb_sens_sens_len-- > 0; i++, bitmap++)
		printf(gettext("%s Sensitivity BM extended word %d 0x%llx\n"),
		    i, *bitmap);
	for (i = 0; sens->sadb_sens_integ_len-- > 0; i++, bitmap++)
		printf(gettext("%s Integrity BM extended word %d 0x%llx\n"),
		    i, *bitmap);
}

/*
 * Print an SADB_EXT_PROPOSAL extension.
 */
void
print_prop(char *prefix, struct sadb_prop *prop)
{
	struct sadb_comb *combs;
	int i, numcombs;

	printf(gettext("%sProposal, replay counter = %u.\n"), prefix,
	    prop->sadb_prop_replay);

	numcombs = prop->sadb_prop_len - SADB_8TO64(sizeof (*prop));
	numcombs /= SADB_8TO64(sizeof (*combs));

	combs = (struct sadb_comb *)(prop + 1);

	for (i = 0; i < numcombs; i++) {
		printf(gettext("%s Combination #%u "), prefix, i + 1);
		if (combs[i].sadb_comb_auth != SADB_AALG_NONE) {
			printf(gettext("Authentication = "));
			dump_aalg(combs[i].sadb_comb_auth, stdout);
			printf(gettext("  minbits=%u, maxbits=%u.\n%s "),
			    combs[i].sadb_comb_auth_minbits,
			    combs[i].sadb_comb_auth_maxbits, prefix);
		}

		if (combs[i].sadb_comb_encrypt != SADB_EALG_NONE) {
			printf(gettext("Encryption = "));
			dump_ealg(combs[i].sadb_comb_encrypt, stdout);
			printf(gettext("  minbits=%u, maxbits=%u.\n%s "),
			    combs[i].sadb_comb_encrypt_minbits,
			    combs[i].sadb_comb_encrypt_maxbits, prefix);
		}

		printf(gettext("HARD: "));
		if (combs[i].sadb_comb_hard_allocations)
			printf(gettext("alloc=%u "),
			    combs[i].sadb_comb_hard_allocations);
		if (combs[i].sadb_comb_hard_bytes)
			printf(gettext("bytes=%llu "),
			    combs[i].sadb_comb_hard_bytes);
		if (combs[i].sadb_comb_hard_addtime)
			printf(gettext("post-add secs=%llu "),
			    combs[i].sadb_comb_hard_addtime);
		if (combs[i].sadb_comb_hard_usetime)
			printf(gettext("post-use secs=%llu"),
			    combs[i].sadb_comb_hard_usetime);

		printf(gettext("\n%s SOFT: "), prefix);
		if (combs[i].sadb_comb_soft_allocations)
			printf(gettext("alloc=%u "),
			    combs[i].sadb_comb_soft_allocations);
		if (combs[i].sadb_comb_soft_bytes)
			printf(gettext("bytes=%llu "),
			    combs[i].sadb_comb_soft_bytes);
		if (combs[i].sadb_comb_soft_addtime)
			printf(gettext("post-add secs=%llu "),
			    combs[i].sadb_comb_soft_addtime);
		if (combs[i].sadb_comb_soft_usetime)
			printf(gettext("post-use secs=%llu"),
			    combs[i].sadb_comb_soft_usetime);
		putchar('\n');
	}
}

/*
 * Print an SADB_EXT_SUPPORTED extension.
 */
void
print_supp(char *prefix, struct sadb_supported *supp)
{
	struct sadb_alg *algs;
	int i, numalgs;

	printf(gettext("%sSupported "), prefix);
	switch (supp->sadb_supported_exttype) {
	case SADB_EXT_SUPPORTED_AUTH:
		printf(gettext("authentication"));
		break;
	case SADB_EXT_SUPPORTED_ENCRYPT:
		printf(gettext("encryption"));
		break;
	}
	printf(gettext(" algorithms.\n"));

	algs = (struct sadb_alg *)(supp + 1);
	numalgs = supp->sadb_supported_len - SADB_8TO64(sizeof (*supp));
	numalgs /= SADB_8TO64(sizeof (*algs));
	for (i = 0; i < numalgs; i++) {
		printf(prefix);
		switch (supp->sadb_supported_exttype) {
		case SADB_EXT_SUPPORTED_AUTH:
			dump_aalg(algs[i].sadb_alg_id, stdout);
			break;
		case SADB_EXT_SUPPORTED_ENCRYPT:
			dump_ealg(algs[i].sadb_alg_id, stdout);
			break;
		}
		printf(gettext(" minbits=%u, maxbits=%u, ivlen=%u.\n"),
		    algs[i].sadb_alg_minbits, algs[i].sadb_alg_maxbits,
		    algs[i].sadb_alg_ivlen);
	}
}

/*
 * Print an SADB_EXT_SPIRANGE extension.
 */
void
print_spirange(char *prefix, struct sadb_spirange *range)
{
	printf(gettext("%sSPI Range, min=0x%x, max=0x%x\n"), prefix,
	    htonl(range->sadb_spirange_min),
	    htonl(range->sadb_spirange_max));
}

/*
 * Take a PF_KEY message pointed to buffer and print it.  Useful for DUMP
 * and GET.
 */
void
print_samsg(uint64_t *buffer)
{
	uint64_t *current;
	struct sadb_msg *samsg = (struct sadb_msg *)buffer;
	struct sadb_ext *ext;
	struct sadb_lifetime *currentlt = NULL, *hardlt = NULL, *softlt = NULL;
	int i;

	print_sadb_msg(samsg);
	current = (uint64_t *)(samsg + 1);
	while (current - buffer < samsg->sadb_msg_len) {
		int lenbytes;

		ext = (struct sadb_ext *)current;
		lenbytes = SADB_64TO8(ext->sadb_ext_len);
		switch (ext->sadb_ext_type) {
		case SADB_EXT_SA:
			print_sa(gettext("SA: "), (struct sadb_sa *)current);
			break;
		/*
		 * Pluck out lifetimes and print them at the end.  This is
		 * to show relative lifetimes.
		 */
		case SADB_EXT_LIFETIME_CURRENT:
			currentlt = (struct sadb_lifetime *)current;
			break;
		case SADB_EXT_LIFETIME_HARD:
			hardlt = (struct sadb_lifetime *)current;
			break;
		case SADB_EXT_LIFETIME_SOFT:
			softlt = (struct sadb_lifetime *)current;
			break;

		case SADB_EXT_ADDRESS_SRC:
			print_address(gettext("SRC: "),
			    (struct sadb_address *)current);
			break;
		case SADB_EXT_ADDRESS_DST:
			print_address(gettext("DST: "),
			    (struct sadb_address *)current);
			break;
		case SADB_EXT_ADDRESS_PROXY:
			print_address(gettext("PXY: "),
			    (struct sadb_address *)current);
			break;
		case SADB_EXT_KEY_AUTH:
			print_key(gettext("AKY: "), (struct sadb_key *)current);
			break;
		case SADB_EXT_KEY_ENCRYPT:
			print_key(gettext("EKY: "), (struct sadb_key *)current);
			break;
		case SADB_EXT_IDENTITY_SRC:
			print_ident(gettext("SID: "),
			    (struct sadb_ident *)current);
			break;
		case SADB_EXT_IDENTITY_DST:
			print_ident(gettext("DID: "),
			    (struct sadb_ident *)current);
			break;
		case SADB_EXT_SENSITIVITY:
			print_sens(gettext("SNS: "),
			    (struct sadb_sens *)current);
			break;
		case SADB_EXT_PROPOSAL:
			print_prop(gettext("PRP: "),
			    (struct sadb_prop *)current);
			break;
		case SADB_EXT_SUPPORTED_AUTH:
			print_supp(gettext("SUA: "),
			    (struct sadb_supported *)current);
			break;
		case SADB_EXT_SUPPORTED_ENCRYPT:
			print_supp(gettext("SUE: "),
			    (struct sadb_supported *)current);
			break;
		case SADB_EXT_SPIRANGE:
			print_spirange(gettext("SPR: "),
			    (struct sadb_spirange *)current);
			break;
		default:
			printf(gettext("UNK: Unknown ext. %d, len %d.\n"),
			    ext->sadb_ext_type, lenbytes);
			for (i = 0; i < ext->sadb_ext_len; i++)
				printf(gettext("UNK: 0x%llx\n"),
				    ((uint64_t *)ext)[i]);
			break;
		}
		current += ext->sadb_ext_len;
	}
	/*
	 * Print lifetimes NOW.
	 */
	if (currentlt != NULL || hardlt != NULL || softlt != NULL)
		print_lifetimes(currentlt, hardlt, softlt);

	if (current - buffer != samsg->sadb_msg_len) {
		fprintf(stderr, gettext("WARNING: "));
		fprintf(stderr,
		    gettext("insufficient buffer space or corrupt message.\n"));
	}
}

/*
 * Write a message to the PF_KEY socket.  If verbose, print the message
 * heading into the kernel.
 */
int
key_write(int fd, void *msg, size_t len)
{
	if (vflag) {
		printf(gettext("VERBOSE ON:  Message to kernel looks like:\n"));
		printf("==========================================\n");
		print_samsg(msg);
		printf("==========================================\n");
	}

	return (write(fd, msg, len));
}

/*
 * SIGALRM handler for time_critical_enter.
 */
void
time_critical_catch(int signal)
{
	if (signal == SIGALRM) {
		fprintf(stderr, "Reply message from PF_KEY timed out.\n");
	} else {
		fprintf(stderr, "Caught signal %d while trying to receive",
		    signal);
		fprintf(stderr, "PF_KEY reply message.\n");
	}
	exit(1);
}

#define	TIME_CRITICAL_TIME 10	/* In seconds */

/*
 * Enter a "time critical" section where key is waiting for a return message.
 */
void
time_critical_enter(void)
{
	signal(SIGALRM, time_critical_catch);
	alarm(TIME_CRITICAL_TIME);
}

/*
 * Exit the "time critical" section after getting an appropriate return
 * message.
 */
void
time_critical_exit(void)
{
	alarm(0);
	signal(SIGALRM, SIG_DFL);
}

/*
 * Construct a PF_KEY FLUSH message for the SA type specified.
 */
void
doflush(int satype)
{
	struct sadb_msg msg;
	int rc;

	msg_init(&msg, SADB_FLUSH, (uint8_t)satype);
	rc = key_write(keysock, &msg, sizeof (msg));
	if (rc == -1)
		Bail("write() to PF_KEY socket failed (in doflush)");

	time_critical_enter();
	do {
		rc = read(keysock, &msg, sizeof (msg));
		if (rc == -1)
			Bail("read (in doflush)");
	} while (msg.sadb_msg_seq != seq || msg.sadb_msg_pid != mypid);
	time_critical_exit();

	/*
	 * I should _never_ hit the following unless:
	 *
	 * 1. There is a kernel bug.
	 * 2. There is another process filling in its pid with mine, and
	 *    issuing a different message that would cause a different result.
	 */
	if (msg.sadb_msg_type != SADB_FLUSH ||
	    msg.sadb_msg_satype != (uint8_t)satype) {
		syslog((LOG_NOTICE|LOG_AUTH),
		    gettext("doflush: Return message not of type SADB_FLUSH!"));
		Bail("doflush: Return message not of type SADB_FLUSH!");
	}

	if (msg.sadb_msg_errno != 0) {
		errno = msg.sadb_msg_errno;
		if (errno == EINVAL) {
			fprintf(stderr, gettext("Cannot flush SA type %d.\n"),
			    satype);
		}
		Bail("return message (in doflush)");
	}
}

/*
 * save_XXX functions are used when "saving" the SA tables to either a
 * file or standard output.  They use the dump_XXX functions where needed,
 * but mostly they use the rparseXXX functions.
 */

/*
 * Print save information for a lifetime extension.
 *
 * NOTE : It saves the lifetime in absolute terms.  For example, if you
 * had a hard_usetime of 60 seconds, you'll save it as 60 seconds, even though
 * there may have been 59 seconds burned off the clock.
 */
boolean_t
save_lifetime(sadb_lifetime_t *lifetime, FILE *ofile)
{
	char *prefix;

	prefix = (lifetime->sadb_lifetime_exttype == SADB_EXT_LIFETIME_SOFT) ?
	    "soft" : "hard";

	if (putc('\t', ofile) == EOF)
		return (B_FALSE);

	if (lifetime->sadb_lifetime_allocations != 0 && fprintf(ofile,
	    "%s_alloc %u ", prefix, lifetime->sadb_lifetime_allocations) < 0)
		return (B_FALSE);

	if (lifetime->sadb_lifetime_bytes != 0 && fprintf(ofile,
	    "%s_bytes %llu ", prefix, lifetime->sadb_lifetime_bytes) < 0)
		return (B_FALSE);

	if (lifetime->sadb_lifetime_addtime != 0 && fprintf(ofile,
	    "%s_addtime %llu ", prefix, lifetime->sadb_lifetime_addtime) < 0)
		return (B_FALSE);

	if (lifetime->sadb_lifetime_usetime != 0 && fprintf(ofile,
	    "%s_usetime %llu ", prefix, lifetime->sadb_lifetime_usetime) < 0)
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Print save information for an address extension.
 */
boolean_t
save_address(sadb_address_t *addr, FILE *ofile)
{
	char *prefix, *printable_addr;
	struct sockaddr *sa = (struct sockaddr *)(addr + 1);
	struct sockaddr_in *sin;
	struct hostent *hp;

	switch (addr->sadb_address_exttype) {
	case SADB_EXT_ADDRESS_SRC:
		prefix = "src";
		break;
	case SADB_EXT_ADDRESS_DST:
		prefix = "dst";
		break;
	case SADB_EXT_ADDRESS_PROXY:
		prefix = "proxy";
		break;
	}

	if (fprintf(ofile, "    %s ", prefix) < 0)
		return (B_FALSE);

	/*
	 * Print addresses based on -n flag.
	 */
	switch (sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (nflag ||
		    ((hp = gethostbyaddr((char *)(&sin->sin_addr),
			sizeof (struct in_addr), AF_INET)) == NULL)) {
			printable_addr = inet_ntoa(sin->sin_addr);
			if (printable_addr == NULL)
				printable_addr = "<inet_ntoa() failed>";
			if (fprintf(ofile, "%s", printable_addr) < 0)
				return (B_FALSE);
		} else {
			/* Reverse name lookup already done. */
			if (fprintf(ofile, "%s", hp->h_name) < 0)
				return (B_FALSE);
		}
		break;
	/* XXX IPv6 */
	}

	return (B_TRUE);
}

/*
 * Print save information for a key extension.
 */
boolean_t
save_key(sadb_key_t *key, FILE *ofile)
{
	char *prefix;

	if (putc('\t', ofile) == EOF)
		return (B_FALSE);

	prefix = (key->sadb_key_exttype == SADB_EXT_KEY_AUTH) ? "auth" : "encr";

	if (fprintf(ofile, "%skey ", prefix) < 0)
		return (B_FALSE);

	/* XXX No checking for failures here. */
	dump_key((uint8_t *)(key + 1), key->sadb_key_bits, ofile);
	return (B_TRUE);
}

/*
 * Print save information for an identity extension.
 */
boolean_t
save_ident(sadb_ident_t *ident, FILE *ofile)
{
	char *prefix;

	if (putc('\t', ofile) == EOF)
		return (B_FALSE);

	prefix = (ident->sadb_ident_exttype == SADB_EXT_IDENTITY_SRC) ? "src" :
	    "dst";

	if (fprintf(ofile, "%sidtype %s %s", prefix,
	    rparseidtype(ident->sadb_ident_type), (char *)(ident + 1)) < 0)
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * "Save" a security association to an output file.
 *
 * NOTE the lack of calls to gettext() because I'm outputting parseable stuff.
 * ALSO NOTE that if you change keywords (see parsecmd()), you'll have to
 * change them here as well.
 */
void
save_assoc(uint64_t *buffer, FILE *ofile)
{
	uint64_t *current;
	struct sadb_msg *samsg = (struct sadb_msg *)buffer;
	struct sadb_ext *ext;
/* XXX Warning:  Don't use bail2() in an if...else without { }. */
#define	bail2(s) { int t = errno; fclose(ofile); errno = t; Bail(s); }
#define	savenl() if (fputs(" \\\n", ofile) == EOF) { bail2("savenl"); }

	if (fputs("# begin assoc\n", ofile) == EOF)
		Bail("save_assoc: Opening comment of SA");
	if (fprintf(ofile, "add %s ", rparsesatype(samsg->sadb_msg_satype)) < 0)
		Bail("save_assoc: First line of SA");
	savenl();

	current = (uint64_t *)(samsg + 1);
	while (current - buffer < samsg->sadb_msg_len) {
		sadb_sa_t *assoc;

		ext = (struct sadb_ext *)current;
		switch (ext->sadb_ext_type) {
		case SADB_EXT_SA:
			assoc = (sadb_sa_t *)ext;
			if (assoc->sadb_sa_state != SADB_SASTATE_MATURE) {
				if (fprintf(ofile, "# WARNING: SA was dying "
				    "or dead\n") < 0) {
					bail2("save_assoc: fprintf not mature");
				}
			}
			if (fprintf(ofile, "    spi 0x%x ",
			    ntohl(assoc->sadb_sa_spi)) < 0)
				bail2("save_assoc: fprintf spi");
			if (fprintf(ofile, "encr_alg %s ",
			    rparsealg(assoc->sadb_sa_encrypt, encr_algs)) < 0)
				bail2("save_assoc: fprintf encrypt");
			if (fprintf(ofile, "auth_alg %s ",
			    rparsealg(assoc->sadb_sa_auth, auth_algs)) < 0)
				bail2("save_assoc: fprintf auth");
			if (fprintf(ofile, "replay %d ",
			    assoc->sadb_sa_replay) < 0)
				bail2("save_assoc: fprintf replay");
			savenl();
			break;
		case SADB_EXT_LIFETIME_HARD:
		case SADB_EXT_LIFETIME_SOFT:
			if (!save_lifetime((sadb_lifetime_t *)ext, ofile))
				bail2("save_lifetime");
			savenl();
			break;
		case SADB_EXT_ADDRESS_SRC:
		case SADB_EXT_ADDRESS_DST:
		case SADB_EXT_ADDRESS_PROXY:
			if (!save_address((sadb_address_t *)ext, ofile))
				bail2("save_address");
			savenl();
			break;
		case SADB_EXT_KEY_AUTH:
		case SADB_EXT_KEY_ENCRYPT:
			if (!save_key((sadb_key_t *)ext, ofile))
				bail2("save_address");
			savenl();
			break;
		case SADB_EXT_IDENTITY_SRC:
		case SADB_EXT_IDENTITY_DST:
			if (!save_ident((sadb_ident_t *)ext, ofile))
				bail2("save_address");
			savenl();
			break;
		case SADB_EXT_SENSITIVITY:
		default:
			/* Skip over irrelevant extensions. */
			break;
		}
		current += ext->sadb_ext_len;
	}

	if (fputs(gettext("\n# end assoc\n\n"), ofile) == EOF)
		bail2("save_assoc: last fputs");
}

/*
 * Because "save" and "dump" both use the SADB_DUMP message, fold both
 * into the same function.
 */
void
dodump(int satype, FILE *ofile)
{
	struct sadb_msg *msg = (struct sadb_msg *)get_buffer;
	int rc;

	if (ofile != NULL) {
		fprintf(ofile, gettext("# This key file was generated by the"));
		fprintf(ofile,
		    gettext(" ipseckey(1m) command's 'save' feature.\n\n"));
	}
	msg_init(msg, SADB_DUMP, (uint8_t)satype);
	rc = key_write(keysock, msg, sizeof (*msg));
	if (rc == -1)
		Bail("write to PF_KEY socket failed (in dodump)");

	do {
		/*
		 * For DUMP, do only the read as a time critical section.
		 */
		time_critical_enter();
		rc = read(keysock, get_buffer, sizeof (get_buffer));
		time_critical_exit();
		if (rc == -1)
			Bail("read (in dodump)");
		if (msg->sadb_msg_pid == mypid &&
		    msg->sadb_msg_type == SADB_DUMP &&
		    msg->sadb_msg_seq != 0 &&
		    msg->sadb_msg_errno == 0) {
			if (ofile == NULL) {
				print_samsg(get_buffer);
				putchar('\n');
			} else {
				save_assoc(get_buffer, ofile);
			}
		}
	} while (msg->sadb_msg_pid != mypid ||
	    (msg->sadb_msg_errno == 0 && msg->sadb_msg_seq != 0));

	if (ofile != NULL && ofile != stdout)
		fclose(ofile);

	if (msg->sadb_msg_errno == 0) {
		if (ofile == NULL)
			printf(gettext("Dump succeeded for SA type %d.\n"),
			    satype);
	} else {
		errno = msg->sadb_msg_errno;
		Bail("Dump failed");
	}
}

/*
 * Perform an add or an update.  ADD and UPDATE are similar in the extension
 * they need.
 */
void
doaddup(int cmd, int satype, char *argv[])
{
	uint8_t *buffer, *nexthdr;
	struct sadb_msg msg, *msgp;
	struct sadb_sa *assoc = NULL;
	struct sadb_address *src = NULL, *dst = NULL, *proxy = NULL;
	struct sadb_key *encrypt = NULL, *auth = NULL;
	struct sadb_ident *srcid = NULL, *dstid = NULL;
	struct sadb_lifetime *hard = NULL, *soft = NULL;  /* Current? */
	/* XXX IPv6 : need lsockaddr for IPv6. */
	struct sockaddr sa;
	/* XXX Need sensitivity eventually. */
	int next, token, sa_len, rc, alloclen, totallen = sizeof (msg);
	uint32_t spi;
	char *thiscmd;
	boolean_t readstate = B_FALSE;

	thiscmd = (cmd == CMD_ADD) ? "add" : "update";

	msg_init(&msg, ((cmd == CMD_ADD) ? SADB_ADD : SADB_UPDATE),
	    (uint8_t)satype);

	/* Assume last element in argv is set to NULL. */
	do {
		token = parseextval(*argv, &next);
		argv++;
		switch (token) {
		case TOK_EOF:
			/* Do nothing, I'm done. */
			break;
		case TOK_UNKNOWN:
			fprintf(stderr,
			    gettext("Unknown extension field %s.\n"),
			    *(argv - 1));
			usage();	/* Will exit program. */
			break;
		case TOK_SPI:
		case TOK_REPLAY:
		case TOK_STATE:
		case TOK_AUTHALG:
		case TOK_ENCRALG:
			/*
			 * May want to place this chunk of code in a function.
			 *
			 * This code checks for duplicate entries on a command
			 * line.
			 */

			/* Allocate the SADB_EXT_SA extension. */
			if (assoc == NULL) {
				assoc = malloc(sizeof (*assoc));
				if (assoc == NULL)
					Bail("malloc(assoc)");
				bzero(assoc, sizeof (*assoc));
				assoc->sadb_sa_exttype = SADB_EXT_SA;
				assoc->sadb_sa_len =
				    SADB_8TO64(sizeof (*assoc));
				totallen += sizeof (*assoc);
			}
			switch (token) {
			case TOK_SPI:
				/*
				 * If some cretin types in "spi 0" then he/she
				 * can type in another SPI.
				 */
				if (assoc->sadb_sa_spi != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single SPI value.\n"));
					usage();
				}
				/* Must convert SPI to network order! */
				assoc->sadb_sa_spi =
				    htonl((uint32_t)parsenum(*argv, B_TRUE));
				break;
			case TOK_REPLAY:
				/*
				 * That same cretin can do the same with
				 * replay.
				 */
				if (assoc->sadb_sa_replay != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single replay wsize.\n"));
					usage();
				}
				assoc->sadb_sa_replay =
				    (uint8_t)parsenum(*argv, B_TRUE);
				if (assoc->sadb_sa_replay != 0) {
					fprintf(stderr, gettext(
					    "WARNING:  Replay with manual"
					    " keying considered harmful.\n"));
				}
				break;
			case TOK_STATE:
				/*
				 * 0 is an actual state value, LARVAL.  This
				 * means that one can type in the larval state
				 * and then type in another state on the same
				 * command line.
				 */
				if (assoc->sadb_sa_state != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single SA state.\n"));
					usage();
				}
				assoc->sadb_sa_state = parsestate(*argv);
				readstate = B_TRUE;
				break;
			case TOK_AUTHALG:
				if (assoc->sadb_sa_auth != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single auth algorithm.\n"));
					usage();
				}
				assoc->sadb_sa_auth = parsealg(*argv,
				    auth_algs);
				break;
			case TOK_ENCRALG:
				if (assoc->sadb_sa_encrypt != 0) {
					fprintf(stderr,
					    gettext("Can only specify single "
						"encryption algorithm.\n"));
					usage();
				}
				assoc->sadb_sa_encrypt = parsealg(*argv,
				    encr_algs);
				break;
			}
			argv++;
			break;
		case TOK_SRCADDR:
			if (src != NULL) {
				fprintf(stderr,
				    gettext("Can only specify "
					"single source address.\n"));
				usage();
			}
			sa_len = parseaddr(*argv, &sa);
			argv++;
			/*
			 * Round of the sockaddr length to an 8 byte
			 * boundary to make PF_KEY happy.
			 */
			alloclen = sizeof (*src) + roundup(sa_len, 8);
			src = malloc(alloclen);
			if (src == NULL)
				Bail("malloc(src)");
			totallen += alloclen;
			src->sadb_address_len = SADB_8TO64(alloclen);
			src->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			src->sadb_address_reserved = 0;
			src->sadb_address_prefixlen = 0;
			src->sadb_address_proto = 0;
			bcopy(&sa, (src + 1), sa_len);
			break;
		case TOK_DSTADDR:
			if (dst != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single "
				    "destination address.\n"));
				usage();
			}
			sa_len = parseaddr(*argv, &sa);
			argv++;
			alloclen = sizeof (*dst) + roundup(sa_len, 8);
			dst = malloc(alloclen);
			if (dst == NULL)
				Bail("malloc(dst)");
			totallen += alloclen;
			dst->sadb_address_len = SADB_8TO64(alloclen);
			dst->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			dst->sadb_address_reserved = 0;
			dst->sadb_address_prefixlen = 0;
			dst->sadb_address_proto = 0;
			bcopy(&sa, (dst + 1), sa_len);
			break;
		case TOK_PROXYADDR:
			if (proxy != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single "
					"proxy address.\n"));
				usage();
			}
			sa_len = parseaddr(*argv, &sa);
			argv++;
			alloclen = sizeof (*proxy) + roundup(sa_len, 8);
			proxy = malloc(alloclen);
			if (proxy == NULL)
				Bail("malloc(proxy)");
			totallen += alloclen;
			proxy->sadb_address_len = SADB_8TO64(alloclen);
			proxy->sadb_address_exttype = SADB_EXT_ADDRESS_PROXY;
			proxy->sadb_address_reserved = 0;
			proxy->sadb_address_prefixlen = 0;
			proxy->sadb_address_proto = 0;
			bcopy(&sa, (proxy + 1), sa_len);
			break;
		case TOK_ENCRKEY:
			if (encrypt != NULL) {
				fprintf(stderr,
				    gettext("Can only specify "
					"single encryption key.\n"));
				usage();
			}
			encrypt = parsekey(*argv);
			totallen += SADB_64TO8(encrypt->sadb_key_len);
			argv++;
			encrypt->sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
			break;
		case TOK_AUTHKEY:
			if (auth != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single"
					" authentication key.\n"));
				usage();
			}
			auth = parsekey(*argv);
			argv++;
			totallen += SADB_64TO8(auth->sadb_key_len);
			auth->sadb_key_exttype = SADB_EXT_KEY_AUTH;
			break;
		case TOK_SRCIDTYPE:
			if (*argv == NULL || *(argv + 1) == NULL) {
				fprintf(stderr,
				    gettext("Unexpected end of command "
					" line.\n"));
				usage();
			}
			if (srcid != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single"
					" source certificate identity.\n"));
				usage();
			}
			alloclen = sizeof (*srcid) +
			    roundup(strlen(*(argv + 1)) + 1, 8);
			srcid = malloc(alloclen);
			if (srcid == NULL)
				Bail("malloc(srcid)");
			totallen += alloclen;
			srcid->sadb_ident_type = parseidtype(*argv);
			argv++;
			srcid->sadb_ident_len = SADB_8TO64(alloclen);
			srcid->sadb_ident_exttype = SADB_EXT_IDENTITY_SRC;
			srcid->sadb_ident_reserved = 0;
			srcid->sadb_ident_id = 0;  /* Not useful here. */
			/* Can use strcpy because I allocate my own memory. */
			strcpy((char *)(srcid + 1), *argv);
			argv++;
			break;
		case TOK_DSTIDTYPE:
			if (*argv == NULL || *(argv + 1) == NULL) {
				fprintf(stderr,
				    gettext("Unexpected end of command"
					" line.\n"));
				usage();
			}
			if (dstid != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single destina"
					"tion certificate identity.\n"));
				usage();
			}
			alloclen = sizeof (*dstid) +
			    roundup(strlen(*(argv + 1)) + 1, 8);
			dstid = malloc(alloclen);
			if (dstid == NULL)
				Bail("malloc(dstid)");
			totallen += alloclen;
			dstid->sadb_ident_type = parseidtype(*argv);
			argv++;
			dstid->sadb_ident_len = SADB_8TO64(alloclen);
			dstid->sadb_ident_exttype = SADB_EXT_IDENTITY_DST;
			dstid->sadb_ident_reserved = 0;
			dstid->sadb_ident_id = 0;  /* Not useful here. */
			/* Can use strcpy because I allocate my own memory. */
			strcpy((char *)(dstid + 1), *argv);
			argv++;
			break;
		case TOK_HARD_ALLOC:
		case TOK_HARD_BYTES:
		case TOK_HARD_ADDTIME:
		case TOK_HARD_USETIME:
			if (hard == NULL) {
				hard = malloc(sizeof (*hard));
				if (hard == NULL)
					Bail("malloc(hard_lifetime)");
				bzero(hard, sizeof (*hard));
				hard->sadb_lifetime_exttype =
				    SADB_EXT_LIFETIME_HARD;
				hard->sadb_lifetime_len =
				    SADB_8TO64(sizeof (*hard));
				totallen += sizeof (*hard);
			}
			switch (token) {
			case TOK_HARD_ALLOC:
				if (hard->sadb_lifetime_allocations != 0) {
					fprintf(stderr,
					    gettext("Can only specify single"
						" hard allocation limit.\n"));
					usage();
				}
				hard->sadb_lifetime_allocations =
				    (uint32_t)parsenum(*argv, B_TRUE);
				break;
			case TOK_HARD_BYTES:
				if (hard->sadb_lifetime_bytes != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single hard byte limit.\n"));
					usage();
				}
				hard->sadb_lifetime_bytes = parsenum(*argv,
				    B_TRUE);
				break;
			case TOK_HARD_ADDTIME:
				if (hard->sadb_lifetime_addtime != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single past-add lifetime.\n"));
					usage();
				}
				hard->sadb_lifetime_addtime = parsenum(*argv,
				    B_TRUE);
				break;
			case TOK_HARD_USETIME:
				if (hard->sadb_lifetime_usetime != 0) {
					fprintf(stderr,
					    gettext("Can only specify "
						"single past-use lifetime.\n"));
					usage();
				}
				hard->sadb_lifetime_usetime = parsenum(*argv,
				    B_TRUE);
				break;
			}
			argv++;
			break;
		case TOK_SOFT_ALLOC:
		case TOK_SOFT_BYTES:
		case TOK_SOFT_ADDTIME:
		case TOK_SOFT_USETIME:
			if (soft == NULL) {
				soft = malloc(sizeof (*soft));
				if (soft == NULL)
					Bail("malloc(soft_lifetime)");
				bzero(soft, sizeof (*soft));
				soft->sadb_lifetime_exttype =
				    SADB_EXT_LIFETIME_SOFT;
				soft->sadb_lifetime_len =
				    SADB_8TO64(sizeof (*soft));
				totallen += sizeof (*soft);
			}
			switch (token) {
			case TOK_SOFT_ALLOC:
				if (soft->sadb_lifetime_allocations != 0) {
					fprintf(stderr,
					    gettext("Can only specify single "
						"soft allocation limit.\n"));
					usage();
				}
				soft->sadb_lifetime_allocations =
				    (uint32_t)parsenum(*argv, B_TRUE);
				break;
			case TOK_SOFT_BYTES:
				if (soft->sadb_lifetime_bytes != 0) {
					fprintf(stderr,
					    gettext("Can only specify single "
						"soft byte limit.\n"));
					usage();
				}
				soft->sadb_lifetime_bytes = parsenum(*argv,
				    B_TRUE);
				break;
			case TOK_SOFT_ADDTIME:
				if (soft->sadb_lifetime_addtime != 0) {
					fprintf(stderr,
					    gettext("Can only specify single "
						"past-add lifetime.\n"));
					usage();
				}
				soft->sadb_lifetime_addtime = parsenum(*argv,
				    B_TRUE);
				break;
			case TOK_SOFT_USETIME:
				if (soft->sadb_lifetime_usetime != 0) {
					fprintf(stderr,
					    gettext("Can only specify single "
						"past-use lifetime.\n"));
					usage();
				}
				soft->sadb_lifetime_usetime = parsenum(*argv,
				    B_TRUE);
				break;
			}
			argv++;
			break;
		default:
			fprintf(stderr,
			    gettext("Don't use extension %s for add/update.\n"),
			    *(argv - 1));
			usage();
			break;
		}
	} while (token != TOK_EOF);

	/*
	 * Okay, so now I have all of the potential extensions!
	 * Allocate a single contiguous buffer.  Keep in mind that it'll
	 * be enough because the key itself will be yanked.
	 */

	if (src == NULL && dst != NULL) {
		/*
		 * Set explicit unspecified source address.
		 */
		struct sockaddr *sa;
		size_t lenbytes = SADB_64TO8(dst->sadb_address_len);

		/* XXX In case all-zeroes sockaddr isn't enough... */
		/* struct sockaddr_in *sin; */
		/* XXX IPv6 : struct sockaddr_in6 *sin6; */

		totallen += lenbytes;
		src = malloc(lenbytes);
		if (src == NULL)
			Bail("malloc(implicit src)");
		/* Confusing, but we're copying from DST to SRC.  :) */
		bcopy(dst, src, lenbytes);
		src->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
		sa = (struct sockaddr *)(src + 1);
		switch (sa->sa_family) {
		/* XXX IPv6 : case AF_INET6: */
		case AF_INET:
		default:
			bzero(sa->sa_data, lenbytes - sizeof (*src) -
			    sizeof (sa->sa_family));
			break;
		}
	}
	msg.sadb_msg_len = SADB_8TO64(totallen);

	buffer = malloc(totallen);
	nexthdr = buffer;
	bcopy(&msg, nexthdr, sizeof (msg));
	nexthdr += sizeof (msg);
	if (assoc != NULL) {
		if (assoc->sadb_sa_spi == 0) {
			fprintf(stderr, gettext("The SPI value is missing for "
			    "the association you wish to %s.\n"), thiscmd);
			usage();
		}
		if (assoc->sadb_sa_auth == 0 && assoc->sadb_sa_encrypt == 0 &&
			cmd == CMD_ADD) {
			fprintf(stderr,
			    gettext("Select at least one algorithm "
				"for this add.\n"));
			usage();
		}

		/* Hack to let user specify NULL ESP implicitly. */
		if (msg.sadb_msg_satype == SADB_SATYPE_ESP &&
		    assoc->sadb_sa_encrypt == 0)
			assoc->sadb_sa_encrypt = SADB_EALG_NULL;

		/* 0 is an actual value.  Print a warning if it was entered. */
		if (assoc->sadb_sa_state == 0) {
			if (readstate)
				fputs(gettext(
				    "WARNING: Cannot set LARVAL SA state.\n"),
				    stderr);
			assoc->sadb_sa_state = SADB_SASTATE_MATURE;
		}
		bcopy(assoc, nexthdr, SADB_64TO8(assoc->sadb_sa_len));
		nexthdr += SADB_64TO8(assoc->sadb_sa_len);
		/* Save the SPI for the case of an error. */
		spi = assoc->sadb_sa_spi;
		free(assoc);
	} else {
		fprintf(stderr, gettext("Need SA parameters for %s.\n"),
		    thiscmd);
		usage();
	}

	if (hard != NULL) {
		bcopy(hard, nexthdr, SADB_64TO8(hard->sadb_lifetime_len));
		nexthdr += SADB_64TO8(hard->sadb_lifetime_len);
		free(hard);
	}

	if (soft != NULL) {
		bcopy(soft, nexthdr, SADB_64TO8(soft->sadb_lifetime_len));
		nexthdr += SADB_64TO8(soft->sadb_lifetime_len);
		free(soft);
	}

	if (dst != NULL) {
		bcopy(dst, nexthdr, SADB_64TO8(dst->sadb_address_len));
		nexthdr += SADB_64TO8(dst->sadb_address_len);
		free(dst);
	} else {
		fprintf(stderr, gettext("Need destination address for %s.\n"),
		    thiscmd);
		usage();
	}

	/* Source has to be set.  Destination reality check was above. */
	bcopy(src, nexthdr, SADB_64TO8(src->sadb_address_len));
	nexthdr += SADB_64TO8(src->sadb_address_len);
	free(src);

	if (proxy != NULL) {
		bcopy(proxy, nexthdr, SADB_64TO8(proxy->sadb_address_len));
		nexthdr += SADB_64TO8(proxy->sadb_address_len);
		free(proxy);
	}

	if (encrypt == NULL && auth == NULL && cmd == CMD_ADD) {
		fprintf(stderr,
		    gettext("Must have at least one key for an add.\n"));
		usage();
	}

	if (encrypt != NULL) {
		bcopy(encrypt, nexthdr, SADB_64TO8(encrypt->sadb_key_len));
		nexthdr += SADB_64TO8(encrypt->sadb_key_len);
		bzero(encrypt, SADB_64TO8(encrypt->sadb_key_len));
		free(encrypt);
	}

	if (auth != NULL) {
		bcopy(auth, nexthdr, SADB_64TO8(auth->sadb_key_len));
		nexthdr += SADB_64TO8(auth->sadb_key_len);
		bzero(auth, SADB_64TO8(auth->sadb_key_len));
		free(auth);
	}

	if (srcid != NULL) {
		bcopy(srcid, nexthdr, SADB_64TO8(srcid->sadb_ident_len));
		nexthdr += SADB_64TO8(srcid->sadb_ident_len);
		free(srcid);
	}

	if (dstid != NULL) {
		bcopy(dstid, nexthdr, SADB_64TO8(dstid->sadb_ident_len));
		nexthdr += SADB_64TO8(dstid->sadb_ident_len);
		free(dstid);
	}

	rc = key_write(keysock, buffer, totallen);
	if (rc == -1)
		Bail("write() to PF_KEY socket (in doaddup)");

	/* Blank the key for paranoia's sake. */
	bzero(buffer, totallen);
	msgp = (struct sadb_msg *)buffer;
	time_critical_enter();
	do {
		rc = read(keysock, buffer, totallen);
		if (rc == -1)
			Bail("read (in doaddup)");
	} while (msgp->sadb_msg_seq != seq || msgp->sadb_msg_pid != mypid);
	time_critical_exit();

	/*
	 * I should _never_ hit the following unless:
	 *
	 * 1. There is a kernel bug.
	 * 2. There is another process filling in its pid with mine, and
	 *    issuing a different message that would cause a different result.
	 */
	if (msgp->sadb_msg_type !=
	    ((cmd == CMD_ADD) ? SADB_ADD : SADB_UPDATE) ||
	    msgp->sadb_msg_satype != (uint8_t)satype) {
		syslog((LOG_NOTICE|LOG_AUTH), gettext(
		    "doaddup: Return message not of type ADD/UPDATE!"));
		Bail("doaddup: Return message not of type ADD/UPDATE!");
	}

	if (msgp->sadb_msg_errno != 0) {
		errno = msgp->sadb_msg_errno;
		if (errno == EEXIST) {
			fprintf(stderr,
			    gettext("Association (type = %s) with spi 0x%x "),
			    rparsesatype(msgp->sadb_msg_satype), ntohl(spi));
			fputs(gettext(
			    "(and matching addresses) already exists.\n"),
			    stderr);
		} else {
			if (errno == EINVAL) {
				fputs(gettext("One of entered the values is "
				    "incorrect.\n"), stderr);
			}
			Bail("return message (in doaddup)");
		}
	}
}

/*
 * DELETE and GET are similar, in that they only need the extensions
 * required to _find_ and SA, and then either delete it or obatain its
 * information.
 */
void
dodelget(int cmd, int satype, char *argv[])
{
	/* XXX IPv6 : sockaddr will become lsockaddr when IPv6 arrives. */
	struct sadb_msg *msg = (struct sadb_msg *)get_buffer;
	uint64_t *nextext;
	struct sadb_sa *assoc = NULL;
	struct sadb_address *src = NULL, *dst = NULL;
	int next, token, sa_len, rc;
	char *thiscmd;
	uint32_t spi;

	msg_init(msg, ((cmd == CMD_GET) ? SADB_GET : SADB_DELETE),
	    (uint8_t)satype);
	/* Set the first extension header to right past the base message. */
	nextext = (uint64_t *)(msg + 1);
	bzero(nextext, sizeof (get_buffer) - sizeof (*msg));

	thiscmd = (cmd == CMD_GET) ? "get" : "delete";

	/* Assume last element in argv is set to NULL. */
	do {
		token = parseextval(*argv, &next);
		argv++;
		switch (token) {
		case TOK_EOF:
			/* Do nothing, I'm done. */
			break;
		case TOK_UNKNOWN:
			fprintf(stderr,
			    gettext("Unknown extension field %s.\n"),
			    *(argv - 1));
			usage();	/* Will exit program. */
			break;
		case TOK_SPI:
			if (assoc != NULL) {
				fprintf(stderr, gettext("Can only specify "
				    "single SPI value.\n"));
				usage();
			}
			assoc = (struct sadb_sa *)nextext;
			nextext = (uint64_t *)(assoc + 1);
			assoc->sadb_sa_len = SADB_8TO64(sizeof (*assoc));
			assoc->sadb_sa_exttype = SADB_EXT_SA;
			assoc->sadb_sa_spi = htonl((uint32_t)parsenum(*argv,
			    B_TRUE));
			spi = assoc->sadb_sa_spi;
			argv++;
			break;
		case TOK_SRCADDR:
			if (src != NULL) {
				fprintf(stderr, gettext("Can only specify "
				    "single source addr.\n"));
				usage();
			}
			/* Everything else after this structure is bzeroed. */
			src = (struct sadb_address *)nextext;
			nextext = (uint64_t *)(src + 1);
			src->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			/* We aren't done with nextext yet... */
			sa_len = parseaddr(*argv, (struct sockaddr *)nextext);
			argv++;
			/* NOW we can figure out the length. */
			nextext += SADB_8TO64(roundup(sa_len, 8));
			src->sadb_address_len = nextext - ((uint64_t *)src);
			break;
		case TOK_DSTADDR:
			if (dst != NULL) {
				fprintf(stderr,
				    gettext("Can only specify single dest. "
					"addr.\n"));
				usage();
			}
			/* Everything else after this structure is bzeroed. */
			dst = (struct sadb_address *)nextext;
			nextext = (uint64_t *)(dst + 1);
			dst->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			/* We aren't done with nextext yet... */
			sa_len = parseaddr(*argv, (struct sockaddr *)nextext);
			argv++;
			/* NOW we can figure out the length. */
			nextext += SADB_8TO64(roundup(sa_len, 8));
			dst->sadb_address_len = nextext - ((uint64_t *)dst);
			break;
		default:
			fprintf(stderr, gettext("Don't use extension %s "
			    "for '%s' command.\n"), *(argv - 1), thiscmd);
			usage();	/* Will exit program. */
			break;
		}
	} while (token != TOK_EOF);

	if (assoc == NULL) {
		fprintf(stderr, gettext("The SPI value is missing for the "
		    "association you wish to %s.\n"), thiscmd);
		usage();
	}

	if (dst == NULL) {
		fprintf(stderr, gettext("The destination address is missing "
		    "for the association you wish to %s.\n"), thiscmd);
		usage();
	}

	/* So I have enough of the message to send it down! */
	msg->sadb_msg_len = nextext - get_buffer;
	rc = key_write(keysock, get_buffer, SADB_64TO8(msg->sadb_msg_len));
	if (rc == -1)
		Bail("write() to PF_KEY socket (in dodelget)");

	bzero(get_buffer, sizeof (get_buffer));
	time_critical_enter();
	do {
		rc = read(keysock, get_buffer, sizeof (get_buffer));
		if (rc == -1)
			Bail("read (in dodelget)");
	} while (msg->sadb_msg_seq != seq || msg->sadb_msg_pid != mypid);
	time_critical_exit();

	/*
	 * I should _never_ hit the following unless:
	 *
	 * 1. There is a kernel bug.
	 * 2. There is another process filling in its pid with mine, and
	 *    issuing a different message that would cause a different result.
	 */
	if (msg->sadb_msg_type != ((cmd == CMD_GET) ? SADB_GET : SADB_DELETE) ||
	    msg->sadb_msg_satype != (uint8_t)satype) {
		syslog((LOG_NOTICE|LOG_AUTH), gettext(
		    "dodelget: Return message not of type GET/DELETE!"));
		Bail("dodelget: Return message not of type GET/DELETE!");
	}

	if (msg->sadb_msg_errno != 0) {
		errno = msg->sadb_msg_errno;
		if (errno == ESRCH) {
			fprintf(stderr,
			    gettext("Association (type = %s) with spi 0x%x "),
			    rparsesatype(msg->sadb_msg_satype), ntohl(spi));
			fputs(gettext(
			    "(and matching addresses) does not exist.\n"),
			    stderr);
			return;
		} else {
			if (errno == EINVAL) {
				fputs(gettext("One of entered the values is "
				    "incorrect.\n"), stderr);
			}
			Bail("return message (in dodelget)");
		}
	}

	if (cmd == CMD_GET) {
		if (SADB_64TO8(msg->sadb_msg_len) > MAX_GET_SIZE) {
			fprintf(stderr, gettext("WARNING:  "
			    "SA information bigger than %d bytes.\n"),
			    MAX_GET_SIZE);
		}
		print_samsg(get_buffer);
	}
}

/*
 * I want "key monitor" to exit very gracefully if ^C is tapped.
 */
void
monitor_catch(int signal)
{
	fprintf(stderr, gettext("Bailing on signal %d.\n"), signal);
	exit(signal);
}

/*
 * Loop forever, listening on PF_KEY messages.
 */
void
domonitor(boolean_t passive)
{
	struct sadb_msg *samsg;
	int rc;

	/* Catch ^C. */
	signal(SIGINT, monitor_catch);

	samsg = (struct sadb_msg *)get_buffer;
	if (!passive) {
		printf(gettext("Actively"));
		msg_init(samsg, SADB_X_PROMISC, 1);	/* Turn ON promisc. */
		rc = key_write(keysock, samsg, sizeof (*samsg));
		if (rc == -1)
			Bail("write (SADB_X_PROMISC)");
	} else {
		printf(gettext("Passively"));
	}
	printf(gettext(" monitoring the PF_KEY socket.\n"));

	for (; ; ) {
		/*
		 * I assume that read() is non-blocking, and will never
		 * return 0.
		 */
		rc = read(keysock, samsg, sizeof (get_buffer));
		if (rc == -1)
			Bail("read (in domonitor)");
		printf(gettext("Read %d bytes.\n"), rc);
		/*
		 * Q:  Should I use the same method of printing as GET does?
		 * A:  For now, yes.
		 */
		print_samsg(get_buffer);
		putchar('\n');
	}
}

#define	START_ARG	8
#define	TOO_MANY_ARGS	(START_ARG << 9)

/*
 * Slice an argv/argc vector from an interactive line or a read-file line.
 */

/* Return codes */
#define	TOO_MANY_TOKENS		-3
#define	MEMORY_ALLOCATION	-2
#define	COMMENT_LINE		1
#define	SUCCESS			0

int
create_argv(char *ibuf, int *newargc, char ***thisargv)
{
	unsigned int argvlen = START_ARG;
	char **current;
	boolean_t firstchar = B_TRUE;

	*thisargv = malloc(sizeof (char *) * argvlen);
	if (thisargv == NULL)
		return (MEMORY_ALLOCATION);
	current = *thisargv;
	*current = NULL;

	for (; *ibuf != '\0'; ibuf++) {
		if (isspace(*ibuf)) {
			if (*current != NULL) {
				*ibuf = '\0';
				current++;
				if (*thisargv + argvlen == current) {
					/* Regrow ***thisargv. */
					if (argvlen == TOO_MANY_ARGS) {
						free(*thisargv);
						return (TOO_MANY_TOKENS);
					}
					/* Double the allocation. */
					current = realloc(*thisargv,
					    sizeof (char *) * (argvlen << 1));
					if (current == NULL) {
						free(*thisargv);
						return (MEMORY_ALLOCATION);
					}
					*thisargv = current;
					current += argvlen;
					argvlen <<= 1;	/* Double the size. */
				}
				*current = NULL;
			}
		} else {
			if (firstchar) {
				firstchar = B_FALSE;
				if (*ibuf == COMMENT_CHAR) {
					free(*thisargv);
					return (COMMENT_LINE);
				}
			}
			if (*current == NULL) {
				*current = ibuf;
				(*newargc)++;
			}
		}
	}

	/*
	 * Tricky corner case...
	 * I've parsed _exactly_ the amount of args as I have space.  It
	 * won't return NULL-terminated, and bad things will happen to
	 * the caller.
	 */
	if (argvlen == *newargc) {
		current = realloc(*thisargv, sizeof (char *) * (argvlen + 1));
		if (current == NULL) {
			free(*thisargv);
			return (MEMORY_ALLOCATION);
		}
		*thisargv = current;
		current[argvlen] = NULL;
	}

	return (SUCCESS);
}

/*
 * Open the output file for the "save" command.
 */
FILE *
opensavefile(char *filename)
{
	int fd;
	FILE *retval;
	struct stat buf;

	/*
	 * If the user specifies "-" or doesn't give a filename, then
	 * dump to stdout.  Make sure to document the dangers of files
	 * that are NFS, directing your output to strange places, etc.
	 */
	if (filename == NULL || strcmp("-", filename) == 0)
		return (stdout);

	/*
	 * open the file with the create bits set.  Since I check for
	 * real UID == root in main(), I won't worry about the ownership
	 * problem.
	 */
	fd = open(filename, O_WRONLY | O_EXCL | O_CREAT | O_TRUNC, S_IRUSR);
	if (fd == -1) {
		if (errno != EEXIST)
			Bail("'save' filename open");
		fd = open(filename, O_WRONLY | O_TRUNC, 0);
		if (fd == -1)
			Bail("'save' filename open");
		if (fstat(fd, &buf) == -1) {
			close(fd);
			bail("'save' fstat");
		}
		if (S_ISREG(buf.st_mode) &&
		    ((buf.st_mode & S_IAMB) != S_IRUSR)) {
			fprintf(stderr,
			    gettext("WARNING: Save file already exists with "
				"permission %o.\n"), buf.st_mode & S_IAMB);
			fprintf(stderr,
			    gettext("Normal users may be able to read IPsec "
				"keying material.\n"));
		}
	}

	/* Okay, we have an FD.  Assign it to a stdio FILE pointer. */
	retval = fdopen(fd, "w");
	if (retval == NULL) {
		close(fd);
		Bail("'save' filename fdopen");
	}
	return (retval);
}

/*
 * Either mask or unmask all relevant signals.
 */
void
mask_signals(boolean_t unmask)
{
	sigset_t set;
	static sigset_t oset;

	if (unmask) {
		(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	} else {
		sigfillset(&set);
		(void) sigprocmask(SIG_SETMASK, &set, &oset);
	}
}

/*
 * Print help text.
 */
#define	puts_tr(s) puts(gettext(s))

void
dohelp(void)
{
	puts_tr("Commands");
	puts_tr("--------");
	puts_tr("?, help  - This list");
	puts_tr("quit, exit - Exit the program");
	puts_tr("monitor - Monitor all PF_KEY reply messages.");
	puts_tr("pmonitor, passive_monitor - Monitor PF_KEY messages that");
	puts_tr("                            reply to all PF_KEY sockets.");
	puts_tr("");
	puts_tr("The following commands are of the form:");
	puts_tr("    <command> {SA type} {attribute value}*");
	puts_tr("");
	puts_tr("add (interactive only) - Add a new security association (SA)");
	puts_tr("update (interactive only) - Update an existing SA");
	puts_tr("delete - Delete an SA");
	puts_tr("get - Display an SA");
	puts_tr("flush - Delete all SAs");
	puts_tr("dump - Display all SAs");
	puts_tr("save - Save SAs to a file");
}

/*
 * "Parse" a command line from argv/argc.
 */
void
parseit(int argc, char *argv[])
{
	int cmd, satype;

	if (argc == 0)
		return;
	cmd = parsecmd(*argv);
	argc--;
	argv++;
	satype = parsesatype(*argv);
	if (satype != SADB_SATYPE_UNSPEC) {
		argc--;
		argv++;
	} else {
		/*
		 * You must specify either "all" or a specific SA type
		 * for the "save" command.
		 */
		if (cmd == CMD_SAVE)
			if (*argv == NULL) {
				fprintf(stderr,
				    gettext("Must specify a specific "
					"SA type for save.\n"));
				usage();
			} else {
				argc--;
				argv++;
			}
	}

	switch (cmd) {
	case CMD_FLUSH:
		doflush(satype);
		break;
	case CMD_ADD:
	case CMD_UPDATE:
		/*
		 * NOTE: Shouldn't allow ADDs or UPDATEs with keying material
		 * from the command line.
		 */
		if (!interactive) {
			fprintf(stderr, gettext("can't do ADD or UPDATE "
			    "from the command line.\n"));
			exit(1);
		}
		if (satype == SADB_SATYPE_UNSPEC) {
			fprintf(stderr, gettext("Must specify a specific "
			    "SA type.\n"));
			usage();
			/* NOTREACHED */
		}
		/* Parse for extensions, including keying material. */
		doaddup(cmd, satype, argv);
		break;
	case CMD_DELETE:
	case CMD_GET:
		if (satype == SADB_SATYPE_UNSPEC) {
			fprintf(stderr, gettext("Must specify a single "
			    "SA type.\n"));
			usage();
			/* NOTREACHED */
		}
		/* Parse for bare minimum to locate an SA. */
		dodelget(cmd, satype, argv);
		break;
	case CMD_MONITOR:
		domonitor(B_FALSE);
		break;
	case CMD_PMONITOR:
		domonitor(B_TRUE);
		break;
	case CMD_DUMP:
		dodump(satype, NULL);
		break;
	case CMD_SAVE:
		mask_signals(B_FALSE);	/* Mask signals */
		dodump(satype, opensavefile(argv[0]));
		mask_signals(B_TRUE);	/* Unmask signals */
		break;
	case CMD_HELP:
		dohelp();
		break;
	case CMD_QUIT:
		if (interactive)
			exit(0);
		break;
	default:
		fprintf(stderr, gettext("Unknown command (%s/%d).\n"),
		    *(argv - ((satype == SADB_SATYPE_UNSPEC) ? 1 : 2)), cmd);
		usage();
	}
}

/*
 * Enter a mode where commands are read from a file.  Treat stdin special.
 */
void
do_interactive(FILE *infile)
{
	char ibuf[IBUF_SIZE], holder[IBUF_SIZE];
	char *hptr, *promptstring = "ipseckey> ";
	char **thisargv;
	int thisargc, longjmp_ret;
	boolean_t continue_in_progress = B_FALSE;

	longjmp_ret = setjmp(env);
	/* LINTED */
	if (longjmp_ret != 0) {
		/* XXX Reset any state in here. */
	}

	interactive = B_TRUE;
	bzero(ibuf, IBUF_SIZE);

	if (infile == stdin) {
		printf(promptstring);
		fflush(stdout);
	} else {
		readfile = B_TRUE;
	}

	while (fgets(ibuf, IBUF_SIZE, infile) != NULL) {
		if (readfile)
			lineno++;
		thisargc = 0;
		thisargv = NULL;

		/*
		 * Check byte IBUF_SIZE - 2, because byte IBUF_SIZE - 1 will
		 * be null-terminated because of fgets().
		 */
		if (ibuf[IBUF_SIZE - 2] != '\0') {
			fprintf(stderr, gettext("Line %d too big.\n"), lineno);
			exit(1);
		}

		if (!continue_in_progress) {
			/* Use -2 because of \n from fgets. */
			if (ibuf[strlen(ibuf) - 2] == CONT_CHAR) {
				/*
				 * Can use strcpy here, I've checked the
				 * length already.
				 */
				strcpy(holder, ibuf);
				hptr = &(holder[strlen(holder)]);

				/* Remove the CONT_CHAR from the string. */
				hptr[-2] = ' ';

				continue_in_progress = B_TRUE;
				bzero(ibuf, IBUF_SIZE);
				continue;
			}
		} else {
			/* Handle continuations... */
			(void) strncpy(hptr, ibuf,
			    (size_t)(&(holder[IBUF_SIZE]) - hptr));
			if (holder[IBUF_SIZE - 1] != '\0') {
				fprintf(stderr,
				    gettext("Command buffer overrun.\n"));
				exit(1);
			}
			/* Use - 2 because of \n from fgets. */
			if (hptr[strlen(hptr) - 2] == CONT_CHAR) {
				bzero(ibuf, IBUF_SIZE);
				hptr += strlen(hptr);

				/* Remove the CONT_CHAR from the string. */
				hptr[-2] = ' ';

				continue;
			} else {
				continue_in_progress = B_FALSE;
				/*
				 * I've already checked the length...
				 */
				strcpy(ibuf, holder);
			}
		}

		switch (create_argv(ibuf, &thisargc, &thisargv)) {
		case TOO_MANY_TOKENS:
			fprintf(stderr, gettext("Too many input tokens.\n"));
			exit(1);
			break;
		case MEMORY_ALLOCATION:
			fprintf(stderr, gettext("Memory allocation error.\n"));
			exit(1);
			break;
		case COMMENT_LINE:
			/* Comment line. */
			break;
		default:
			parseit(thisargc, thisargv);
			free(thisargv);
			if (infile == stdin) {
				printf(promptstring);
				fflush(stdout);
			}
			break;
		}
		bzero(ibuf, IBUF_SIZE);
	}
	if (!readfile) {
		putchar('\n');
		fflush(stdout);
	}
	exit(0);
}

int
main(int argc, char *argv[])
{
	int ch;
	FILE *infile = stdin, *savefile;
	boolean_t dosave = B_FALSE, readfile = B_FALSE;
#if 0
	/* See next #if 0 for why. */
	struct passwd *pwd;
#endif

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	openlog("ipseckey", LOG_CONS, LOG_AUTH);
	if (getuid() != 0) {
		fprintf(stderr, "You must be root to run ipseckey.\n");
#if 0
		/*
		 * While a good idea in theory, logging this error can
		 * open up a denial-of-service hole.
		 *
		 * When a better auditing facility is available, this sort
		 * of reporting should be in this if statement.
		 */

		/* I can use MT-unsafe getpwuid() here. */
		pwd = getpwuid(getuid());
		syslog((LOG_NOTICE|LOG_AUTH),
		    gettext("User %s (uid=%d) tried to run ipseckey.\n"),
		    (pwd == NULL) ? "<unknown>" : pwd->pw_name, getuid());
#endif
		exit(1);
	}

	/* umask me to paranoid, I only want to create files read-only */
	(void) umask((mode_t)00377);

	while ((ch = getopt(argc, argv, "pnvf:s:")) != EOF)
		switch (ch) {
		case 'p':
			pflag = B_TRUE;
			break;
		case 'n':
			nflag = B_TRUE;
			break;
		case 'v':
			vflag = B_TRUE;
			break;
		case 'f':
			if (dosave)
				usage();
			infile = fopen(optarg, "r");
			if (infile == NULL)
				bail(optarg);
			readfile = B_TRUE;
			break;
		case 's':
			if (readfile)
				usage();
			dosave = B_TRUE;
			savefile = opensavefile(optarg);
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	mypid = getpid();

	keysock = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);

	if (keysock == -1)
		Bail("Opening PF_KEY socket");

	if (dosave) {
		mask_signals(B_FALSE);	/* Mask signals */
		dodump(SADB_SATYPE_UNSPEC, savefile);
		mask_signals(B_TRUE);	/* Unmask signals */
		exit(0);
	}

	if (infile != stdin || *argv == NULL) {
		/* Go into interactive mode here. */
		do_interactive(infile);
	}

	parseit(argc, argv);

	return (0);
}
