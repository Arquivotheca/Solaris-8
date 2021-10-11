/*
 * Routines and structures which are used from CMU's 2.2 bootp implementation
 * are labelled as such. Code not labelled is:
 *
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcptab.c	1.67	99/08/16 SMI"

/*
 *	Copyright 1988, 1991 by Carnegie Mellon University
 *
 *			All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carnegie Mellon University not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

/*
 * in.dhcpd configuration file reading code.
 *
 * The routines in this file deal with reading, interpreting, and storing
 * the information found in the in.dhcpd configuration file (usually
 * /etc/dhcptab).
 */

/*
 * XXXX What's missing: Symbol code is very generic, but doesn't allow
 * per symbol granularity checking - ie, using goodname() to check the
 * hostname, for example. Perhaps each symbol should have a verifier
 * function possibly associated with it (null is ok), which would return
 * TRUE if ok, FALSE if not, and print out a nasty message.
 *
 * Option overload. If set, then NO BOOTFILE or SNAME values can exist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include "netinet/dhcp.h"
#include "hash.h"
#include "dhcpd.h"
#include "per_network.h"
#include <locale.h>

/*
 * Local constants
 */
#define	SUCCESS			  0
#define	E_SYNTAX_ERROR		(-1)
#define	E_UNKNOWN_SYMBOL	(-2)
#define	E_INCLUDE_UNDEFINED	(-3)
#define	E_BAD_IPADDR		(-4)
#define	E_BAD_STRING		(-5)
#define	E_BAD_OCTET		(-6)
#define	E_BAD_NUMBER		(-7)
#define	E_BAD_BOOLEAN		(-8)
#define	E_NOT_ENOUGH_IP		(-9)
#define	E_BAD_IP_GRAN		(-10)

#define	OP_ADDITION		  1	/* Operations on tags */
#define	OP_DELETION		  2
#define	OP_BOOLEAN		  3

#define	MAXUCHARLEN		255	/* Max option size (8 bits) */
#define	MAXENTRYLEN		3072	/* Max size of an entire entry */
#define	MAX_ITEMS		16	/* Max number of items in entry */
#define	MAX_MACRO_NESTING	20	/* Max number of nested includes */
#define	HASHTABLESIZE		257	/* must be a prime number */

#define	DB_DHCP_MAC		'm'	/* Like TBL_DHCP_{MACRO,SYMBOL} */
#define	DB_DHCP_SYM		's'	/* But not strings! */

static int	nentries;		/* Total number of entries */
static int	include_nest;		/* macro nesting counter */
static Tbl	newtbl;			/* reordered Tbl. */
static Tbl	oldtbl;			/* The original tbl. */
static hash_tbl	*mtable;
static time_t	mtable_time;		/* load time of hash table */

/*
 * Option descriptor. Describes explicitly the information associated with
 * a DHCP option.
 */

typedef enum {
	ASCII	= 0,	/* Ascii string */
	OCTET	= 1,	/* Hex octets */
	IP	= 2,	/* Dotted decimal Internet address */
	NUMBER	= 3,	/* Host order number */
	BOOL	= 4,	/* boolean. */
	INCLUDE = 5	/* Macro include */
} CDTYPE;

#define	DHCP_SYMBOL_SIZE	8
typedef struct {
	char	nm[DHCP_SYMBOL_SIZE + 1];	/* symbol name */
	ushort_t	code;		/* Parameter code */
	uchar_t	vend;		/* Vendor code, if code == CD_VENDOR_SPEC */
	char	**class;	/* client class table */
	CDTYPE	type;		/* Type of parameter */
	uchar_t	gran;		/* Granularity of type */
	uchar_t	max;		/* maximum items of type at granularity */
} SYM;

#define	INCLUDE_SYM	"Include"	/* symbol for macro include */

static SYM sym_list[] = {
/* Symbol		Code		Vend	Class	Type	G  Max	*/
/* ======		====		====	=====	====	=  ===	*/
{ "Subnet",	CD_SUBNETMASK,		0,	{0},	IP,	1, 1 	},
{ "UTCoffst",	CD_TIMEOFFSET,		0,	{0},	NUMBER,	4, 1 	},
{ "Router",	CD_ROUTER,		0,	{0},	IP,	1, 0 	},
{ "Timeserv",	CD_TIMESERV,		0,	{0},	IP,	1, 0 	},
{ "IEN116ns",	CD_IEN116_NAME_SERV,	0,	{0},	IP,	1, 0 	},
{ "DNSserv",	CD_DNSSERV,		0,	{0},	IP,	1, 0 	},
{ "Logserv",	CD_LOG_SERV,		0,	{0},	IP,	1, 0 	},
{ "Cookie",	CD_COOKIE_SERV,		0,	{0},	IP,	1, 0 	},
{ "Lprserv",	CD_LPR_SERV,		0,	{0},	IP,	1, 0 	},
{ "Impress",	CD_IMPRESS_SERV,	0,	{0},	IP,	1, 0 	},
{ "Resource",	CD_RESOURCE_SERV,	0,	{0},	IP,	1, 0 	},
{ "Hostname",	CD_BOOL_HOSTNAME,	0,	{0},	BOOL,	0, 0 	},
{ "Bootsize",	CD_BOOT_SIZE,		0,	{0},	NUMBER, 2, 1 	},
{ "Dumpfile",	CD_DUMP_FILE,		0,	{0},	ASCII,	0, 0 	},
{ "DNSdmain",	CD_DNSDOMAIN,		0,	{0},	ASCII,	0, 0 	},
{ "Swapserv",	CD_SWAP_SERV,		0,	{0},	IP,	1, 1 	},
{ "Rootpath",	CD_ROOT_PATH,		0,	{0},	ASCII,	0, 0 	},
{ "ExtendP",	CD_EXTEND_PATH,		0,	{0},	ASCII,	0, 0 	},
{ "IpFwdF",	CD_IP_FORWARDING_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "NLrouteF",	CD_NON_LCL_ROUTE_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "PFilter",	CD_POLICY_FILTER,	0,	{0},	IP, 	2, 0 	},
{ "MaxIpSiz",	CD_MAXIPSIZE,		0,	{0},	NUMBER, 2, 1 	},
{ "IpTTL",	CD_IPTTL,		0,	{0},	NUMBER,	1, 1 	},
{ "PathTO",	CD_PATH_MTU_TIMEOUT,	0,	{0},	NUMBER,	4, 1 	},
{ "PathTbl",	CD_PATH_MTU_TABLE_SZ,	0,	{0},	NUMBER,	2, 0 	},
{ "MTU",	CD_MTU,			0,	{0},	NUMBER,	2, 1 	},
{ "SameMtuF",	CD_ALL_SUBNETS_LCL_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "Broadcst",	CD_BROADCASTADDR,	0,	{0},	IP,	1, 1 	},
{ "MaskDscF",	CD_MASK_DISCVRY_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "MaskSupF",	CD_MASK_SUPPLIER_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "RDiscvyF",	CD_ROUTER_DISCVRY_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "RSolictS",	CD_ROUTER_SOLICIT_SERV,	0,	{0},	IP,	1, 1 	},
{ "StaticRt",	CD_STATIC_ROUTE,	0,	{0},	IP,	2, 0 	},
{ "TrailerF",	CD_TRAILER_ENCAPS_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "ArpTimeO",	CD_ARP_TIMEOUT,		0,	{0},	NUMBER,	4, 1 	},
{ "EthEncap",	CD_ETHERNET_ENCAPS_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "TcpTTL",	CD_TCP_TTL,		0,	{0},	NUMBER,	1, 1 	},
{ "TcpKaInt",	CD_TCP_KALIVE_INTVL,	0,	{0},	NUMBER,	4, 1 	},
{ "TcpKaGbF",	CD_TCP_KALIVE_GRBG_ON,	0,	{0},	NUMBER,	1, 1 	},
{ "NISdmain",	CD_NIS_DOMAIN,		0,	{0},	ASCII,	0, 0 	},
{ "NISservs",	CD_NIS_SERV,		0,	{0},	IP,	1, 0 	},
{ "NTPservs",	CD_NTP_SERV,		0,	{0},	IP,	1, 0 	},
{ "NetBNms",	CD_NETBIOS_NAME_SERV,	0,	{0},	IP,	1, 0 	},
{ "NetBDsts",	CD_NETBIOS_DIST_SERV,	0,	{0},	IP,	1, 0 	},
{ "NetBNdT",	CD_NETBIOS_NODE_TYPE,	0,	{0},	NUMBER,	1, 1 	},
{ "NetBScop",	CD_NETBIOS_SCOPE,	0,	{0},	ASCII,	0, 0 	},
{ "XFontSrv",	CD_XWIN_FONT_SERV,	0,	{0},	IP,	1, 0 	},
{ "XDispMgr",	CD_XWIN_DISP_SERV,	0,	{0},	IP,	1, 0 	},
{ "LeaseTim",	CD_LEASE_TIME,		0,	{0},	NUMBER,	4, 1 	},
{ "Message",	CD_MESSAGE,		0,	{0},	ASCII,	0, 0 	},
{ "T1Time",	CD_T1_TIME,		0,	{0},	NUMBER,	4, 1 	},
{ "T2Time",	CD_T2_TIME,		0,	{0},	NUMBER,	4, 1 	},
{ "NW_dmain",	CD_NW_IP_DOMAIN,	0,	{0},	ASCII,	0, 0	},
{ "NWIPOpts",	CD_NW_IP_OPTIONS,	0,	{0},	OCTET,	1, 128	},
{ "NIS+dom",	CD_NISPLUS_DMAIN,	0,	{0},	ASCII,	0, 0	},
{ "NIS+serv",	CD_NISPLUS_SERVS,	0,	{0},	IP,	1, 0	},
{ "TFTPsrvN",	CD_TFTP_SERV_NAME,	0,	{0},	ASCII,	0, 64	},
{ "OptBootF",	CD_OPT_BOOTFILE_NAME,	0,	{0},	ASCII,	0, 128	},
{ "MblIPAgt",	CD_MOBILE_IP_AGENT,	0,	{0},	IP,	1, 0	},
{ "SMTPserv",	CD_SMTP_SERVS,		0,	{0},	IP,	1, 0	},
{ "POP3serv",	CD_POP3_SERVS,		0,	{0},	IP,	1, 0	},
{ "NNTPserv",	CD_NNTP_SERVS,		0,	{0},	IP,	1, 0	},
{ "WWWservs",	CD_WWW_SERVS,		0,	{0},	IP,	1, 0	},
{ "Fingersv",	CD_FINGER_SERVS,	0,	{0},	IP,	1, 0	},
{ "IRCservs",	CD_IRC_SERVS,		0,	{0},	IP,	1, 0	},
{ "STservs",	CD_STREETTALK_SERVS,	0,	{0},	IP,	1, 0	},
{ "STDAservs",	CD_STREETTALK_DA_SERVS,	0,	{0},	IP,	1, 0	},
{ "BootFile",	CD_BOOTFILE,		0,	{0},	ASCII,	0, 128	},
{ "BootSrvA",	CD_SIADDR,		0,	{0},	IP,	1, 1 	},
{ "BootSrvN",	CD_SNAME,		0,	{0},	ASCII,	0, 64 	},
{ "LeaseNeg",	CD_BOOL_LEASENEG,	0,	{0},	BOOL,	0, 0 	},
{ "EchoVC",	CD_BOOL_ECHO_VCLASS,	0,	{0},	BOOL,	0, 0 	},
{ "BootPath",	CD_BOOTPATH,		0,	{0},	ASCII,	0, 128	},
{ INCLUDE_SYM,	0,			0,	{0},	INCLUDE, 0, 32	}
};

/*
 * Forward declarations.
 */
static SYM	*sym_head = &sym_list[0];
static int	sym_num_items = sizeof (sym_list) / sizeof (SYM);
static MACRO *process_entry(Row *);
static int process_data(ENCODE *, SYM *, char **);
static int eval_symbol(char **, MACRO *);
static char *get_string(char **, char *, uchar_t *);
static void adjust(char **);
static void eat_whitespace(char **);
static int get_addresses(char **, SYM *, ENCODE *, int);
static int prs_inetaddr(char **, uint32_t *);
static void get_sym_name(char *, char **);
static void print_error_msg(int, uchar_t);
static int define_symbol(char **, char *);
static int first_macro_row(Tbl *);
static int scan_include(char **, char *);
static int check_includes(int, char *);
static void free_macro(MACRO *);
static int macro_cmp(MACRO *, MACRO *);
static void add_vndlist(ENCODE *, MACRO *, SYM *);

/*
 * Initialize the hash table.
 */
int
inittab(void)
{
	/*
	 * Allocate hash table
	 */
	mtable = hash_Init(HASHTABLESIZE);

	if (mtable == (hash_tbl *)0)
		return (EINVAL);

	return (0);
}

/*
 * Check presence/access to dhcptab database file.
 */
int
checktab(void)
{
	int	err;
	char	*datastore;
	Tbl_stat	*tbl_statp;
	int		tbl_err, ns;
	char		*pathp = NULL;

	if ((ns = dd_ns(&tbl_err, &pathp)) == TBL_FAILURE)
		return (EINVAL);

	if (ns == TBL_NS_UFS)
		datastore = "files";
	else
		datastore = "nisplus";

	/*
	 * Check access on dhctab.
	 */
	tbl_err = 0;
	tbl_statp = NULL;
	err = 0;
	if (stat_dd(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    &tbl_statp) != TBL_SUCCESS || check_dd_access(tbl_statp,
	    &tbl_err) != 0) {
		switch (tbl_err) {
		case TBL_NO_ACCESS:
			dhcpmsg(LOG_ERR,
			    "No permission to access dhcptab in %s (%s)\n",
			    pathp, datastore);
			err = EACCES;
			break;
		case TBL_NO_TABLE:
			dhcpmsg(LOG_INFO,
			    "Dhcptab table does not exist in %s (%s)\n",
			    pathp, datastore);
			err = ENOENT;
			break;
		default:
			if (!ethers_compat || verbose) {
				dhcpmsg(LOG_ERR,
"Error checking status of dhcptab in %s (%s)\n",
				    pathp, datastore);
			}
			err = EINVAL;
			break;
		}
	}
	if (tbl_statp != NULL)
		free_dd_stat(tbl_statp);
	return (err);
}

/*
 * Read dhcptab database file.
 */
int
readtab(int preserve)
{
	int	err = 0, first_mac;
	MACRO		*mc;
	unsigned int	hc;
	int		tbl_err;
	uint32_t	i;
	Tbl		dhcptab;

	/* Get the *entire* dhcptab. */
	(void) memset((void *)&dhcptab, 0, sizeof (dhcptab));
	if ((err = list_dd(TBL_DHCPTAB, TBL_NS_DFLT, NULL, NULL, &tbl_err,
	    &dhcptab, NULL, NULL)) != TBL_SUCCESS) {
		if (tbl_err == TBL_NO_TABLE) {
			dhcpmsg(LOG_INFO, "Empty dhcptab macro database.\n");
			err = 0;	/* not a "real" error */
		} else
			dhcpmsg(LOG_ERR,
			    "Error opening macro database: %d\n", tbl_err);
		goto leave_readtab;
	}

	/*
	 * Because libdhcp doesn't guarantee any order, we need to
	 * preprocess the macro list to guarantee that macros which are
	 * included in other macro definitions are already defined prior
	 * to their use. This means that macro processing is a two step process.
	 */
	first_mac = first_macro_row(&dhcptab);
	include_nest = 0;
	if (first_mac >= 0) {
		oldtbl = dhcptab;	/* struct copy */
		newtbl.rows = dhcptab.rows;
		/* LINTED [smalloc returns lw aligned addresses] */
		newtbl.ra = (Row **)smalloc(sizeof (Row **) * dhcptab.rows);
		for (i = 0; i < first_mac; i++)
			newtbl.ra[i] = oldtbl.ra[i];	/* copy symdefs */
		for (i = first_mac; i < oldtbl.rows; i++) {
			if ((err = check_includes(first_mac,
			    oldtbl.ra[i]->ca[0])) != 0)
				break;
		}
		if (err != 0) {
			free(newtbl.ra);
			free_dd(&dhcptab);
			goto leave_readtab;
		} else {
			free(dhcptab.ra);
			dhcptab = newtbl;	/* struct copy */
		}
	}

	resettab();

	/*
	 * Now table is reordered. process as usual.
	 */
	nentries = 0;
	for (i = 0; i < dhcptab.rows; i++) {

		if ((mc = process_entry(dhcptab.ra[i])) == (MACRO *)NULL)
			continue;

		hc = hash_HashFunction((uchar_t *)mc->nm,
		    (uint_t)strlen(mc->nm));

		if (hash_Insert(mtable, hc, macro_cmp, mc->nm, mc) < 0) {
			dhcpmsg(LOG_WARNING,
			    "Duplicate macro definition: %s\n", mc->nm);
			continue;
		}
		nentries++;
	}

	mtable_time = time(NULL);

	if (verbose) {
		dhcpmsg(LOG_INFO,
		    "Read %d entries from DHCP macro database on %s",
		    nentries, ctime(&mtable_time));
	}

	free_dd(&dhcptab);

leave_readtab:
	if (preserve && err != 0) {
		dhcpmsg(LOG_WARNING,
		    "DHCP macro database rescan failed, using scan: %s",
		    ctime(&mtable_time));
		err = 0;
	}
	return (err);
}

/*
 *  Reset the dhcptab hash table, free any dynamic symbol definitions.
 */
void
resettab(void)
{
	int	i, j;

	/* Entirely erase all hash tables. */
	hash_Reset(mtable, free_macro);

	/*
	 * Dump any dynamically defined symbol definitions, and reinitialize.
	 */
	if (sym_head != &sym_list[0]) {
		for (i = 0; i < sym_num_items; i++) {
			if (sym_head[i].code == CD_VENDOR_SPEC) {
				for (j = 0; sym_head[i].class[j] != NULL;
				    j++) {
					free(sym_head[i].class[j]);
				}
				free(sym_head[i].class);
			}
		}
		free(sym_head);
		sym_head = &sym_list[0];
		sym_num_items = sizeof (sym_list) / sizeof (SYM);
	}
}

/*
 * Given an value field pptr, return the first INCLUDE_SYM value found in
 * include, updating pptr along the way. Returns nonzero if no INCLUDE_SYM
 * symbol is found (pptr is still updated).
 */
static int
scan_include(char **cpp, char *include)
{
	char	t_sym[DHCP_SYMBOL_SIZE + 1];
	uchar_t	ilen;

	while (*cpp && **cpp != '\0') {
		eat_whitespace(cpp);
		get_sym_name(t_sym, cpp);
		if (strcmp(t_sym, INCLUDE_SYM) == 0) {
			ilen = DHCP_SCRATCH;
			if (**cpp == '=')
				(*cpp)++;
			(void) get_string(cpp, include, &ilen);
			include[ilen] = '\0';
			return (0);
		} else
			adjust(cpp);
	}
	return (1);
}

/*
 * Return the first macro row in dhcptab. Returns -1 if no macros exist.
 */
static int
first_macro_row(Tbl *tblp)
{
	int i;

	for (i = 0; i < tblp->rows; i++) {
		if (tolower((int)*tblp->ra[i]->ca[1]) == (int)DB_DHCP_MAC)
			return (i);
	}
	return (-1);
}

/*
 * RECURSIVE function: Scans for included macros, and reorders Tbl to
 * ensure macro definitions occur in the correct order.
 *
 * Returns 0 for success, nonzero otherwise.
 */
static int
check_includes(int first, char *mname)
{
	char	include[DHCP_SCRATCH + 1];
	int	m, err = 0;
	Row	*current_rowp = (Row *)NULL;
	char	*cp;

	include_nest++;

	if (include_nest > MAX_MACRO_NESTING) {
		dhcpmsg(LOG_ERR,
		    "Circular macro definition using: %s\n", mname);
		err = E_SYNTAX_ERROR;
		goto leave_check_include;
	}

	for (m = first; m < newtbl.rows; m++) {
		if (newtbl.ra[m] != (Row *)NULL &&
		    strcmp(newtbl.ra[m]->ca[0], mname) == 0) {
			err = 0; /* already processed */
			goto leave_check_include;
		}
	}

	/*
	 * is it defined someplace?
	 */
	for (m = first; m < oldtbl.rows; m++) {
		if (strcmp(oldtbl.ra[m]->ca[0], mname) == 0) {
			current_rowp = oldtbl.ra[m];
			break;
		}
	}

	if (current_rowp == (Row *)NULL) {
		dhcpmsg(LOG_ERR, "Undefined macro: %s\n", mname);
		err = E_SYNTAX_ERROR;
		goto leave_check_include;
	}

	/*
	 * Scan value field, looking for includes.
	 */
	cp = current_rowp->ca[2];
	while (cp) {
		adjust(&cp);
		if (scan_include(&cp, include) != 0) {
			/* find a free entry */
			for (m = first; m < newtbl.rows; m++) {
				if (newtbl.ra[m] == NULL)
					break;
			}
			if (m >= newtbl.rows) {
				dhcpmsg(LOG_ERR,
				    "Macro expansion (Include=%s) error!\n",
				    mname);
				err = E_SYNTAX_ERROR;
			} else {
				newtbl.ra[m] = current_rowp;
				err = 0;
			}
			break;
		}

		if (*include == '\0') {
			/*
			 * Null value for macro name. We can safely ignore
			 * this entry. An error message will be generated
			 * later during encode processing.
			 */
			continue;
		}

		if (strcmp(mname, include) == 0) {
			dhcpmsg(LOG_ERR,
			    "Circular macro definition using: %s\n", mname);
			err = E_SYNTAX_ERROR;
			break;
		}

		/* Recurse. */
		if ((err = check_includes(first, include)) != 0)
			break;
	}

leave_check_include:
	include_nest--;
	return (err);
}

/*
 * Given a macro name, look it up in the hash table.
 * Returns ptr to MACRO structure, NULL if error occurs.
 */
MACRO *
get_macro(char *mnamep)
{
	uint_t hc;

	if (mnamep == (char *)NULL)
		return ((MACRO *)NULL);

	hc = hash_HashFunction((uchar_t *)mnamep, (uint_t)strlen(mnamep));

	return ((MACRO *)hash_Lookup(mtable, hc, macro_cmp, mnamep));
}

static void
free_macro(MACRO *mp)
{
	int i;

	if (mp) {
		free_encode_list(mp->head);
		for (i = 0; i < mp->classes; i++) {
			if (mp->list[i]->head != NULL)
				free_encode_list(mp->list[i]->head);
			free(mp->list[i]);
		}
		free(mp->list);
		free(mp);
	}
}

static int
macro_cmp(MACRO *m1, MACRO *m2)
{
	if (!m1 || !m2)
		return (FALSE);

	if (strcmp(m1->nm, m2->nm) == 0)
		return (TRUE);
	else
		return (FALSE);
}

/*
 * Parse out all the various tags and parameters in the row entry pointed
 * to by "src".
 *
 * Returns 0 for success, nozero otherwise.
 */
static MACRO *
process_entry(Row *src)
{
	char *cp;
	MACRO *mc, *retval = NULL;

	if (src == NULL)
		return (retval);

	if ((int)strlen(src->ca[0]) > (int)DT_MAX_MACRO_LEN) {
		dhcpmsg(LOG_ERR,
		    "Token: %s is too long. Limit: %d characters.\n",
		    src->ca[0], DT_MAX_MACRO_LEN);
		return (retval);
	}

	switch (tolower((int)*src->ca[1])) {
	case DB_DHCP_SYM:
		/* New Symbol definition */
		cp = src->ca[2];
		if (define_symbol(&cp, src->ca[0]) != 0)
			dhcpmsg(LOG_ERR,
			    "Bad Runtime symbol definition: %s\n", src->ca[0]);
		/* Success. Treat new symbol like the predefines. */
		break;
	case DB_DHCP_MAC:
		/* Macro definition */

		/* LINTED [smalloc returns longword aligned addresses.] */
		mc = (MACRO *)smalloc(sizeof (MACRO));
		(void) strcpy(mc->nm, src->ca[0]);

		cp = src->ca[2];
		adjust(&cp);
		while (*cp != '\0') {
			if (eval_symbol(&cp, mc) != 0) {
				dhcpmsg(LOG_ERR,
				    "Error processing macro: %s\n", mc->nm);
				free_macro(mc);
				return (NULL);
			}
			adjust(&cp);
			eat_whitespace(&cp);
		}
		retval = mc;
		break;
	default:
		dhcpmsg(LOG_ERR, "Unrecognized token: %s.\n", src->ca[0]);
		break;
	}
	return (retval);
}

/*
 * This function processes the parameter name pointed to by "symbol" and
 * updates the appropriate ENCODE structure in data if one already exists,
 * or allocates a new one for this parameter.
 */
static int
eval_symbol(char **symbol, MACRO *mc)
{
	int	index, optype, i, j, err = 0;
	SYM	*sp;
	ENCODE	*tmp;
	VNDLIST	**mpp, **ipp;
	MACRO	*ic;
	uint_t	hc;
	uchar_t	ilen;
	char	t_sym[DHCP_SYMBOL_SIZE + 1];
	char	include[DHCP_SCRATCH + 1];

	if ((*symbol)[0] == ':')
		return (0);

	eat_whitespace(symbol);
	get_sym_name(t_sym, symbol);

	for (index = 0; index < sym_num_items; index++) {
		if (strcmp(t_sym, sym_head[index].nm) == 0)
			break;
	}
	if (index >= sym_num_items) {
		dhcpmsg(LOG_ERR, "Unrecognized symbol name: '%s'\n", t_sym);
		return (E_UNKNOWN_SYMBOL);
	} else
		sp = &sym_head[index];
	/*
	 * Determine the type of operation to be done on this symbol
	 */
	switch (**symbol) {
	case '=':
		optype = OP_ADDITION;
		(*symbol)++;
		break;
	case '@':
		optype = OP_DELETION;
		(*symbol)++;
		break;
	case ':':
	case '\0':
		optype = OP_BOOLEAN;
		break;
	default:
		dhcpmsg(LOG_ERR, "Syntax error: symbol: '%s' in macro: %s\n",
		    t_sym, mc->nm);
		return (E_SYNTAX_ERROR);
	}

	switch (optype) {
	case OP_ADDITION:
		switch (sp->type) {
		case BOOL:
			err =  E_SYNTAX_ERROR;
			break;

		case INCLUDE:
			/*
			 * If symbol type is INCLUDE, then walk the encode
			 * list, replacing any previous encodes with those
			 * from the INCLUDed macro. Vendor options are also
			 * merged, if their class and vendor codes match.
			 */
			ilen = DHCP_SCRATCH;
			(void) get_string(symbol, include, &ilen);
			include[ilen] = '\0';
			hc = hash_HashFunction((uchar_t *)include,
			    (uint_t)ilen);
			ic = (MACRO *)hash_Lookup(mtable, hc, macro_cmp,
			    include);
			if (ic == (MACRO *)NULL) {
				dhcpmsg(LOG_ERR, "WARNING: No macro: '%1$s' \
defined for 'Include' symbol in macro: %2$s\n",
				    include, mc->nm);
				adjust(symbol);
				return (0);
			}

			mc->head = combine_encodes(mc->head, ic->head,
			    ENC_DONT_COPY);

			if (ic->list == NULL && mc->list == NULL)
				break;

			/* Vendor options. */
			if (mc->list == NULL) {
				/*
				 * No combining necessary. Just duplicate
				 * ic's vendor options - all classes.
				 */
				/* LINTED [smalloc returns lwaligned addr] */
				mc->list = (VNDLIST **)smalloc(
				    sizeof (VNDLIST **) * ic->classes);
				for (i = 0;  i < ic->classes; i++) {
					/* LINTED [lwaligned addr] */
					mc->list[i] = (VNDLIST *)smalloc(
					    sizeof (VNDLIST));
					(void) strcpy(mc->list[i]->class,
					    ic->list[i]->class);
					mc->list[i]->head = copy_encode_list(
					    ic->list[i]->head);
				}
				mc->classes = ic->classes;
			} else {
				/* Class and vendor code must match. */
				for (i = 0, ipp = ic->list;
				    ipp && i < ic->classes; i++) {
					for (j = 0, mpp = mc->list;
					    j < mc->classes; j++) {
						if (strcmp(mpp[j]->class,
						    ipp[i]->class) == 0) {
							mpp[j]->head =
							    combine_encodes(
							    mpp[j]->head,
							    ipp[i]->head,
							    ENC_DONT_COPY);
							break;
						}
					}
				}
			}
			break;

		default:
			/*
			 * Get encode associated with symbol value.
			 */
			/* LINTED [smalloc ret's lw aligned] */
			tmp = (ENCODE *)smalloc(sizeof (ENCODE));

			if ((err = process_data(tmp, sp, symbol)) != 0)
				free_encode(tmp);
			else {
				/*
				 * Find/replace/add encode.
				 */
				if (sp->code != CD_VENDOR_SPEC) {
					replace_encode(&mc->head, tmp,
					    ENC_DONT_COPY);
				} else
					add_vndlist(tmp, mc, sp);
			}
			break;
		}
		break;

	case OP_DELETION:
		if (sp->type == INCLUDE)
			return (E_SYNTAX_ERROR);

		if (sp->code != CD_VENDOR_SPEC) {
			tmp = find_encode(mc->head, sp->code);
			if (tmp != (ENCODE *)NULL) {
				if (tmp->prev != (ENCODE *)NULL)
					tmp->prev->next = tmp->next;
				else
					mc->head = mc->head->next;
				free_encode(tmp);
			}
		} else {
			for (i = 0; sp->class && sp->class[i]; i++) {
				for (j = 0; mc->list && j < mc->classes;
				    j++) {
					if (strcmp(sp->class[i],
					    mc->list[j]->class) == 0) {
						tmp = find_encode(
						    mc->list[j]->head,
						    sp->code);
						if (tmp == NULL)
							continue;
						if (tmp->prev != NULL) {
							tmp->prev->next =
							    tmp->next;
						} else {
							mc->list[j]->head =
							    mc->list[j]->
							    head->next;
						}
						free_encode(tmp);
					}
				}
			}
		}

		err = 0;
		break;

	case OP_BOOLEAN:
		if (sp->type == INCLUDE)
			return (E_SYNTAX_ERROR);
		/*
		 * True signified by existence, false by omission.
		 */
		if (sp->code != CD_VENDOR_SPEC) {
			tmp = find_encode(mc->head, sp->code);
			if (tmp == (ENCODE *)NULL) {
				tmp = make_encode(sp->code, 0, NULL, 0);
				replace_encode(&mc->head, tmp,
				    ENC_DONT_COPY);
			}
		} else {
			for (i = 0; sp->class && sp->class[i]; i++) {
				for (j = 0; mc->list && j < mc->classes;
				    j++) {
					if (strcmp(sp->class[i],
					    mc->list[j]->class) == 0) {
						tmp = find_encode(
						    mc->list[j]->head,
						    sp->code);
						if (tmp == NULL) {
							tmp = make_encode(
							    sp->code, 0,
							    NULL, 0);
							replace_encode(
							    &mc->list[j]->
							    head, tmp,
							    ENC_DONT_COPY);
						}
					}
				}
			}
		}

		err = 0;
		break;
	}
	if (err)
		print_error_msg(err, index);
	return (err);
}

/*
 * Find/add option to appropriate client classes.
 */
static void
add_vndlist(ENCODE *vp, MACRO *mp, SYM *sp)
{
	int	i, j, class_exists, copy;
	VNDLIST	**tmp;

	copy = ENC_DONT_COPY;
	for (i = 0; sp->class && sp->class[i] != NULL; i++) {
		class_exists = 0;
		for (j = 0; mp->list && j < mp->classes; j++) {
			if (strcmp(sp->class[i], mp->list[j]->class) == 0) {
				class_exists = 1;
				replace_encode(&mp->list[j]->head, vp, copy);
				if (copy == ENC_DONT_COPY)
					copy = ENC_COPY;
			}
		}
		if (!class_exists) {
			tmp = (VNDLIST **)realloc(mp->list,
			    sizeof (VNDLIST **) * (j + 1));
			if (tmp != NULL)
				mp->list = tmp;
			else {
				dhcpmsg(LOG_ERR, "Warning: ran out of \
memory adding vendor class: '%1$s' for symbol: '%2$s'\n",
				    sp->class[i], sp->nm);
				break;
			}
			/* LINTED [smalloc ret's lw aligned] */
			mp->list[j] = (VNDLIST *)smalloc(sizeof (VNDLIST));
			(void) strcpy(mp->list[j]->class, sp->class[i]);
			if (copy == ENC_DONT_COPY) {
				mp->list[j]->head = vp;
				copy = ENC_COPY;
			} else
				mp->list[j]->head = dup_encode(vp);
			mp->classes++;
		}
	}
}

/*
 * This function interprets the value of pos into the val field of
 * current, based on the type field in default. If the len field is set
 * in default, then the size serialized is limited to this value. If the
 * len in default is 0, then data is serialized until an unquoted : or
 * null is encountered. Note that if the final size of the serialized data
 * not evenly divisible by default len, a syntax error is returned.
 */
static int
process_data(ENCODE *ec, SYM *sym, char **pos)
{
	int items, i;
	int error = 0;
	uchar_t *tmp;
	/*
	 * The following buffer must be aligned on a int64_t boundary.
	 */
	uint64_t scratch[(MAXUCHARLEN + sizeof (int64_t) - 1) /
	    sizeof (int64_t)];

	if (ec->len != 0) {
		/* replace current setting */
		free(ec->data);
		ec->len = 0;
	}
	switch (sym->type) {
	case ASCII:
		if (sym->max)
			ec->len = sym->max;
		else
			ec->len = MAXUCHARLEN;
		(void) get_string(pos, (char *)scratch, &ec->len);
		ec->data = (uchar_t *)smalloc(ec->len);
		(void) memcpy(ec->data, (char *)scratch, ec->len);
		break;
	case OCTET:
		if (sym->max)
			ec->len = sym->max;
		else
			ec->len = MAXUCHARLEN;
		/*
		 * Octets are encoded as 2 digit Hex ascii, thus there are
		 * two ascii chars for each hex digit after conversion. Adjust
		 * Maximum appropriately.
		 */
		ec->len *= 2;

		if ((ec->data = get_octets((uchar_t **)pos,
		    (uchar_t *)&ec->len)) == (uchar_t *)NULL) {
			error = E_BAD_OCTET;
			free(ec->data);
			ec->len = 0;
		}
		break;
	case NUMBER:
		/*
		 * For multi-item values, we keep appending the data,
		 * keeping the count updated until max is hit or we hit
		 * a bad number.
		 */
		items = (sym->max) ? sym->max : MAX_ITEMS;
		tmp = (uchar_t *)scratch;
		(void) memset(tmp, 0, items * sym->gran);
		for (i = 0; i < items; i++) {
			eat_whitespace(pos);
			if (!*pos || **pos == ':' || **pos == '\0')
				break;	/* finished */
			if (get_number(pos, tmp, sym->gran) < 0) {
				ec->data = (uchar_t *)NULL;
				ec->len = 0;
				error = E_BAD_NUMBER;
				break;
			}
#if	defined(_LITTLE_ENDIAN)
			switch (sym->gran) {
			case sizeof (int16_t):
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				*(uint16_t *)tmp = htons(*(uint16_t *)tmp);
				break;
			case sizeof (int32_t):
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				*(uint32_t *)tmp = htonl(*(uint32_t *)tmp);
				break;
			case sizeof (int64_t):
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				*(uint32_t *)&tmp[0] =
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				    htonl(*(uint32_t *)&tmp[0]);
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				*(uint32_t *)&tmp[sizeof (int32_t)] =
	/* LINTED [tmp = scratch, which is aligned on a 32bit boundary] */
				    htonl(*(uint32_t *)&tmp[sizeof (int32_t)]);
				break;
			}
#endif	/* _LITTLE_ENDIAN */
			tmp += sym->gran;
			ec->len += sym->gran;
		}
		if (error == 0) {
			ec->data = (uchar_t *)smalloc(ec->len);
			(void) memcpy(ec->data, scratch, ec->len);
		}
		break;
	case IP:
		/*
		 * for multi-item values, we keep appending the data,
		 * keeping the count updated until max is hit, or we hit
		 * a bad ip address.
		 */
		items = (sym->max) ? sym->max : MAX_ITEMS;
		error = get_addresses(pos, sym, ec, items);
		break;
	}
	if (error == 0) {
		if (sym->code == CD_VENDOR_SPEC)
			ec->code = sym->vend;
		else
			ec->code = sym->code;
	}
	return (error);
}

/*
 * CMU 2.2 routine.
 *
 * Read a string from the buffer indirectly pointed to through "src" and
 * move it into the buffer pointed to by "dest".  A pointer to the maximum
 * allowable length of the string (including null-terminator) is passed as
 * "length".  The actual length of the string which was read is returned in
 * the unsigned integer pointed to by "length".  This value is the same as
 * that which would be returned by applying the strlen() function on the
 * destination string (i.e the terminating null is not counted as a
 * character).  Trailing whitespace is removed from the string.  For
 * convenience, the function returns the new value of "dest".
 *
 * The string is read until the maximum number of characters, an unquoted
 * colon (:), or a null character is read.  The return string in "dest" is
 * null-terminated.
 */
static char *
get_string(char **src, char *dest, uchar_t *length)
{
	int n = 0, len, quoteflag;

	quoteflag = FALSE;
	len = *length - 1;
	while ((n < len) && (**src)) {
		if (!quoteflag && (**src == ':'))
			break;
		if (**src == '"') {
			(*src)++;
			quoteflag = !quoteflag;
			continue;
		}
		if (**src == '\\') {
			(*src)++;
			if (!**src)
				break;
		}
		*dest++ = *(*src)++;
		n++;
	}

	/*
	* Remove that troublesome trailing whitespace. . .
	*/
	while ((n > 0) && isspace(*(char *)(dest - 1))) {
		dest--;
		n--;
	}

	*dest = '\0';
	*length = n;
	return (dest);
}

/*
 * This function adjusts the caller's pointer to point just past the
 * first-encountered colon.  If it runs into a null character, it leaves
 * the pointer pointing to it.
 */
static void
adjust(char **s)
{
	char *t;

	t = *s;
	while (*t && (*t != ':'))
		t++;

	if (*t)
		t++;
	*s = t;
}

/*
 * This function adjusts the caller's pointer to point to the first
 * non-whitespace character.  If it runs into a null character, it leaves
 * the pointer pointing to it.
 */
static void
eat_whitespace(char **s)
{
	char *t;

	t = *s;
	while (*t && isspace(*t))
		t++;
	*s = t;
}

static int
get_addresses(char **src, SYM *sym, ENCODE *ec, int items)
{
	struct in_addr buffer[(MAXUCHARLEN + 1) / sizeof (struct in_addr)];
	struct in_addr *addrbuf;
	uint_t i, m;
	int error = 0;

	/*
	 * Per item
	 */
	addrbuf = &buffer[0];
	for (i = 0; i < items && **src != ':' && **src != '\0'; i++) {
		/* Lower granularity */
		for (m = 0; m < sym->gran; m++) {
			eat_whitespace(src);
			if (!**src) {
				error = E_NOT_ENOUGH_IP;
				break;
			}
			if (prs_inetaddr(src, &addrbuf->s_addr) < 0) {
				error = E_BAD_IPADDR;
				break;
			}
			addrbuf++;
		}
		if (error)
			break;
		if (m != sym->gran) {
			error = E_BAD_IP_GRAN;
			break;
		}
	}
	if (error == 0) {
		ec->len = i * sym->gran * sizeof (struct in_addr);
		ec->data = (uchar_t *)smalloc((uint_t)ec->len);
		(void) memcpy(ec->data, (char *)buffer, ec->len);
	}
	return (error);
}

/*
 * prs_inetaddr(src, result)
 *
 * "src" is a value-result parameter; the pointer it points to is updated
 * to point to the next data position.   "result" points to an uint32_t
 * in which an address is returned.
 *
 * This function parses the IP address string in ASCII "dot notation" pointed
 * to by (*src) and places the result (in network byte order) in the uint32_t
 * pointed to by "result".  For malformed addresses, -1 is returned,
 * (*src) points to the first illegal character, and the uint32_t pointed
 * to by "result" is unchanged.  Successful calls return 0.
 */
static int
prs_inetaddr(char **src, uint32_t *result)
{
	uint32_t value;
	uint32_t parts[4], *pp = parts;
	int n;

	if (!isdigit(**src))
		return (-1);
	for (;;) {
		value = 0L;
		if (get_number(src, (void *)&value, sizeof (uint32_t)) != 0)
			value = 0L;
		if (**src == '.') {
			/*
			 * Internet format:
			 *	a.b.c.d
			 *	a.b.c	(with c treated as 16-bits)
			 *	a.b	(with b treated as 24 bits)
			 */
			if (pp >= parts + 4)
				return (-1);
			*pp++ = value;
			(*src)++;
			continue;
		} else
			break;
	}
	/*
	 * Check for trailing characters.
	 */
	if (**src && !(isspace(**src) || (**src == ':')))
		return (-1);

	*pp++ = value;

	/*
	 * Construct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts;
	switch (n) {
	case 1:				/* a -- 32 bits */
		value = parts[0];
		break;
	case 2:				/* a.b -- 8.24 bits */
		value = (parts[0] << 24) | (parts[1] & 0xFFFFFF);
		break;
	case 3:				/* a.b.c -- 8.8.16 bits */
		value = (parts[0] << 24) | ((parts[1] & 0xFF) << 16) |
		    (parts[2] & 0xFFFF);
		break;
	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		value = (parts[0] << 24) | ((parts[1] & 0xFF) << 16) |
		    ((parts[2] & 0xFF) << 8) | (parts[3] & 0xFF);
		break;
	default:
		return (-1);
	}
	*result = htonl(value);
	return (0);
}

/*
 *  Copy symbol name into buffer. Sym ends up pointing to the end of the
 * token.
 */
static void
get_sym_name(char *buf, char **sym)
{
	int i;

	for (i = 0; i < DHCP_SYMBOL_SIZE; i++) {
		if (**sym == ':' || **sym == '=' || **sym == '@' ||
		    **sym == '\0')
			break;
		*buf++ = *(*sym)++;
	}
	*buf = '\0';
}

static void
print_error_msg(int error, uchar_t index)
{
	switch (error) {
	case E_BAD_IPADDR:
		dhcpmsg(LOG_ERR, "Error processing Internet address \
value(s) for symbol: '%s'\n", sym_list[index].nm);
		break;
	case E_BAD_STRING:
		dhcpmsg(LOG_ERR, "Error processing ASCII string value for \
symbol: '%s'\n", sym_list[index].nm);
		break;
	case E_BAD_OCTET:
		dhcpmsg(LOG_ERR, "Error processing OCTET string value for \
symbol: '%s'\n", sym_list[index].nm);
		break;
	case E_BAD_NUMBER:
		dhcpmsg(LOG_ERR, "Error processing NUMBER value for \
symbol: '%s'\n", sym_list[index].nm);
		break;
	case E_BAD_BOOLEAN:
		dhcpmsg(LOG_ERR,
		    "Error processing BOOLEAN value for symbol: '%s'\n",
		    sym_list[index].nm);
		break;
	case E_SYNTAX_ERROR:
	/* FALLTHRU */
	default:
		dhcpmsg(LOG_ERR,
		    "Syntax error found processing value for symbol: '%s'\n",
		    sym_list[index].nm);
		break;
	}
}

/*
 * Define new symbols for things like site-wide and vendor options.
 */
static int
define_symbol(char **ptr, char *symname)
{
	int	i, extend = 0;
	char	*tmp, *cp;
	uchar_t		code;
	SYM		newsym;

	if ((int)strlen(symname) > (int)DHCP_SYMBOL_SIZE) {
		dhcpmsg(LOG_ERR,
		    "Symbol name: '%s' too long. Limit: %d characters.\n",
		    symname, DHCP_SYMBOL_SIZE);
		return (E_SYNTAX_ERROR);
	}

	/*
	 * Only permit new symbol definitions, not old ones. I suppose we
	 * could allow the administrator to redefine symbols, but what if
	 * they redefine subnetmask to be a new brownie recipe? Let's stay
	 * out of that rat hole for now.
	 */
	for (i = 0; i < sym_num_items; i++) {
		if (strcmp(symname, sym_head[i].nm) == 0) {
			dhcpmsg(LOG_ERR, "Symbol: %s already defined. New \
definition ignored.\n",
			    symname);
			adjust(ptr);
			return (0);
		}
	}

	(void) memset(&newsym, 0, sizeof (newsym));
	(void) strcpy(newsym.nm, symname);

	eat_whitespace(ptr);

	/* site/vendor */
	if ((tmp = strchr(*ptr, ',')) == NULL)
		return (E_SYNTAX_ERROR);

	*tmp = '\0';
	if (strncmp(*ptr, "Vendor", 6) == 0) {
		newsym.code = CD_VENDOR_SPEC;
		*ptr = &(*ptr)[6];
		eat_whitespace(ptr);

		if (**ptr != '=') {
			dhcpmsg(LOG_ERR, "Missing client class string from \
vendor symbol definition: '%s'\n",
			    symname);
			return (E_SYNTAX_ERROR);
		}

		++(*ptr);

		if (strlen(*ptr) > DHCP_MAX_CLASS_SIZE) {
			dhcpmsg(LOG_ERR, "Client class is too long for \
vendor symbol: '%s'. Must be less than: %d\n",
			    symname, DHCP_MAX_CLASS_SIZE);
			return (E_SYNTAX_ERROR);
		}

		/*
		 * Process each client class in turn..
		 */
		newsym.class = (char **)NULL;
		for (i = 0, cp = (char *)strtok(*ptr, " \t\n");
		    cp != NULL && (newsym.class = (char **)realloc(
		    newsym.class, sizeof (char **) * (i + 1))) != NULL;
		    cp = (char *)strtok(NULL, " \t\n"), i++) {
			newsym.class[i] = NULL;
			if (strlen(cp) > DHCP_CLASS_SIZE) {
				dhcpmsg(LOG_ERR, "Client class '%s' is too "
				    "long for vendor symbol: '%s'. Must be "
				    "less than: %d\n", cp, symname,
				    DHCP_CLASS_SIZE);
				for (i = 0; newsym.class[i] != (char *)NULL;
				    i++) {
					free(newsym.class[i]);
				}
				free(newsym.class);
				return (E_SYNTAX_ERROR);
			} else
				newsym.class[i] = strdup(cp);
		}

		/*
		 * Terminate with null entry
		 */
		newsym.class = (char **)realloc(newsym.class,
		    sizeof (char **) * (i + 1));
		if (newsym.class != (char **)NULL)
			newsym.class[i] = (char *)NULL;
		else {
			dhcpmsg(LOG_ERR,
			    "Ran out of memory processing symbol: '%s'\n",
			    symname);
			for (i = 0; newsym.class[i] != (char *)NULL; i++)
				free(newsym.class[i]);
			free(newsym.class);
			return (E_SYNTAX_ERROR);
		}
		*ptr = tmp + 1;
	} else if (strcmp(*ptr, "Site") == 0) {
		*ptr = tmp + 1;
	} else if (strcmp(*ptr, "Extend") == 0) {
		extend = 1;
		*ptr = tmp + 1;
	} else {
		dhcpmsg(LOG_ERR, "Missing/Incorrect Site/Vendor flag in \
symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}

	/*
	 * Do code.
	 */
	eat_whitespace(ptr);

	if (!isdigit(**ptr)) {
		dhcpmsg(LOG_ERR,
		    "Missing code digit in symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	code = 0;
	if (get_number(ptr, (void *)&code, 1) < 0 || code == 0 ||
	    code >= CD_END) {
		dhcpmsg(LOG_ERR,
		    "Bad code digit: %d in symbol definition: '%s'\n",
		    code, symname);
		return (E_SYNTAX_ERROR);
	}
	if (newsym.code == CD_VENDOR_SPEC)
		newsym.vend = code;
	else {
		if (!extend) {
			if (code < DHCP_SITE_OPT || code >= CD_END) {
				dhcpmsg(LOG_ERR,
"Out of range (%d-%d) site option code: %d in symbol definition: '%s'\n",
				    DHCP_SITE_OPT, CD_END - 1, code, symname);
				return (E_SYNTAX_ERROR);
			}
		} else {
			if (code <= DHCP_LAST_STD || code >= DHCP_SITE_OPT) {
				dhcpmsg(LOG_ERR,
"Out of range (%d-%d) DHCP extend option code: %d in symbol definition: '%s'\n",
				    DHCP_LAST_STD + 1, DHCP_SITE_OPT - 1,
				    code, symname);
				return (E_SYNTAX_ERROR);
			}
		}
		newsym.code = code;
	}

	(*ptr)++;	/* step over , */
	/*
	 * Value type.
	 */
	if ((tmp = strchr(*ptr, ',')) == NULL) {
		dhcpmsg(LOG_ERR, "Missing value descriptor in symbol \
definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	*tmp = '\0';
	if (strcmp(*ptr, "ASCII") == 0) {
		newsym.type = ASCII;
	} else if (strcmp(*ptr, "OCTET") == 0) {
		newsym.type = OCTET;
	} else if (strcmp(*ptr, "IP") == 0) {
		newsym.type = IP;
	} else if (strcmp(*ptr, "NUMBER") == 0) {
		newsym.type = NUMBER;
	} else if (strcmp(*ptr, "BOOL") == 0) {
		newsym.type = BOOL;
	} else {
		dhcpmsg(LOG_ERR, "Unrecognized value descriptor: %1$s in \
symbol definition: '%s'\n",
		    *ptr, symname);
		return (E_SYNTAX_ERROR);
	}
	*ptr = tmp + 1;

	/*
	 * Granularity
	 */
	eat_whitespace(ptr);

	if (!isdigit(**ptr)) {
		dhcpmsg(LOG_ERR,
		    "Missing value granularity in symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	code = 0;
	if (get_number(ptr, (void *)&code, 1) < 0) {
		dhcpmsg(LOG_ERR,
		    "Bad value granularity in symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	newsym.gran = code;

	(*ptr)++;
	/*
	 * Maximum number of items of granularity.
	 */
	eat_whitespace(ptr);

	if (!isdigit(**ptr)) {
		dhcpmsg(LOG_ERR,
		    "Item maximum is missing in symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	code = 0;
	if (get_number(ptr, (void *)&code, 1) < 0) {
		dhcpmsg(LOG_ERR,
		    "Bad item maximum in symbol definition: '%s'\n",
		    symname);
		return (E_SYNTAX_ERROR);
	}
	newsym.max = code;
	(*ptr)++;

#ifdef	DEBUG
	dhcpmsg(LOG_DEBUG,
	    "Symbol: '%s' is defined to have these characteristics:\n",
	    newsym.nm);
	dhcpmsg(LOG_DEBUG,
	    "Code: %d, Vend: %d, Type: %d, Gran: %d, max: %d\n",
	    newsym.code, newsym.vend, newsym.type, newsym.gran, newsym.max);
	if (newsym.vend != 0) {
		dhcpmsg(LOG_DEBUG, "Client Classes:\n");
		for (i = 0; newsym.class[i] != NULL; i++) {
			dhcpmsg(LOG_DEBUG, "\t%s\n",
			    newsym.class[i]);
		}
	}
#endif	/* DEBUG */

	/*
	 * Now add it to the existing definitions.
	 */
	if (sym_head == &sym_list[0]) {
		/*
		 * No dynamic symbol list yet. make one.
		 */
		sym_head = (SYM *)malloc(sizeof (sym_list) + sizeof (SYM));
		if (sym_head != NULL) {
			(void) memcpy(sym_head, &sym_list[0],
			    sizeof (sym_list));
		}
	} else {
		/*
		 * A current dynamic symbol list. Realloc it.
		 */
		sym_head = (SYM *)realloc(sym_head,
		    sym_num_items * sizeof (SYM) + sizeof (SYM));
	}
	if (sym_head != (SYM *)NULL) {
		sym_num_items++;
		(void) memcpy(&sym_head[sym_num_items - 1], &newsym,
		    sizeof (SYM));
	} else {
		dhcpmsg(LOG_ERR,
		    "Cannot extend symbol table, using predefined table.\n");
		sym_num_items = sizeof (sym_list) / sizeof (SYM);
		if (sym_head && sym_head != &sym_list[0])
			free(sym_head);	/* realloc fails */
		sym_head = &sym_list[0];
	}
	return (0);
}
