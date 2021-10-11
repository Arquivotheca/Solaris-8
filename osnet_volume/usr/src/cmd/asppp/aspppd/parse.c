#ident	"@(#)parse.c	1.31	96/11/22 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <limits.h>
#include <netdb.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "aspppd.h"
#include "iflist.h"
#include "log.h"
#include "parse.h"
#include "path.h"

#define	BUF_SIZE		1024
#define	CONFIG_FILE		"/etc/asppp.cf"
#define	DIGITS			"0123456789"
#define	HEX_DIGITS		DIGITS "ABCDEFXabcdefx"
#define	IP_DIGITS		HEX_DIGITS "."
#define	MAX_TOKEN_LENGTH	255
#define	OCTAL_DIGITS		"01234567"
#define	WHITE_SPACE		" \t\v\n\f"

#define	push_back(x)		pushed_back = B_TRUE

static void			check_path(struct path *);
static void			do_chap_name(void);
static void			do_chap_secret(void);
static void			do_chap_peer_name(void);
static void			do_chap_peer_secret(void);
static void			do_debug_level(void);
static void			do_defaults(void);
static void			do_default_route(void);
static void			do_ifconfig(void);
static void			do_ignore_line(void);
static void			do_inactivity_timeout(void);
static void			do_interface(void);
static void			do_ipcp_async_map(void);
static void			do_ipcp_compression(void);
static void			do_lcp_compression(void);
static void			do_lcp_mru(void);
static void			do_negotiate_address(void);
static void			do_pap_id(void);
static void			do_pap_password(void);
static void			do_pap_peer_id(void);
static void			do_pap_peer_password(void);
static void			do_path(void);
static void			do_peer_ip_address(void);
static void			do_peer_system_name(void);
static void			do_require_authentication(void);
static void			do_version(void);
static void			do_will_do_authentication(void);
static void			error(char *, ...);
static struct sockaddr_in	*get_ip_address(void);
static char			*get_token(void);
static char			hex_char(char *, int *);
static void			initialize_defaults(void);
static char			octal_char(char *, int *);
static void			replace_escaped_chars(char *, char *, int);

struct symbol_action {
	char	*symbol;
	void	(*action)();
};

static struct symbol_action sa [] = {
	{ "chap_peer_name", do_chap_peer_name },
	{ "chap_peer_secret", do_chap_peer_secret },
	{ "chap_name", do_chap_name },
	{ "chap_secret", do_chap_secret },
	{ "debug_level", do_debug_level},
	{ "defaults", do_defaults },
	{ "default_route", do_default_route },
	{ "ifconfig", do_ifconfig },
	{ "inactivity_timeout", do_inactivity_timeout },
	{ "interface", do_interface },
	{ "ipcp_async_map", do_ipcp_async_map },
	{ "ipcp_compression", do_ipcp_compression },
	{ "lcp_compression", do_lcp_compression },
	{ "lcp_mru", do_lcp_mru },
	{ "negotiate_address", do_negotiate_address },
	{ "pap_id", do_pap_id },
	{ "pap_password", do_pap_password },
	{ "pap_peer_id", do_pap_peer_id },
	{ "pap_peer_password", do_pap_peer_password },
	{ "path", do_path },
	{ "peer_ip_address", do_peer_ip_address },
	{ "peer_system_name", do_peer_system_name },
	{ "require_authentication", do_require_authentication },
	{ "version", do_version },
	{ "will_do_authentication", do_will_do_authentication },
};

static struct path	*cp;
static struct path	defaults;
static jmp_buf		env;
static int		errcnt;
static struct hostent	*host;
static boolean_t	pushed_back;
static int		tokencnt;

void
parse_config_file()
{
	char		*token;
	int		i;

	cp = NULL;
	host = NULL;
	tokencnt = 0;
	pushed_back = B_FALSE;

	initialize_defaults();

	if (setjmp(env) == 0) {
		for (;;) {
			token = get_token();
			for (i = 0;
			    i < (sizeof (sa) / sizeof (struct symbol_action));
			    ++i)
				if (strcasecmp(token, sa[i].symbol) == 0)
					break;

			if (i == (sizeof (sa) / sizeof (struct symbol_action)))
				error("unrecognized symbol %s\n", token);
			else
				(*sa[i].action)();
		}
	}

	if (cp) {
		check_path(cp);
		if (errcnt == 0)
		    add_path(cp);
	}

	if (host && endhostent() < 0)
		fail("parse_config_file: endhostent failed\n");

	if (paths == NULL)
		error("no paths defined in %s\n", CONFIG_FILE);

	if (errcnt)
		fail("parse_config_file: Errors in configuration "
			"file %s\n", CONFIG_FILE);
	else
		log(1, "parse_config_file: Successful configuration\n");
}

static void
check_path(struct path *p)
{
	/*
	 * Place any keyword dependency/consistency checks in
	 * this function.
	 */

	switch (p->auth.will_do) {
	case chap:
	case all:
		if (p->auth.chap.secret == NULL)
		    error("Must specify chap_secret\n");
		if (p->auth.chap.name == NULL)
		    error("Must specify chap_name\n");
	}

	switch (p->auth.required) {
	case chap:
	case all:
		if (p->auth.chap_peer.secret == NULL)
		    error("Must specify chap_peer_secret\n");
		if (p->auth.chap_peer.name == NULL)
		    error("Must specify chap_peer_name\n");
	}
}

static void
do_chap_name(void)
{
	/*
	 * chap_name octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if (strcmp(token+(n-2), "\\0") == 0)
			    error("chap_name \"%s\" ends in NUL\n", token);
			else if (strcmp(token+(n-4), "\\r\\n") == 0)
			    error("chap_name \"%s\" ends in CR/LF\n", token);
			else {
				if ((cp->auth.chap.name = (char *)malloc(n+1))
				    == NULL)
				    fail("do_chap_name: malloc failed\n");
				replace_escaped_chars(token,
							cp->auth.chap.name, n);
			}
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_chap_secret(void)
{
	/*
	 * chap_secret octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.chap.secret = (char *)malloc(n+1))
			    == NULL)
			    fail("do_chap_secret: malloc failed\n");
			replace_escaped_chars(token, cp->auth.chap.secret, n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_chap_peer_name(void)
{
	/*
	 * chap_peer_name octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if (strcmp(token+(n-2), "\\0") == 0)
			    error("chap_peer_name \"%s\" ends in NUL\n",
				token);
			else if (strcmp(token+(n-4), "\\r\\n") == 0)
			    error("chap_peer_name \"%s\" ends in CR/LF\n",
				token);
			else {
				if ((cp->auth.chap_peer.name =
					(char *)malloc(n+1)) == NULL)
				    fail("do_chap_peer_name: malloc failed\n");
				replace_escaped_chars(token,
						cp->auth.chap_peer.name, n);
			}
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_chap_peer_secret(void)
{
	/*
	 * chap_peer_secret octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.chap_peer.secret =
				(char *)malloc(n+1)) == NULL)
			    fail("do_chap_peer_secret: malloc failed\n");
			replace_escaped_chars(token, cp->auth.chap_peer.secret,
						n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_debug_level(void)
{
	/*
	 *  debug_level <number>
	 */

	int	level;
	char	*token;

	token = get_token();
	if (strspn(token, DIGITS) == strlen(token)) {
		if ((level = atoi(token)) >= 0 && level < 10)
			debug = level;
		else
			error("debug_level %d outside range (0-9)\n", level);
	} else
		error("debug_level value %s not recognized\n", token);
}

static void
do_defaults(void)
{
	/*
	 * defaults
	 */

	if (cp != &defaults) {
		if (cp) {
			check_path(cp);
			if (errcnt == 0)
				add_path(cp);
			else
				free_path(cp);
		}
		cp = &defaults;
	}
}

static void
do_default_route(void)
{
	/*
	 *  default_route
	 */

	if (cp)
		cp->default_route = B_TRUE;
	else
		error("no path defined\n");
}

static void
do_ifconfig(void)
{
	/*
	 * ifconfig <interface> <ignored parameters>
	 */

	int		n;
	char		*token;
	char		*number;
	boolean_t	errors = B_FALSE;

	if (strncasecmp("ipdptp", token = get_token(), 6) == 0) {
		number = token + 6;
	} else if (strncasecmp("ipd", token, 3) == 0) {
		number = token + 3;
	} else {
		errors = B_TRUE;
	}

	(void) strtok(NULL, "\n");

	if (errors || strspn(number, DIGITS) != (n = strlen(number))) {
		error("invalid interface -> %s\n", token);
		return;
	}

	add_interface(token);
}

static void
do_ignore_line(void)
{
	(void) strtok(NULL, "\n");
}

static void
do_inactivity_timeout(void)
{
	/*
	 * inactivity_timeout <number>
	 */

	char *token;

	if (cp) {
		token = get_token();
		if (strspn(token, DIGITS) == strlen(token))
			cp->timeout = atoi(token);
		else
			error("inactivity_timeout %s not recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_interface(void)
{
	/*
	 *  interface ( ipd<id> | ipdptp<id> )
	 *	id ::= <integer> | * | ?
	 */

	int	n;
	char	*token;

	if (cp) {
		if (strncasecmp("ipdptp", token = get_token(), 6) == 0) {
			cp->inf.iftype = IPD_PTP;
			token += 6;
		} else if (strncasecmp("ipd", token, 3) == 0) {
			cp->inf.iftype = IPD_MTP;
			token += 3;
		} else {
			error("ipd or ipdptp keyword expected\n");
			return;
		}

		if (strspn(token, DIGITS) == (n = strlen(token))) {
			cp->inf.ifunit = atoi(token);
			cp->inf.wild_card = B_FALSE;
		} else if (n == 1 && (*token == '*' || *token == '?')) {
			if (cp->inf.iftype == IPD_PTP)
				cp->inf.wild_card = B_TRUE;
			else
				error("wildcards (* | ?) not legal for ipd\n");
		} else
			error("interface number %s not recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_ipcp_async_map(void)
{
	/*
	 * ipcp_async_map <hex number>
	 */

	u_long	hex;
	char	*token;

	if (cp) {
		token = get_token();
		if (strspn(token, HEX_DIGITS) == strlen(token)) {
			if ((hex = strtoul(token, (char **)NULL, 16))
			    == ULONG_MAX)
				error("Numeric conversion overflow "
				    "on %s\n", token);
			else
				cp->ipcp.async_map = hex;
		} else
			error("ipcp_async_map value %s not "
			    "recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_ipcp_compression(void)
{
	/*
	 * ipcp_compression ( vj | off )
	 */

	char *token;

	if (cp) {
		if (strcmp(token = get_token(), "vj") == 0)
			cp->ipcp.compression = vj;
		else if (strcmp(token, "off") == 0)
			cp->ipcp.compression = off;
		else
			error("ipcp_compression value %s not "
			    "recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_lcp_compression(void)
{
	/*
	 * lcp_compression ( on | off )
	 */

	char *token;

	if (cp) {
		if (strcmp(token = get_token(), "on") == 0)
			cp->lcp.compression = on;
		else if (strcmp(token, "off") == 0)
			cp->lcp.compression = off;
		else
			error("lcp_compression value %s not "
			    "recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_lcp_mru(void)
{
	/*
	 * lcp_mru <number>
	 */

	char *token;

	if (cp) {
		token = get_token();
		if (strspn(token, DIGITS) == strlen(token))
			cp->lcp.mru = atoi(token);
		else
			error("lcp_mru value %s not recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_negotiate_address(void)
{
	/*
	 * negotiate_address ( on | off )
	 */

	char *token;

	if (cp) {
		if (strcmp(token = get_token(), "on") == 0)
		    cp->inf.get_ipaddr = B_TRUE;
		else if (strcmp(token, "off") == 0)
		    cp->inf.get_ipaddr = B_FALSE;
		else
			error("negotiate_address value %s not "
			    "recognized\n", token);
	} else
		error("no path defined\n");
}

static void
do_pap_id(void)
{
	/*
	 * pap_id octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.pap.id = (char *)malloc(n+1))
			    == NULL)
			    fail("do_pap_id: malloc failed\n");
			replace_escaped_chars(token, cp->auth.pap.id, n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_pap_password(void)
{
	/*
	 * pap_password octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.pap.password = (char *)malloc(n+1))
			    == NULL)
			    fail("do_pap_password: malloc failed\n");
			replace_escaped_chars(token, cp->auth.pap.password,
						n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_pap_peer_id(void)
{
	/*
	 * pap_peer_id octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.pap_peer.id = (char *)malloc(n+1))
			    == NULL)
			    fail("do_pap_peer_id: malloc failed\n");
			replace_escaped_chars(token, cp->auth.pap_peer.id, n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_pap_peer_password(void)
{
	/*
	 * pap_peer_password octet+
	 */

	int  n;
	char *token;

	if (cp) {
		token = get_token();
		if ((n = strlen(token)) <= MAX_TOKEN_LENGTH) {
			if ((cp->auth.pap_peer.password =
				(char *)malloc(n+1)) == NULL)
			    fail("do_pap_peer_password: malloc failed\n");
			replace_escaped_chars(token,
						cp->auth.pap_peer.password, n);
		} else
		    error("token greater than %d characters: %s\n",
			MAX_TOKEN_LENGTH, token);
	} else
	    error("no path defined\n");
}

static void
do_path(void)
{
	/*
	 * path
	 */

	if (cp == &defaults)
		cp = NULL;
	else if (cp) {
		check_path(cp);
		if (errcnt == 0) {
			add_path(cp);
			cp = NULL;
		}
	}

	if (!cp)
		if ((cp = (struct path *)calloc(1, sizeof (*cp))) == NULL)
			fail("do_path: calloc failed\n");

	*cp = defaults;
}

static void
do_peer_ip_address(void)
{
	/*
	 * peer_ip_address  <ip address>
	 */

	struct sockaddr_in	*sin;

	if (cp) {
		sin = get_ip_address();
		(void) memcpy(&cp->inf.sa, sin, sizeof (*sin));
	} else
		error("no path defined\n");
}

static void
do_peer_system_name(void)
{
	/*
	 * peer_system_name <system name> [ <max # of connections> | <empty> ]
	 */

	char	*token;

	if (cp) {
		token = get_token();
		if ((cp->uucp.system_name = (char *)malloc(strlen(token)+1))
		    == NULL)
		    fail("do_peer_system_name: malloc failed\n");
		(void) strcpy(cp->uucp.system_name, token);
		if (((cp->uucp.max_connections = atoi(get_token())) == 0)) {
			push_back(token);
			cp->uucp.max_connections = 1;
		}
	} else
	    error("no path defined\n");
}

static void
do_require_authentication(void)
{
	/*
	 * require_authentication  off | pap [chap] | chap [pap]
	 */

	char *token;

	if (cp) {
		if (strcmp(token = get_token(), "off") == 0)
		    cp->auth.required = none;
		else if (strcmp(token, "pap") == 0) {
			cp->auth.required = pap;
			if (strcmp(token = get_token(), "chap") == 0)
			    cp->auth.required = all;
			else
			    push_back(token);
		} else if (strcmp(token, "chap") == 0) {
			cp->auth.required = chap;
			if (strcmp(token = get_token(), "pap") == 0)
			    cp->auth.required = all;
			else
			    push_back(token);
		} else {
			error("off, pap, or chap required: %s\n", token);
			push_back(token);
		}
	} else
	    error("no path defined\n");
}

static void
do_version(void)
{
	/*
	 * version <number>
	 */

	char *token;

	if (tokencnt != 1)
		error("version must be first keyword encountered\n");
	else {
		token = get_token();
		if (strcmp(token, "1") != 0)
			error("only version 1 files are acceptable\n");
	}
}

static void
do_will_do_authentication(void)
{
	/*
	 * will_do_authentication  off | pap [chap] | chap [pap]
	 */

	char *token;

	if (cp) {
		if (strcmp(token = get_token(), "off") == 0)
		    cp->auth.will_do = none;
		else if (strcmp(token, "pap") == 0) {
			cp->auth.will_do = pap;
			if (strcmp(token = get_token(), "chap") == 0)
			    cp->auth.will_do = all;
			else
			    push_back(token);
		} else if (strcmp(token, "chap") == 0) {
			cp->auth.will_do = chap;
			if (strcmp(token = get_token(), "pap") == 0)
			    cp->auth.will_do = all;
			else
			    push_back(token);
		} else {
			error("off, pap, or chap required: %s\n", token);
			push_back(token);
		}
	} else
	    error("no path defined\n");
}

static void
error(char *fmt, ...)
{
	va_list	args;
	char	buf[BUF_SIZE];

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);
	log(0, "parse_config_file: %s", buf);
	errcnt++;
}

static struct sockaddr_in
*get_ip_address()
{
	u_long				addr;
	static struct sockaddr_in	sin;
	char				*token;

	token = get_token();
	if (strspn(token, IP_DIGITS) == strlen(token)) {
		if ((int)(addr = inet_addr(token)) == -1) {
			error("malformed ip address: %s\n", token);
			(void) memset(&sin, '\0', sizeof (sin));
		} else {
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = addr;
		}
	} else {
		if ((host = gethostbyname(token)) == NULL) {
			error("host IP address not found: %s\n", token);
			(void) memset(&sin, '\0', sizeof (sin));
		} else {
			sin.sin_family = AF_INET;
			(void) memcpy(&sin.sin_addr.s_addr, *host->h_addr_list,
			    host->h_length);
		}
	}

	return (&sin);
}

static char
*get_token()
{
	static FILE	*cf = NULL;
	static char	*buf;
	static char	*token = NULL;

	if (pushed_back) {
		pushed_back = B_FALSE;
		return (token);
	}

	if (token && (token = strtok(NULL, WHITE_SPACE)) && *token != '#') {
		tokencnt++;
		return (token);
	}

	if (cf == NULL) {
		if ((cf = fopen(CONFIG_FILE, "r")) == NULL)
			fail("get_token: Can't open config file\n");
		buf = (char *)malloc(BUF_SIZE);
		if (buf == NULL)
			fail("get_token: malloc failed\n");
	}

	while (fgets(buf, BUF_SIZE, cf))
		if ((token = strtok(buf, WHITE_SPACE)) && *token != '#') {
			tokencnt++;
			return (token);
		}

	if (ferror(cf))
		fail("get_token: fgets returned error\n");

	if (feof(cf)) {
		if (fclose(cf))
			fail("get_token: fclose failed\n");
		cf = NULL;
		free(buf);
		token = NULL;
		longjmp(env, 1);
	}

	fail("get_token: fgets terminated without error or eof\n");
}

static char
hex_char(char *p, int *n)
{
	char *end;
	u_long hex = 0;

	if (strspn(p, HEX_DIGITS) == 2)
	    hex = strtoul(p, &end, 16);
	else
	    error("Not a hexadecimal value: \\x%s\n", p);

	*n = end - p;
	return ((char)hex);
}

static void
initialize_defaults(void)
{
	defaults.next = NULL;
	defaults.s = -1;
	defaults.cns_id = -1;
	defaults.mux = -1;
	defaults.mux_id = -1;
	defaults.inf.iftype = IPD_NULL;
	defaults.inf.ifunit = (u_int) -1;
	defaults.inf.wild_card = B_FALSE;
	defaults.inf.get_ipaddr = B_FALSE;
	memset((void *)&defaults.inf.sa, 0, sizeof (struct sockaddr));
	defaults.uucp.system_name = NULL;
	defaults.uucp.max_connections = 1;
	defaults.timeout = 120;
	defaults.state = inactive;
	defaults.pid = -1;
	defaults.default_route = B_FALSE;
	defaults.ipcp.async_map = (u_long) 0xFFFFFFFF;
	defaults.ipcp.compression = vj;
	defaults.lcp.compression = on;
	defaults.lcp.mru = 1500;
	memset((void *)&defaults.auth, 0, sizeof (authentication));
}

static char
octal_char(char *p, int *n)
{
	char *end;
	u_long octal = 0;

	if ((int)strspn(p, OCTAL_DIGITS) < 4)
	    octal = strtoul(p, &end, 8);
	else
	    error("Not an octal value: \\%s\n", p);

	*n = end - p;
	return ((char)octal);
}

static void
replace_escaped_chars(char *input, char *output, int size)
{
	int i;
	int n;
	char *p;

	for (i = 0, p = input; *p && i < size; ) {
		if (*p != '\\') {
			*output++ = *p++;
			i++;
		} else {
			switch (++i, *++p) {
			case 'a':
				*output++ = '\007';	/* BEL */
				break;
			case 'b':
				*output++ = '\010';	/* BS */
				break;
			case 't':
				*output++ = '\011';	/* HT */
				break;
			case 'n':
				*output++ = '\012';	/* NL */
				break;
			case 'v':
				*output++ = '\013';	/* VT */
				break;
			case 'f':
				*output++ = '\014';	/* FF */
				break;
			case 'r':
				*output++ = '\015';	/* CR */
				break;
			case 's':
				*output++ = ' ';	/* SP */
				break;
			case 'x':
				p++;
				i++;
				*output++ = hex_char(p, &n);
				p += (n-1);
				i += (n-1);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				*output++ = octal_char(p, &n);
				p += (n-1);
				i += (n-1);
				break;
			default:
				*output++ = *p;
				break;
			}
			p++;
			i++;
		}
	}
	*output = '\0';

	assert((int)strlen(output) <= (int)strlen(input));
}
