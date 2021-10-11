/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* CSTYLED */
%{
#ident	"@(#)ftpcmd.y	1.31	99/10/21 SMI"	/* SVr4.0 1.6	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                 All rights reserved.
 *
 */


/*
 * Grammar for FTP commands.
 * See RFC 765.
 */


#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/ftp.h>

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <setjmp.h>
#include <syslog.h>

extern	struct sockaddr_storage data_dest;
extern	struct sockaddr_storage rem_addr;
extern	struct sockaddr_in *data_dest_sin;
extern	struct sockaddr_in6 *data_dest_sin6;
extern	int logged_in;
extern	struct passwd *pw;
extern	int guest;
extern	int logging;
extern	int type;
extern	int form;
extern	int debug;
extern	int timeout;
extern  int pdata;
extern	char hostname[];
extern	char real_username[132];
extern	char *globerr;
extern	int usedefault;
extern	int unique;
extern  int transflag;
extern  char tmpline[];
char	**glob();
extern	int addrfmly;
extern	int peerfmly;
static unsigned short cliport = 0;
static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;
char	cbuf[512];
char	*fromname;
int	epsv_all = 0;
int	host_eport_error = 0;
int	host_lport_error = 0;
/* Type values for various passive modes supported by server */
#define	TYPE_PASV	0
#define	TYPE_EPSV	1
#define	TYPE_LPSV	2

%}

%token
	A	B	C	E	F	I
	L	N	P	R	S	T

	SP	CRLF	COMMA	STRING	NUMBER

	USER	PASS	ACCT	REIN	QUIT	PORT	LPRT	LPSV
	EPRT	EPSV	PASV	TYPE	STRU	MODE	RETR	STOR
	APPE	MLFL	MAIL	MSND	MSOM	MSAM
	MRSQ	MRCP	ALLO	REST	RNFR	RNTO
	ABOR	DELE	CWD	LIST	NLST	SITE
	STAT	HELP	NOOP	XMKD	XRMD	XPWD
	XCUP	STOU	SYST

	LEXERR

%start	cmd_list

%%

cmd_list:	/* empty */
	|	cmd_list cmd
		= {
			fromname = NULL;
		}
	|	cmd_list rcmd
	;

cmd:		USER SP username CRLF
		= {
			extern struct passwd *sgetpwnam();

			logged_in = 0;
			real_username[0] = 0;
			guest = 0;
			if (strcmp((char *)$3, "ftp") == 0 ||
			    strcmp((char *)$3, "anonymous") == 0) {
				if ((pw = sgetpwnam("ftp")) != NULL) {
					guest = 1;
					reply(331,
					    "Guest login ok, send ident as"
					    " password.");
				} else {
					reply(530, "User %s unknown.", $3);
					audit_ftpd_no_anon();
				}
			} else {
				strncpy(real_username, (char *)$3,
				    sizeof (real_username));
				real_username[sizeof (real_username) - 1] = 0;
				if (checkuser((char *)$3))
					pw = sgetpwnam((char *)$3);
				reply(331, "Password required for %s.", $3);
			}
			free((char *)$3);
		}
	|	PASS SP password CRLF
		= {
			pass((char *)$3);
			free((char *)$3);
		}
	|	PORT check_login SP host_port CRLF
		= {
			if (epsv_all) {
				reply(501, "PORT not allowed after EPSV ALL");
				goto port_done;
			}
			usedefault = 0;
			/*
			 * XXX RFC959 states that the data connection must be
			 * closed if "the port specification is changed by a
			 * command from the user" (page 19).  Current Internet
			 * practise deviates from this by closing the data
			 * connection even if the same port is specified
			 * again.  The logic behind this is that the client
			 * won't issue a PORT command unless it's expecting a
			 * new connection.  -bson
			 */
			if (pdata > 0) {
				(void) close(pdata);
				pdata = -1;
			}
			if ($2) {
				boolean_t is_unspec;

				if (data_dest.ss_family == AF_INET6) {
					data_dest_sin6 = (struct sockaddr_in6 *)
					    &data_dest;
					is_unspec = IN6_IS_ADDR_UNSPECIFIED(
					    &data_dest_sin6->sin6_addr);
				} else if (data_dest.ss_family == AF_INET) {
					data_dest_sin = (struct sockaddr_in *)
					    &data_dest;
					is_unspec =
					    (data_dest_sin->sin_addr.s_addr
					    == INADDR_ANY);
				}
				if ((cliport >= IPPORT_RESERVED) &&
				    !is_unspec) {
					reply(200, "PORT command "
						"successful.");
				} else {
					reply(500, "PORT argument must be "
					    "%u or greater.",
					    IPPORT_RESERVED);
				}
			}
			port_done:;
		}
	|	EPRT SP host_eport CRLF
		{
			in_port_t port;

			if (epsv_all && !host_eport_error) {
				reply(501, "EPRT not allowed after EPSV ALL");
				goto eprt_done;
			}
			if (data_dest.ss_family == AF_INET6) {
				data_dest_sin6 = (struct sockaddr_in6 *)
				    &data_dest;
				port = data_dest_sin6->sin6_port;
			} else if (data_dest.ss_family == AF_INET) {
				data_dest_sin = (struct sockaddr_in *)
				    &data_dest;
				port = data_dest_sin->sin_port;
			}
			if ($2 && !host_eport_error) {
				if ((ntohs(port)
				    >= IPPORT_RESERVED)) {
					usedefault = 0;
					if (pdata > 0) {
						(void) close(pdata);
						pdata = -1;
					}
					reply(200, "EPRT command successful.");
				} else {
					usedefault = 1;
					reply(500, "Illegal EPRT "
					    "range rejected.");
				}
			}
			eprt_done:;
		}
	|	LPRT SP host_lport CRLF
		{
			if (epsv_all && !host_lport_error) {
				reply(501, "LPRT not allowed after EPSV ALL");
				goto lprt_done;
			}
			if (!host_lport_error) {
				usedefault = 0;
				if (pdata > 0) {
					(void) close(pdata);
					pdata = -1;
				}
				reply(200, "LPRT command successful.");
			}
			lprt_done:;
		}
	|	EPSV check_login CRLF
		= {
			passive(TYPE_EPSV, 0);
		}
	|	EPSV check_login SP STRING CRLF
		= {
			if (strcasecmp((const char *)$4, "ALL") == 0) {
				reply(200, "EPSV ALL command successful.");
				epsv_all = 1;
			} else {
				char *chptr;
				int af;

				af = strtoul((char *)$4, &chptr, 0);
				if (*chptr) {
					reply(501, "'EPSV %s':"
					    "command not understood.", $4);
				} else {
					if ((af == 1) || (af == 2)) {
						passive(TYPE_EPSV, af);
					} else {
						reply(522,
						    "Protocol not supported, "
						    "use (1,2)");
					}
				}
			}
		}
	|	LPSV check_login CRLF
		= {
			if (epsv_all)
				reply(501, "LPSV not allowed after EPSV ALL");
			else
				passive(TYPE_LPSV, 0);
		}

	|	PASV check_login CRLF
		= {
			if (epsv_all)
				reply(501, "PASV not allowed after EPSV ALL");
			else
				passive(TYPE_PASV, 0);
		}
	|	TYPE SP type_code CRLF
		= {
			switch (cmd_type) {

			case TYPE_A:
				if (cmd_form == FORM_N) {
					reply(200, "Type set to A.");
					type = cmd_type;
					form = cmd_form;
				} else
					reply(504, "Form must be N.");
				break;

			case TYPE_E:
				reply(504, "Type E not implemented.");
				break;

			case TYPE_I:
				reply(200, "Type set to I.");
				type = cmd_type;
				break;

			case TYPE_L:
				if (cmd_bytesz == 8) {
					reply(200,
					    "Type set to L (byte size 8).");
					type = cmd_type;
				} else
					reply(504, "Byte size must be 8.");
			}
		}
	|	STRU SP struct_code CRLF
		= {
			switch ($3) {

			case STRU_F:
				reply(200, "STRU F ok.");
				break;

			default:
				reply(504, "Unimplemented STRU type.");
			}
		}
	|	MODE SP mode_code CRLF
		= {
			switch ($3) {

			case MODE_S:
				reply(200, "MODE S ok.");
				break;

			default:
				reply(502, "Unimplemented MODE type.");
			}
		}
	|	ALLO SP NUMBER CRLF
		= {
			reply(202, "ALLO command ignored.");
		}
	|	RETR check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				retrieve((char *)NULL, (char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	STOR check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				store((char *)$4, "w");
			if ($4 != NULL)
				free((char *)$4);
		}
	|	APPE check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				store((char *)$4, "a");
			if ($4 != NULL)
				free((char *)$4);
		}
	|	NLST check_login CRLF
		= {
			if ($2)
				retrieve("/bin/ls", "");
		}
	|	NLST check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				retrieve("/bin/ls %s", (char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	LIST check_login CRLF
		= {
			if ($2)
				retrieve("/bin/ls -la", "");
		}
	|	LIST check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				retrieve("/bin/ls -la %s", (char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	DELE check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				delete((char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	RNTO SP pathname CRLF
		= {
			if (fromname) {
				renamecmd(fromname, (char *)$3);
				free(fromname);
				fromname = NULL;
			} else {
				reply(503, "Bad sequence of commands.");
			}
			free((char *)$3);
		}
	|	ABOR CRLF
		= {
			reply(225, "ABOR command successful.");
		}
	|	CWD check_login CRLF
		= {
			if ($2)
				cwd(pw->pw_dir);
		}
	|	CWD check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				cwd((char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	HELP CRLF
		= {
			help((char *)NULL);
		}
	|	HELP SP STRING CRLF
		= {
			help((char *)$3);
		}
	|	NOOP CRLF
		= {
			reply(200, "NOOP command successful.");
		}
	|	XMKD check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				makedir((char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	XRMD check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL)
				removedir((char *)$4);
			if ($4 != NULL)
				free((char *)$4);
		}
	|	XPWD check_login CRLF
		= {
			if ($2)
				pwd();
		}
	|	XCUP check_login CRLF
		= {
			if ($2)
				cwd("..");
		}
	|	STOU check_login SP pathname CRLF
		= {
			if ($2 && $4 != NULL) {
				unique++;
				store((char *)$4, "w");
				unique = 0;
			}
			if ($4 != NULL)
				free((char *)$4);
		}
	|	SYST CRLF
		= {
			reply(215, "UNIX Type: L8 Version: SUNOS");

		}
	|	QUIT CRLF
		= {
			reply(221, "Goodbye.");
			dologout(0);
		}
	|	error CRLF
		= {
			yyerrok;
		}
	;

rcmd:		RNFR check_login SP pathname CRLF
		= {
			char *renamefrom();

			if ($2 && $4) {
				fromname = renamefrom((char *)$4);
				if (fromname == NULL && $4) {
					free((char *)$4);
				}
			}
		}
	;

username:	STRING
	;

password:	STRING
	;

byte_size:	NUMBER
	;

host_port:  NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER
	= {
		char *a, *p;

		if (rem_addr.ss_family == AF_INET6) {
			data_dest_sin6 = (struct sockaddr_in6 *)&data_dest;
			data_dest_sin6->sin6_family = AF_INET6;

			a = (char *)&data_dest_sin6->sin6_addr;
			memset(a, 0, 10);
			a[11] = a[10] = 0xff;
			a[12] = $1; a[13] = $3; a[14] = $5; a[15] = $7;

			p = (char *)&data_dest_sin6->sin6_port;
		} else if (rem_addr.ss_family == AF_INET) {
			data_dest_sin = (struct sockaddr_in *)&data_dest;
			data_dest_sin->sin_family = AF_INET;

			a = (char *)&data_dest_sin->sin_addr;
			a[0] = $1; a[1] = $3; a[2] = $5; a[3] = $7;

			p = (char *)&data_dest_sin->sin_port;
		}

		/*  Only allow client ports in "user space" */
		p[0] = 0; p[1] = 0;
		if ($9 > 255 || $9 < 0 || $11 > 255 || $11 < 0) {
			syslog(LOG_ERR,
			    "attempt to spoof port number %d,%d",
			    $9, $11);
		} else {
			cliport = ($9 << 8) + $11;
			if (cliport >= IPPORT_RESERVED) {
				p[0] = $9; p[1] = $11;
			}
		}
	}
	;

host_eport:	STRING
	= {
		char d, fmt[32], name[INET6_ADDRSTRLEN + 1];
		int proto;
		in_port_t port;

		host_eport_error = 0;
		d = *((char *)$1);

		if ((d < 33) || (d > 126)) {
			reply(500, "Bad delimiter '%c' (%d).", d, d);
			host_eport_error = 1;
			goto ebad;
		}
		(void) sprintf(fmt, "%1$c%%d%1$c%%%2$d[^%1$c]%1$c%%hu%1$c", d,
		    INET6_ADDRSTRLEN);
		if (sscanf((const char *)$1, fmt, &proto, name, &port) != 3) {
			reply(500, "EPRT bad format.");
			host_eport_error = 1;
			goto ebad;
		}
		if ((proto != 1) && (proto != 2)) {
			reply(522, "Protocol not supported, use (1,2)");
			host_eport_error = 1;
			goto ebad;
		}

		if (peerfmly == AF_INET) {
			char *a;

			if (proto != 1) {
				reply(501, "%s", "Address family invalid "
				    "for your control connection");
				host_eport_error = 1;
				goto ebad;
			}

			data_dest_sin = (struct sockaddr_in *)
			    &data_dest;
			a = (char *)&data_dest_sin->sin_addr;
			if (inet_pton(AF_INET, name, &a[0]) <= 0) {
				reply(500, "Bad address %s.", name);
				host_eport_error = 1;
				goto ebad;
			}
			data_dest_sin->sin_family = AF_INET;
			data_dest_sin->sin_port = htons(port);
		} else {
			data_dest_sin6 = (struct sockaddr_in6 *)
			    &data_dest;
			if (proto == 1) {
				char *a;

				a = (char *)&data_dest_sin6->sin6_addr;
				memset(a, 0, 10);
				a[11] = a[10] = 0xff;
				if (inet_pton(AF_INET, name, &a[12]) <= 0) {
					reply(500, "Bad address %s.", name);
					host_eport_error = 1;
					goto ebad;
				}
			} else {
				if (inet_pton(AF_INET6, name,
				    &data_dest_sin6->sin6_addr) <= 0) {
					reply(500, "Bad address %s.", name);
					host_eport_error = 1;
					goto ebad;
				}

			}
			data_dest_sin6->sin6_family = AF_INET6;
			data_dest_sin6->sin6_port = htons(port);
		}
		ebad:
			;
		}
	;

host_lport
	: NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
	NUMBER COMMA NUMBER COMMA NUMBER
		{
			char *a, *p;

			host_lport_error = 0;
			if ($1 != 6) {
				reply(521, "Supported address family is (6)");
				host_lport_error = 1;
				goto bad;
			}
			if (($3 != sizeof (struct in6_addr)) || ($37 != 2)) {
				reply(500, "Bad length.");
				host_lport_error = 1;
				goto bad;
			}
			if (peerfmly != AF_INET6) {
				reply(501, "%s", "Address family invalid for "
				    "your control connection");
				host_lport_error = 1;
				goto bad;
			}
			data_dest_sin6 = (struct sockaddr_in6 *)
			    &data_dest;
			data_dest_sin6->sin6_family = AF_INET6;
			a = (char *)&data_dest_sin6->sin6_addr;
			a[0] = $5; a[1] = $7; a[2] = $9; a[3] = $11;
			a[4] = $13; a[5] = $15; a[6] = $17; a[7] = $19;
			a[8] = $21; a[9] = $23; a[10] = $25;
			a[11] = $27; a[12] = $29; a[13] = $31;
			a[14] = $33; a[15] = $35;

			p = (char *)&data_dest_sin6->sin6_port;
			/*  Only allow client ports in "user space" */
			p[0] = 0; p[1] = 0;
			if ($39 > 255 || $39 < 0 || $41 > 255 || $41 < 0) {
				syslog(LOG_ERR, "attempt to spoof port "
				    "number %d,%d", $39, $41);
			} else {
				cliport = ($39 << 8) + $41;
				if (cliport >= IPPORT_RESERVED) {
					p[0] = $39; p[1] = $41;
				}
			}
		bad:
			;
		}
	;
form_code:	N
	= {
		$$ = FORM_N;
	}
	|	T
	= {
		$$ = FORM_T;
	}
	|	C
	= {
		$$ = FORM_C;
	}
	;

type_code:	A
	= {
		cmd_type = TYPE_A;
		cmd_form = FORM_N;
	}
	|	A SP form_code
	= {
		cmd_type = TYPE_A;
		cmd_form = $3;
	}
	|	E
	= {
		cmd_type = TYPE_E;
		cmd_form = FORM_N;
	}
	|	E SP form_code
	= {
		cmd_type = TYPE_E;
		cmd_form = $3;
	}
	|	I
	= {
		cmd_type = TYPE_I;
	}
	|	L
	= {
		cmd_type = TYPE_L;
		cmd_bytesz = 8;
	}
	|	L SP byte_size
	= {
		cmd_type = TYPE_L;
		cmd_bytesz = $3;
	}
	/* this is for a bug in the BBN ftp */
	|	L byte_size
	= {
		cmd_type = TYPE_L;
		cmd_bytesz = $2;
	}
	;

struct_code:	F
	= {
		$$ = STRU_F;
	}
	|	R
	= {
		$$ = STRU_R;
	}
	|	P
	= {
		$$ = STRU_P;
	}
	;

mode_code:	S
	= {
		$$ = MODE_S;
	}
	|	B
	= {
		$$ = MODE_B;
	}
	|	C
	= {
		$$ = MODE_C;
	}
	;

pathname:	pathstring
	= {
		/*
		 * Problem: this production is used for all pathname
		 * processing, but only gives a 550 error reply.
		 * This is a valid reply in some cases but not in others.
		 */
		if ($1 && strncmp((char *)$1, "~", 1) == 0) {
			$$ = (int)*glob((char *)$1);
			if (globerr != NULL) {
				reply(550, globerr);
				$$ = NULL;
			}
			free((char *)$1);
		} else
			$$ = $1;
	}
	;

pathstring:	STRING
	;

check_login:	/* empty */
	= {
		if (logged_in)
			$$ = 1;
		else {
			reply(530, "Please login with USER and PASS.");
			$$ = 0;
		}
	}
	;

%%

extern sigjmp_buf errcatch;

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional STRING */

struct tab {
	char	*name;
	short	token;
	short	state;
	short	implemented;	/* 1 if command is implemented */
	char	*help;
};

struct tab cmdtab[] = {		/* In order defined in RFC 765 */
	{ "USER", USER, STR1, 1,	"<sp> username" },
	{ "PASS", PASS, STR1, 1,	"<sp> password" },
	{ "ACCT", ACCT, STR1, 0,	"(specify account)" },
	{ "REIN", REIN, ARGS, 0,	"(reinitialize server state)" },
	{ "QUIT", QUIT, ARGS, 1,	"(terminate service)", },
	{ "PORT", PORT, ARGS, 1,	"<sp> b0, b1, b2, b3, b4" },
	{ "EPRT", EPRT, STR1, 1,	"<sp> |proto|addr|port|", },
	{ "LPRT", LPRT, ARGS, 1,	"<sp> af, hal, h1, h2, ..." },
	{ "EPSV", EPSV, OSTR, 1,      "(set server in extended passive mode)" },
	{ "LPSV", LPSV, ARGS, 1,	"(set server in IPv6 passive mode)" },
	{ "PASV", PASV, ARGS, 1,	"(set server in passive mode)" },
	{ "TYPE", TYPE, ARGS, 1,	"<sp> [ A | E | I | L ]" },
	{ "STRU", STRU, ARGS, 1,	"(specify file structure)" },
	{ "MODE", MODE, ARGS, 1,	"(specify transfer mode)" },
	{ "RETR", RETR, STR1, 1,	"<sp> file-name" },
	{ "STOR", STOR, STR1, 1,	"<sp> file-name" },
	{ "APPE", APPE, STR1, 1,	"<sp> file-name" },
	{ "MLFL", MLFL, OSTR, 0,	"(mail file)" },
	{ "MAIL", MAIL, OSTR, 0,	"(mail to user)" },
	{ "MSND", MSND, OSTR, 0,	"(mail send to terminal)" },
	{ "MSOM", MSOM, OSTR, 0,	"(mail send to terminal or mailbox)" },
	{ "MSAM", MSAM, OSTR, 0,	"(mail send to terminal and mailbox)" },
	{ "MRSQ", MRSQ, OSTR, 0,	"(mail recipient scheme question)" },
	{ "MRCP", MRCP, STR1, 0,	"(mail recipient)" },
	{ "ALLO", ALLO, ARGS, 1,	"allocate storage (vacuously)" },
	{ "REST", REST, STR1, 0,	"(restart command)" },
	{ "RNFR", RNFR, STR1, 1,	"<sp> file-name" },
	{ "RNTO", RNTO, STR1, 1,	"<sp> file-name" },
	{ "ABOR", ABOR, ARGS, 1,	"(abort operation)" },
	{ "DELE", DELE, STR1, 1,	"<sp> file-name" },
	{ "CWD",  CWD,  OSTR, 1,	"[ <sp> directory-name]" },
	{ "XCWD", CWD,	OSTR, 1,	"[ <sp> directory-name ]" },
	{ "LIST", LIST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "NLST", NLST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "SITE", SITE, STR1, 0,	"(get site parameters)" },
	{ "SYST", SYST, ARGS, 1,	"(get type of operating system)"},
	{ "STAT", STAT, OSTR, 0,	"(get server status)" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
	{ "NOOP", NOOP, ARGS, 1,	"" },
	{ "MKD",  XMKD, STR1, 1,	"<sp> path-name" },
	{ "XMKD", XMKD, STR1, 1,	"<sp> path-name" },
	{ "RMD",  XRMD, STR1, 1,	"<sp> path-name" },
	{ "XRMD", XRMD, STR1, 1,	"<sp> path-name" },
	{ "PWD",  XPWD, ARGS, 1,	"(return current directory)" },
	{ "XPWD", XPWD, ARGS, 1,	"(return current directory)" },
	{ "CDUP", XCUP, ARGS, 1,	"(change to parent directory)" },
	{ "XCUP", XCUP, ARGS, 1,	"(change to parent directory)" },
	{ "STOU", STOU, STR1, 1,	"<sp> file-name" },
	{ NULL,   0,    0,    0,	0 }
};

struct tab *
lookup(cmd)
	char *cmd;
{
	register struct tab *p;

	for (p = cmdtab; p->name != NULL; p++)
		if (strcmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * getline - a hacked up version of fgets to ignore TELNET escape codes.
 */
char *
getline(s, n, iop)
	char *s;
	register FILE *iop;
{
	register c;
	register char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	for (c = 0; tmpline[c] != '\0' && --n > 0; ++c) {
		*cs++ = tmpline[c];
		if (tmpline[c] == '\n') {
			*cs++ = '\0';
			if (debug) {
				/*
				 * Don't log the passwd from a non-anonymous
				 * PASS command.
				 */
				if (strncasecmp(s, "PASS", 4) != 0 || guest) {
					syslog(LOG_DEBUG, "command: %s", s);
				} else {
					syslog(LOG_DEBUG,
					    "command: PASS <password>");
				}
			}
			tmpline[0] = '\0';
			return (s);
		}
		if (c == 0) {
			tmpline[0] = '\0';
		}
	}
	while (--n > 0 && (c = getc(iop)) != EOF) {
		c = 0377 & c;
		while (c == IAC) {
			switch (c = 0377 & getc(iop)) {
			case WILL:
			case WONT:
				c = 0377 & getc(iop);
				printf("%c%c%c", IAC, WONT, c);
				(void) fflush(stdout);
				break;
			case DO:
			case DONT:
				c = 0377 & getc(iop);
				printf("%c%c%c", IAC, DONT, c);
				(void) fflush(stdout);
				break;
			default:
				break;
			}
			if ((c = getc(iop)) == EOF)
				break;
			c &= 0377;

		}
		*cs++ = c;
		if (c == '\n')
			break;
	}
	if (c == EOF && cs == s) {
		return (NULL);
	}
	*cs++ = '\0';
	if (debug) {
		/* Don't log passwd from PASS unless anonymous */
		if (strncasecmp(s, "PASS", 4) != 0 || guest)
			syslog(LOG_DEBUG, "command: %s", s);
		else
			syslog(LOG_DEBUG, "command: PASS <passwd>");
	}
	return (s);
}

static int
toolong()
{
	time_t now;
	extern char *ctime();
	extern time_t time();

	reply(421,
	    "Timeout (%d seconds): closing control connection.", timeout);
	(void) time(&now);
	if (logging) {
		syslog(LOG_INFO,
		    "User %s timed out after %d seconds at %s",
		    (pw ? pw -> pw_name : "unknown"), timeout, ctime(&now));
	}
	dologout(1);
}

yylex()
{
	static int cpos, state;
	register char *cp;
	register struct tab *p;
	int n;
	char c;


	for (;;) {

		switch (state) {

		case CMD:

			(void) signal(SIGALRM, (void (*)())toolong);
			(void) alarm((unsigned)timeout);

			if (getline(cbuf, sizeof (cbuf)-1, stdin) == NULL) {
				reply(221, "You could at least say goodbye.");
				dologout(0);
			}
			(void) alarm((unsigned)timeout);
			if ((cp = strchr(cbuf, '\r')) != NULL) {
				*cp++ = '\n';
				*cp = '\0';
			}
			if ((cp = strpbrk(cbuf, " \n")) != NULL)
				cpos = cp - cbuf;
			if (cpos == 0)
				cpos = 4;
			c = cbuf[cpos];
			cbuf[cpos] = '\0';
			upper(cbuf);
			p = lookup(cbuf);
			cbuf[cpos] = c;
			if (p != 0) {
				if (p->implemented == 0) {
					nack(p->name);
					siglongjmp(errcatch, 0);
					/* NOTREACHED */
				}
				state = p->state;
				yylval = (int)p->name;
				return (p->token);
			}
			break;

		case OSTR:

			if (cbuf[cpos] == '\n') {
				state = CMD;
				return (CRLF);
			}
			/* FALL THRU */

		case STR1:

			if (cbuf[cpos] == ' ') {
				cpos++;
				state = STR2;
				return (SP);
			}
			break;

		case STR2:

			cp = &cbuf[cpos];
			n = strlen(cp);
			cpos += n - 1;
			/*
			 * Make sure the string is nonempty and \n terminated.
			 */
			if (n > 1 && cbuf[cpos] == '\n') {
				cbuf[cpos] = '\0';
				yylval = copy(cp);
				cbuf[cpos] = '\n';
				state = ARGS;
				return (STRING);
			}
			break;

		case ARGS:

			if (isdigit(cbuf[cpos])) {
				cp = &cbuf[cpos];
				while (isdigit(cbuf[++cpos]))
					;
				c = cbuf[cpos];
				cbuf[cpos] = '\0';
				yylval = atoi(cp);
				cbuf[cpos] = c;
				return (NUMBER);
			}
			switch (cbuf[cpos++]) {

			case '\n':
				state = CMD;
				return (CRLF);

			case ' ':
				return (SP);

			case ',':
				return (COMMA);

			case 'A':
			case 'a':
				return (A);

			case 'B':
			case 'b':
				return (B);

			case 'C':
			case 'c':
				return (C);

			case 'E':
			case 'e':
				return (E);

			case 'F':
			case 'f':
				return (F);

			case 'I':
			case 'i':
				return (I);

			case 'L':
			case 'l':
				return (L);

			case 'N':
			case 'n':
				return (N);

			case 'P':
			case 'p':
				return (P);

			case 'R':
			case 'r':
				return (R);

			case 'S':
			case 's':
				return (S);

			case 'T':
			case 't':
				return (T);

			}
			break;

		default:
			fatal("Unknown state in scanner.");
		}
		yyerror((char *)NULL);
		state = CMD;
		siglongjmp(errcatch, 0);
	}
}

upper(s)
	char *s;
{
	while (*s != '\0') {
		if (islower(*s))
			*s = toupper(*s);
		s++;
	}
}

copy(s)
	char *s;
{
	char *p;

	p = malloc((unsigned)strlen(s) + 1);
	if (p == NULL)
		fatal("Ran out of memory.");
	(void) strcpy(p, s);
	return ((int)p);
}

help(s)
	char *s;
{
	register struct tab *c;
	register int width, NCMDS;

	width = 0, NCMDS = 0;
	for (c = cmdtab; c->name != NULL; c++) {
		int len = strlen(c->name) + 1;

		if (c->implemented == 0)
			len++;
		if (len > width)
			width = len;
		NCMDS++;
	}
	width = (width + 8) &~ 7;
	if (s == 0) {
		register int i, j, w;
		int columns, lines;

		lreply(214,
		    "The following commands are recognized:");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			printf("   ");
			for (j = 0; j < columns; j++) {
				c = cmdtab + j * lines + i;
				printf("%s%c", c->name,
					c->implemented ? ' ' : '*');
				if (c + lines >= &cmdtab[NCMDS])
					break;
				w = strlen(c->name) + 1;
				while (w < width) {
					putchar(' ');
					w++;
				}
			}
			printf("\r\n");
		}
		reply(214, "(*'s => unimplemented)");
		(void) fflush(stdout);
		return;
	}
	upper(s);
	c = lookup(s);
	if (c == (struct tab *)0) {
		reply(502, "Unknown command %s.", s);
		return;
	}
	if (c->implemented)
		reply(214, "Syntax: %s %s", c->name, c->help);
	else
		reply(214, "%-*s\t%s; unimplemented.", width, c->name, c->help);
}
