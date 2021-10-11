/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)main.c	1.34	99/11/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/systeminfo.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dhcdata.h>
#include "msg.h"
#include <locale.h>

#define	PN_OPT_ADD		1
#define	PN_OPT_CREAT		2
#define	PN_OPT_DELETE		3
#define	PN_OPT_MODIFY		4
#define	PN_OPT_DESTROY		5
#define	PN_OPT_DISPLAY		6
#define	PN_OPT_LIST		7

extern int optind, opterr;
extern char *optarg;
extern char *disp_err(int);
extern void build_dhcp_ipname(char *, struct in_addr *, struct in_addr *);
extern int getnetmaskbyaddr(const struct in_addr, struct in_addr *);

static char *SLtime(const time_t *);
static int exp_date_to_secs(const char *, time_t *);
static int convert_to_ip(const char *, struct in_addr *, char *);
static void convert_ascip_to_name(const char *, char *, int);
static char *v_flags(const char *);

int
main(int argc, char *argv[])
{
	int		major, c, err, flags, count, i;
	int		pflag, aflag, cflag, eflag, fflag, hflag, iflag, mflag,
			nflag, sflag, vflag, yflag;
	ulong_t		tl;
	char		*tp;
	char 		*(*timefp)();
	struct netent	*ne;
	struct hostent	*he;
	time_t		lease = 0L, exptime;
	int		ns = TBL_NS_DFLT;
	int		tbl_err = TBL_SUCCESS;
	Tbl_stat	*tblstatp;
	Tbl		tbl;

	char		*domp, dombuf[MAXPATHLEN];
	char		*pathp, path[MAXPATHLEN];
	char		*namep, namebuf[MAXPATHLEN], pnname[64];

	char		tmpbuf[MAXPATHLEN];
	char		hbuf[MAXHOSTNAMELEN];

	char		*TimeString;
	char		*flagp;
	char		cidbuf[DT_MAX_CID_LEN * 2] = { '0', '0', 0 };
	char		fbuf[64] = { '0', '0', 0 };
	char		cipbuf[64];
	struct in_addr	client;
	char		*clientp;
	char		sipbuf[64];
	struct in_addr	server;
	char		*serverp;
	struct in_addr	tmpip, netaddr, mask;
	char		client_buf[MAXHOSTNAMELEN];
	char		server_buf[MAXHOSTNAMELEN];
	char		expbuf[64] = { '0', 0 };
	char		mbuf[DT_MAX_MACRO_LEN] =
			    { 'U', 'N', 'K', 'N', 'O', 'W', 'N', 0 };
	char		comment[MAXPATHLEN] = { 0 };
	char		*cidp, *fp, *cip, *sip, *exp, *mp, *cp;
	char 		**listp;
	char		*locp;

	domp = pathp = NULL;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif	/* ! TEXT_DOMAIN */

	(void) memset((char *)&tbl, 0, sizeof (tbl));

	major = pflag = aflag = cflag = eflag = fflag = hflag = iflag = mflag =
	    nflag = sflag = vflag = yflag = 0;
	client.s_addr = 0L;
	while ((c = getopt(argc, argv,
	    "LPCRyavA:D:M:r:p:s:i:f:e:h:m:c:n:")) != -1) {
		switch (c) {
		case 'A':
			/* Add an entry */
			major = PN_OPT_ADD;
			if (convert_to_ip(optarg, &client, tmpbuf) != 0) {
				(void) fprintf(stderr, gettext(MSG_INV_CLIENT),
				    optarg, tmpbuf);
				return (DD_CRITICAL);
			}
			break;
		case 'C':
			/* Create an empty per network table */
			major = PN_OPT_CREAT;
			break;
		case 'D':
			/* Delete an entry */
			major = PN_OPT_DELETE;
			if (convert_to_ip(optarg, &client, tmpbuf) != 0) {
				(void) fprintf(stderr, gettext(MSG_INV_CLIENT),
				    optarg, tmpbuf);
				return (DD_CRITICAL);
			}
			break;
		case 'L':
			/* List tables */
			major = PN_OPT_LIST;
			break;
		case 'M':
			/* Modify an entry */
			major = PN_OPT_MODIFY;
			if (convert_to_ip(optarg, &client, tmpbuf) != 0) {
				(void) fprintf(stderr, gettext(MSG_INV_CLIENT),
				    optarg, tmpbuf);
				return (DD_CRITICAL);
			}
			break;
		case 'R':
			/* Remove a dhcp-network table */
			major = PN_OPT_DESTROY;
			break;
		case 'P':
			/* Display a dhcp-network table */
			major = PN_OPT_DISPLAY;
			break;
		case 'r':
			/* Override dhcp resource name */
			if (strcasecmp(optarg, "files") == 0) {
				ns = TBL_NS_UFS;
			} else if (strcasecmp(optarg, "nisplus") == 0) {
				ns = TBL_NS_NISPLUS;
			} else {
				(void) fprintf(stderr, gettext(MSG_INV_RESRC));
				return (DD_CRITICAL);
			}
			break;
		case 'p':
			/* Override dhcp path name */
			pflag = 1;
			if (strlen(optarg) > (MAXPATHLEN - 1)) {
				(void) fprintf(stderr, gettext(MSG_RESRC_2BIG),
				    MAXPATHLEN);
				return (DD_CRITICAL);
			}
			(void) strcpy(path, optarg);
			break;
		case 'v':
			vflag = 1;
			break;
		case 'a':
			aflag = 1;	/* Client id should be converted */
			break;
		case 'c':
			/* Comment */
			cflag = 1;
			if (strlen(optarg) > (MAXPATHLEN - 1)) {
				(void) fprintf(stderr,
				    gettext(MSG_COMMENT_2BIG), MAXPATHLEN - 1);
				return (DD_CRITICAL);
			}
			(void) strcpy(comment, optarg);
			break;
		case 'e':
			/* lease expiration */
			eflag = 1;
			if (exp_date_to_secs(optarg, &lease) == -1) {
				(void) fprintf(stderr, gettext(MSG_INV_LEASE));
				return (DD_CRITICAL);
			}
			(void) sprintf(expbuf, "%ld", lease);
			break;
		case 'f':
			/*
			 * Flags. We accept either a number or the keywords
			 * "ORed" together.
			 */
			fflag = 1;
			if (isdigit(*optarg)) {
				flags = atoi(optarg);
			} else {
				for (flags = 0, tp = strtok(optarg, "+");
				    tp != NULL; tp = strtok(NULL, "+")) {
					if (strncasecmp(tp, PN_DYNAMIC,
					    strlen(PN_DYNAMIC)) == 0) {
						flags |= F_DYNAMIC;
					} else if (strncasecmp(tp, PN_AUTOMATIC,
					    strlen(PN_AUTOMATIC)) == 0) {
						flags |= F_AUTOMATIC;
					} else if (strncasecmp(tp, PN_MANUAL,
					    strlen(PN_MANUAL)) == 0) {
						flags |= F_MANUAL;
					} else if (strncasecmp(tp, PN_UNUSABLE,
					    strlen(PN_UNUSABLE)) == 0) {
						flags |= F_UNUSABLE;
					} else if (strncasecmp(tp, PN_BOOTP,
					    strlen(PN_BOOTP)) == 0) {
						flags |= F_BOOTP_ONLY;
					} else {
						(void) fprintf(stderr,
						    gettext(MSG_INV_KEYWORD),
						    tp);
					}
				}
			}
			(void) sprintf(fbuf, "%02u", flags);
			break;
		case 'h':
			/* client host name */
			hflag = 1;
			if (strlen(optarg) > (MAXHOSTNAMELEN - 1)) {
				(void) fprintf(stderr, gettext(MSG_HOST_2BIG),
				    MAXHOSTNAMELEN);
				return (DD_CRITICAL);
			} else {
				if (isdigit(*optarg)) {
					(void) fprintf(stderr,
					    gettext(MSG_HOST_DIGIT));
					return (DD_CRITICAL);
				}
				(void) strcpy(hbuf, optarg);
			}
			break;
		case 'i':
			/* Client id VALIDATE */
			iflag = 1;
			if (strlen(optarg) > (DT_MAX_CID_LEN - 1)) {
				(void) fprintf(stderr, gettext(MSG_CID_2BIG),
				    (DT_MAX_CID_LEN - 1));
				return (DD_CRITICAL);
			}
			(void) strcpy(cidbuf, optarg);
			break;
		case 'm':
			/* macro name VALIDATE */
			mflag = 1;
			if (strlen(optarg) > DT_MAX_MACRO_LEN) {
				(void) fprintf(stderr, gettext(MSG_MACRO_2BIG),
				    DT_MAX_MACRO_LEN);
				return (DD_CRITICAL);
			}
			(void) strcpy(mbuf, optarg);
			break;
		case 'n':
			/* new client ip address */
			nflag = 1;
			if (!isdigit(*optarg) || (tmpip.s_addr =
			    inet_addr(optarg)) == (ulong_t)0xffffffff) {
				(void) fprintf(stderr,
				    gettext(MSG_INV_NEW_CIP));
				return (DD_CRITICAL);
			}
			(void) sprintf(cipbuf, "%s", inet_ntoa(tmpip));
			break;

		case 's':
			/* server name */
			sflag = 1;
			if (strlen(optarg) > MAXHOSTNAMELEN) {
				(void) fprintf(stderr, gettext(MSG_SIP_2LONG),
				    MAXHOSTNAMELEN);
				return (DD_CRITICAL);
			}
			if (convert_to_ip(optarg, &server, tmpbuf) != 0) {
				(void) fprintf(stderr, gettext(MSG_INV_SERVER),
				    optarg, tmpbuf);
				return (DD_CRITICAL);
			}
			(void) sprintf(sipbuf, "%s", inet_ntoa(server));
			break;
		case 'y':
			yflag = 1;	/* verify macro name */
			break;
		default:
			(void) fprintf(stderr, gettext(MSG_USAGE));
			return (DD_CRITICAL);
		}
	}

	if (major == 0) {
		(void) fprintf(stderr, gettext(MSG_USAGE));
		return (DD_CRITICAL);
	}

	/* Must specify a network for every option but -L, which must not */
	if (argv[optind] == NULL) {
		if (major != PN_OPT_LIST) {
			(void) fprintf(stderr, gettext(MSG_NO_NETWORK));
			return (DD_CRITICAL);
		}
	} else {
		if (major == PN_OPT_LIST) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			return (DD_CRITICAL);
		}
	}

	if (major != PN_OPT_LIST) {
		/* Handle (network ip or name) argument */
		if (strlen(argv[optind]) > MAXHOSTNAMELEN) {
			(void) fprintf(stderr, gettext(MSG_NET_2LONG),
			    MAXHOSTNAMELEN);
			return (DD_CRITICAL);
		}
		if (isdigit(*argv[optind])) {
			/* Number */
			netaddr.s_addr = inet_addr(argv[optind]);
			if (netaddr.s_addr == (ulong_t)0xffffffff) {
				(void) fprintf(stderr, gettext(MSG_INV_NET_IP));
				return (DD_CRITICAL);
			}
		} else {
			/* Name */
			if ((ne = getnetbyname(argv[optind])) != NULL &&
			    ne->n_addrtype == AF_INET) {
				for (i = 0, tl = (ulong_t)0xff000000, count = 0;
				    i < 4; i++, tl >>= 8) {
					if ((ne->n_net & tl) == 0)
						count += 8;
					else
						break;
				}
				netaddr.s_addr = htonl(ne->n_net << count);
			} else {
				(void) fprintf(stderr, gettext(MSG_NO_SUCH_NET),
				    argv[optind]);
				return (DD_CRITICAL);
			}
		}

		err = getnetmaskbyaddr(netaddr, &mask);
		if (err != DD_SUCCESS) {
			/* Use the default netmask. */
			tmpip.s_addr = ntohl(netaddr.s_addr);
			if (IN_CLASSA(tmpip.s_addr)) {
				mask.s_addr = (ulong_t)IN_CLASSA_NET;
			} else if (IN_CLASSB(tmpip.s_addr)) {
				mask.s_addr = (ulong_t)IN_CLASSB_NET;
			} else if (IN_CLASSC(tmpip.s_addr)) {
				mask.s_addr = (ulong_t)IN_CLASSC_NET;
			} else if (IN_CLASSD(tmpip.s_addr)) {
				mask.s_addr = (ulong_t)IN_CLASSD_NET;
			}
			mask.s_addr = htonl(mask.s_addr);
			(void) fprintf(stderr, gettext(MSG_USE_DEF_MASK),
				    inet_ntoa(mask));
			err = DD_WARNING;
		}

		/*
		 * Apply mask to client address. Must be in the correct network
		 */
		if (client.s_addr != 0L && (client.s_addr & mask.s_addr) !=
		    netaddr.s_addr) {
			(void) strcpy(tmpbuf, inet_ntoa(netaddr));
			(void) fprintf(stderr, gettext(MSG_WRONG_NET),
			    inet_ntoa(client), tmpbuf);
			return (DD_CRITICAL);
		}

		build_dhcp_ipname(pnname, &netaddr, &mask);
	}

	if (pflag) {
		if (ns == TBL_NS_DFLT) {
			ns = dd_ns(&tbl_err, &pathp);
			if (ns == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_CANT_DET_RESRC));
				return (DD_CRITICAL);
			}
		}
		if (ns == TBL_NS_UFS) {
			(void) sprintf(namebuf, "%s/%s", path, pnname);
		} else {
			(void) strcpy(namebuf, pnname);
			(void) strcpy(dombuf, path);
			domp = dombuf;
		}
	} else {
		(void) strcpy(namebuf, pnname);
	}

	/*
	 * Further validate/process options
	 */
	if (aflag && !iflag) {
		(void) fprintf(stderr, gettext(MSG_USAGE));
		return (DD_CRITICAL);
	}
	if (iflag) {
		if (aflag) {
			/*
			 * Client id is an ascii string. Convert to hexadecimal
			 * notation.
			 */
			int len = sizeof (tmpbuf);
			if (octet_to_ascii((uchar_t *)cidbuf, strlen(cidbuf),
			    tmpbuf, &len) != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_ERR_CNVT_CID));
				return (DD_CRITICAL);
			}
			if (len > (DT_MAX_CID_LEN - 1)) {
				(void) fprintf(stderr, gettext(MSG_CID_2BIG),
				    (DT_MAX_CID_LEN / 2) - 1);
				return (DD_CRITICAL);
			}
			(void) strcpy(cidbuf, tmpbuf);
		} else {
			/*
			 * Validate the client id. It must consist
			 * of hexadecimal characters, and be an
			 * even number of characters. Lower case (a-f) are
			 * converted to uppercase.
			 */
			if ((strlen(cidbuf) % 2) != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_BAD_CID_ODD));
				return (DD_CRITICAL);
			}
			for (err = 0, i = 0; cidbuf[i] != '\0'; i++) {
				if (!isxdigit(cidbuf[i])) {
					err = 1;
					break;
				}
				cidbuf[i] = toupper(cidbuf[i]);
			}
			if (err != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_BAD_CID_FORMAT));
				return (DD_CRITICAL);
			}
		}
	}
	if (mflag && yflag) {
		/*
		 * User wishes to verify existence of named macro before
		 * processing request.
		 */
		if (pflag) {
			(void) sprintf(tmpbuf, "%s/dhcptab", path);
			namep = tmpbuf;
		} else
			namep = NULL;

		/* Find the existing entry */
		err = list_dd(TBL_DHCPTAB, ns, namep, domp, &tbl_err, &tbl,
		    mbuf, "m");
		free_dd(&tbl);
		if (err == TBL_FAILURE) {
			(void) fprintf(stderr, gettext(MSG_CANT_FIND_MACRO),
			    mbuf, disp_err(tbl_err));
			if (tbl_err == TBL_NO_ENTRY)
				err = DD_ENOENT;
			else
				err = DD_WARNING;
			return (err);
		}
	}

	if (!sflag) {
		/*
		 * Default to host's IP address.
		 */
		(void) sysinfo(SI_HOSTNAME, tmpbuf, MAXHOSTNAMELEN + 1);
		if ((tp = strchr(tmpbuf, '.')) != NULL)
			*tp = '\0';

		if ((he = gethostbyname(tmpbuf)) != NULL &&
		    he->h_addrtype == AF_INET &&
		    he->h_length == sizeof (struct in_addr)) {
			(void) memcpy((char *)&server, he->h_addr_list[0],
			    sizeof (server));
			(void) sprintf(sipbuf, "%s", inet_ntoa(server));
		} else {
			(void) fprintf(stderr, gettext(MSG_CANT_GEN_DEF_SIP));
			return (DD_CRITICAL);
		}
	}

	switch (major) {
	case PN_OPT_ADD:
		if (nflag) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}
		(void) sprintf(tmpbuf, "%s", inet_ntoa(client));
		if (hflag) {
			/*
			 * We give up if an entry for this IP address
			 * already exists in the hosts table. Ditto if
			 * the name is already being used.
			 */
			err = list_dd(TBL_HOSTS, ns, NULL, domp, &tbl_err,
			    &tbl, tmpbuf, NULL);
			free_dd(&tbl);
			if (err == TBL_SUCCESS) {
				(void) fprintf(stderr, gettext(MSG_HOST_EXISTS),
				    tmpbuf);
				err = DD_EEXISTS;
				break;
			}
			err = list_dd(TBL_HOSTS, ns, NULL, domp, &tbl_err,
			    &tbl, NULL, hbuf);
			free_dd(&tbl);
			if (err == TBL_SUCCESS) {
				(void) fprintf(stderr, gettext(MSG_HOST_EXISTS),
				    hbuf);
				err = DD_EEXISTS;
				break;
			}
			err = add_dd_entry(TBL_HOSTS, ns, NULL, domp, &tbl_err,
			    tmpbuf, hbuf, NULL, NULL);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_ADD_HOST_FAIL), tmpbuf, hbuf,
				    disp_err(tbl_err));
				err = DD_WARNING;
				break;
			}
		}
		err = add_dd_entry(TBL_DHCPIP, ns, namebuf, domp, &tbl_err,
		    cidbuf, fbuf, tmpbuf, sipbuf, expbuf, mbuf, comment);
		if (err == TBL_FAILURE) {
			(void) fprintf(stderr, gettext(MSG_ADD_FAILED),
			    tmpbuf, disp_err(tbl_err));
			if (hflag) {
				err = rm_dd_entry(TBL_HOSTS, ns, NULL, domp,
				    &tbl_err, tmpbuf, NULL);
				if (err == TBL_FAILURE) {
					(void) fprintf(stderr,
					    gettext(MSG_HOST_CLEANUP), tmpbuf,
					    disp_err(tbl_err));
				}
			}
			err = DD_WARNING;
		} else
			err = DD_SUCCESS;
		break;
	case PN_OPT_CREAT:
		if ((aflag + cflag + eflag + fflag + hflag + iflag + mflag +
		    nflag + sflag + yflag) != 0) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}
		if (stat_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err,
		    &tblstatp) == TBL_SUCCESS) {
			(void) fprintf(stderr, gettext(MSG_CREATE_EXISTS),
			    namebuf, disp_err(tbl_err));
			free_dd_stat(tblstatp);
			err = DD_EEXISTS;
			break;
		}
		if (make_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err, NULL,
		    NULL) != TBL_SUCCESS) {
			(void) fprintf(stderr, gettext(MSG_CREATE_FAILED),
			    namebuf, disp_err(tbl_err));
			err = DD_WARNING;
		} else
			err = DD_SUCCESS;
		break;
	case PN_OPT_DELETE:
		if ((aflag + cflag + eflag + fflag + hflag + iflag + mflag +
		    nflag + sflag) != 0) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}
		(void) sprintf(tmpbuf, "%s", inet_ntoa(client));
		if (yflag) {
			/*
			 * Delete any hostname associated with this
			 * ip address.
			 */
			err = rm_dd_entry(TBL_HOSTS, ns, NULL, domp, &tbl_err,
			    tmpbuf, NULL);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_DEL_HOST_FAIL), tmpbuf,
				    disp_err(tbl_err));
				if (tbl_err == TBL_NO_ENTRY)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
		}
		err = rm_dd_entry(TBL_DHCPIP, ns, namebuf, domp, &tbl_err,
		    NULL, tmpbuf, NULL, NULL, NULL);
		if (err == TBL_FAILURE) {
			(void) fprintf(stderr, gettext(MSG_DEL_FAILED),
			    tmpbuf, disp_err(tbl_err));
			if (tbl_err == TBL_NO_ENTRY)
				err = DD_ENOENT;
			else
				err = DD_WARNING;
		} else
			err = DD_SUCCESS;
		break;
	case PN_OPT_MODIFY:
		(void) sprintf(tmpbuf, "%s", inet_ntoa(client));

		if (hflag) {
			/*
			 * We give up if an entry with this name is already
			 * being used by a different IP address.
			 */
			err = list_dd(TBL_HOSTS, ns, NULL, domp, &tbl_err,
			    &tbl, NULL, hbuf);

			if (err == TBL_SUCCESS && strcmp(tbl.ra[0]->ca[0],
			    tmpbuf) != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_MOD_HOST_INUSE), hbuf);
				err = DD_EEXISTS;
				free_dd(&tbl);
				break;
			}

			/*
			 * If we didn't already find the right IP address
			 * hosts record, locate it. If we did find that
			 * the client already has the right name, then we
			 * need do nothing.
			 */
			if (err != TBL_SUCCESS) {
				free_dd(&tbl); /* nuke potential leftovers */
				/*
				 * Locate the hosts record by IP address, and
				 * change the primary host name.
				 */
				err = list_dd(TBL_HOSTS, ns, NULL, domp,
				    &tbl_err, &tbl, tmpbuf, NULL);
				if (err == TBL_SUCCESS) {
					/*
					 * Maintain alias and comment from
					 * current record. We only change the
					 * primary host name.
					 */
					err = mod_dd_entry(TBL_HOSTS, ns, NULL,
					    domp, &tbl_err, tmpbuf, NULL,
					    tmpbuf, hbuf, tbl.ra[0]->ca[2],
					    tbl.ra[0]->ca[3]);
					free_dd(&tbl);
					if (err == TBL_FAILURE) {
						(void) fprintf(stderr,
						    gettext(MSG_MOD_CHNG_HOSTS),
						    tmpbuf, hbuf,
						    disp_err(tbl_err));
						err = DD_WARNING;
						break;
					}
				} else {
					/*
					 * There isn't a hosts record. Add one.
					 * Unfortunately, these accesses are not
					 * idempotent, thus some other instance
					 * could update this before we get a
					 * chance. In any case, it isn't a
					 * fatal error.
					 */
					err = add_dd_entry(TBL_HOSTS, ns, NULL,
					    domp, &tbl_err, tmpbuf, hbuf, NULL,
					    NULL);
					if (err == TBL_FAILURE) {
						(void) fprintf(stderr,
						    gettext(
						    MSG_MOD_ADDHOST_FAILED),
						    tmpbuf, hbuf,
						    disp_err(tbl_err));
						err = DD_WARNING;
						break;
					}
				}
			}
		}

		/* Find the existing entry */
		err = list_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err, &tbl,
		    NULL, tmpbuf, NULL, NULL, NULL);
		if (err == TBL_FAILURE) {
			(void) fprintf(stderr, gettext(MSG_MOD_NOENT),
			    tmpbuf, disp_err(tbl_err));
			if (tbl_err == TBL_NO_ENTRY)
				err = DD_ENOENT;
			else
				err = DD_WARNING;
			break;
		}

		if (iflag)
			cidp = cidbuf;
		else
			cidp = tbl.ra[0]->ca[0];
		if (fflag)
			fp = fbuf;
		else
			fp = tbl.ra[0]->ca[1];
		if (nflag)
			cip = cipbuf;
		else
			cip = tbl.ra[0]->ca[2];
		if (sflag)
			sip = sipbuf;
		else
			sip = tbl.ra[0]->ca[3];
		if (eflag)
			exp = expbuf;
		else
			exp = tbl.ra[0]->ca[4];
		if (mflag)
			mp = mbuf;
		else
			mp = tbl.ra[0]->ca[5];
		if (cflag)
			cp = comment;
		else
			cp = tbl.ra[0]->ca[6];

		/* assume only one row */
		err = mod_dd_entry(TBL_DHCPIP, ns, namebuf, domp, &tbl_err,
		    NULL, tmpbuf, NULL, NULL, NULL, cidp, fp, cip, sip,
		    exp, mp, cp);
		free_dd(&tbl);
		if (err == TBL_FAILURE) {
			(void) fprintf(stderr, gettext(MSG_MOD_FAILED),
			    tmpbuf, disp_err(tbl_err));
			err = DD_WARNING;
		}
		break;
	case PN_OPT_DISPLAY:
		if ((aflag + cflag + eflag + fflag + hflag + iflag + mflag +
		    nflag + sflag + yflag) != 0) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}

		if (vflag)
			timefp = ctime;
		else
			timefp = SLtime;

		/* Display the whole table */
		err = list_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err, &tbl,
		    NULL, NULL, NULL, NULL, NULL);
		if (err == TBL_SUCCESS) {
			(void) fprintf(stdout,
			    "%-8s\t%-4s\t%-8s\t%-8s\t%-25s\t%-8s\t%s\n\n",
			    gettext("Client ID"), gettext("Flags"),
			    gettext("Client IP"), gettext("Server IP"),
			    gettext("Lease Expiration"), gettext("Macro"),
			    gettext("Comment"));

			for (i = tbl.rows - 1; i >= 0; i--) {
				exptime = (time_t)strtol(tbl.ra[i]->ca[4],
				    (char **)NULL, 10);
				if (exptime == (time_t)-1) {
					TimeString = gettext("Forever");
				} else if (exptime == (time_t)0) {
					TimeString = gettext("Zero");
				} else {
					TimeString = timefp(&exptime);
					if (TimeString != NULL) {
						/* nuke the new line */
						tp = strchr(TimeString, '\n');
						if (tp != NULL)
							*tp = '\0';
					}
				}
				if (vflag) {
					convert_ascip_to_name(tbl.ra[i]->ca[2],
					    client_buf, sizeof (client_buf));
					clientp = client_buf;
					convert_ascip_to_name(tbl.ra[i]->ca[3],
					    server_buf, sizeof (server_buf));
					serverp = server_buf;
					flagp = v_flags(tbl.ra[i]->ca[1]);
				} else {
					flagp = tbl.ra[i]->ca[1];
					clientp = tbl.ra[i]->ca[2];
					serverp = tbl.ra[i]->ca[3];
				}
				(void) fprintf(stdout,
"%-8s\t%-4s\t%-8s\t%-8s\t%-25s\t%-8s\t%s\n",
				    tbl.ra[i]->ca[0], flagp, clientp, serverp,
				    TimeString, tbl.ra[i]->ca[5],
				    tbl.ra[i]->ca[6] == NULL ? "" :
					tbl.ra[i]->ca[6]);
			}
			free_dd(&tbl);
		} else
			err = DD_ENOENT;
		break;
	case PN_OPT_DESTROY:
		if ((aflag + cflag + eflag + fflag + hflag + iflag + mflag +
		    nflag + sflag + yflag) != 0) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}
		tblstatp = NULL;
		if (stat_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err,
		    &tblstatp) != TBL_SUCCESS || check_dd_access(tblstatp,
		    &tbl_err) != 0) {
			(void) fprintf(stderr, gettext(MSG_RM_FAILED),
			    namebuf, disp_err(tbl_err));
			if (tblstatp != NULL)
				free_dd_stat(tblstatp);
			if (tbl_err == TBL_NO_TABLE)
				err = DD_ENOENT;
			else
				err = DD_WARNING;
			break;
		}
		free_dd_stat(tblstatp);
		err = del_dd(TBL_DHCPIP, ns, namebuf, domp, &tbl_err);
		if (err == TBL_SUCCESS)
			err = DD_SUCCESS;
		else {
			(void) fprintf(stderr, gettext(MSG_RM_FAILED), namebuf,
			    disp_err(tbl_err));
			err = DD_WARNING;
		}
		break;
	case PN_OPT_LIST:
		if ((aflag + cflag + eflag + fflag + hflag + iflag + mflag +
		    nflag + sflag + + vflag + yflag) != 0) {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
			break;
		}
		if (ns == TBL_NS_UFS) {
			locp = namebuf;
		} else {
			locp = domp;
		}
		listp = dd_ls(ns, locp, &tbl_err);
		if (listp != NULL) {
			for (i = 0; listp[i] != NULL; ++i) {
				for (cp = listp[i]; *cp != '\0'; ++cp) {
					if (*cp == '_')
						*cp = '.';
				}
				(void) fprintf(stdout, "%s\n", listp[i]);
				free(listp[i]);
			}
		} else {
			if (tbl_err == TBL_SUCCESS) {
				err = DD_SUCCESS;
			} else if (tbl_err == TBL_NO_ENTRY) {
				err = DD_ENOENT;
				(void) fprintf(stderr,
				    gettext(MSG_BAD_LIST_PATH),
				    locp == NULL ? "" : locp);
			} else {
				(void) fprintf(stderr, gettext(MSG_LIST_FAILED),
				    disp_err(tbl_err));
				err = DD_CRITICAL;
			}
		}
		free(listp);
		break;
	default:
		(void) fprintf(stderr, gettext(MSG_USAGE));
		err = DD_CRITICAL;
		break;
	}

	return (err);
}


/*
 * Convert a date in the form mm/dd/yyyy to seconds since Unix time 0.
 */
static int
exp_date_to_secs(const char *date, time_t *val)
{
	struct tm time = { 0 };

	/* Handle case where lease is cleared or permanent */
	if (strcmp(date, "0") == 0 || strcmp(date, "-1") == 0) {
		*val = atol(date);
	} else {
		if (strptime(date, "%m/%d/%Y", &time) == NULL)
			return (-1);
		*val =  mktime(&time);
	}
	return (0);
}

/*
 * Convert a potential dotted notation IP address or host name into a
 * ip address.
 */
static int
convert_to_ip(const char *op, struct in_addr *ip, char *e)
{
	struct in_addr	tmp;
	struct hostent	*he;

	if (isdigit(*op)) {
		ip->s_addr = inet_addr(op);
		if (ip->s_addr == (ulong_t)0xffffffff) {
			(void) sprintf(e, gettext(MSG_INV_IP));
			return (EINVAL);
		}
	} else {
		if ((he = gethostbyname(op)) != NULL &&
		    he->h_addrtype == AF_INET &&
		    he->h_length == sizeof (struct in_addr)) {
			(void) memcpy((char *)&tmp, he->h_addr_list[0],
			    sizeof (tmp));
			ip->s_addr = tmp.s_addr;
		} else {
			(void) sprintf(e, gettext(MSG_NO_CNVT_NM_TO_IP));
			return (errno);
		}
	}
	return (0);
}

/*
 * Convert ascii Dotted IP to a hostname.
 */
static void
convert_ascip_to_name(const char *dotip, char *bufp, int buflen)
{
	struct in_addr	tmp;
	struct hostent	*he;

	bufp[0] = '\0';
	if (isdigit(*dotip)) {
		tmp.s_addr = inet_addr(dotip);
		if (tmp.s_addr != (ulong_t)0xffffffff) {
			he = gethostbyaddr((char *)&tmp, 4, AF_INET);
			if (he != NULL)
				(void) strncpy(bufp, he->h_name, buflen);
		}
	}
	if (bufp[0] == '\0')
		(void) strncpy(bufp, dotip, buflen);
}


static char *
SLtime(const time_t *clock)
{
	static char	timebuf[27];	/* buffer is plenty big enough */

	if (cftime(timebuf, "%m/%d/%Y", clock) == 0)
		(void) strcpy(timebuf, "UNKNOWN");
	return (timebuf);
}

static char *
v_flags(const char *flagsp)
{
	int	flags, count = 0;
	static char	buf[BUFSIZ];

	flags = atoi(flagsp);
	if (flags == F_DYNAMIC) {
		buf[count++] = 'D';
	} else {
		if (flags & F_AUTOMATIC)
			buf[count++] = 'P';
		if (flags & F_MANUAL)
			buf[count++] = 'M';
		if (flags & F_UNUSABLE)
			buf[count++] = 'U';
		if (flags & F_BOOTP_ONLY)
			buf[count++] = 'B';
	}
	buf[count] = '\0';
	return (buf);
}
