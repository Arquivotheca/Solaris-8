#ident	"@(#)main.c	1.14	99/08/09 SMI"

/*
 * Copyright (c) 1996, 1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcdata.h>
#include "msg.h"
#include <locale.h>

#define	DT_OPT_ADD		1
#define	DT_OPT_CREAT		2
#define	DT_OPT_DELETE		3
#define	DT_OPT_MODIFY		4
#define	DT_OPT_DESTROY		5
#define	DT_OPT_DISPLAY		6

#ifndef	TRUE
#define	TRUE	1
#endif	/* TRUE */
#ifndef	FALSE
#define	FALSE	0
#endif	/* FALSE */

extern int optind, opterr;
extern char *optarg;
extern char *disp_err(int);

static int edit_macro(char *, char *, char *, int, int *);
static int verify_symbol(char *);
static int verify_macro(char *);
static int verify_string(char *);
static char *keyval(char *);

int
main(int argc, char *argv[])
{
	register int	major, c, i, err;
	register int	pflag, sflag, dflag, eflag, mflag, nflag;
	int		ns = TBL_NS_DFLT;
	char		path[MAXPATHLEN];
	char		defbuf[DT_MAX_CMD_LEN * 3];
	char		editbuf[DT_MAX_CMD_LEN * 3];
	char		macro[DT_MAX_MACRO_LEN];
	char		newmacro[DT_MAX_MACRO_LEN];
	char		symbuf[DT_MAX_SYMBOL_LEN + 1];
	char		namebuf[MAXPATHLEN], dombuf[MAXPATHLEN];
	char		*domp = NULL, *namep = NULL, *pathp = NULL, *keyp;
	char		*Type;
	char		type[2];
	int		tbl_err;
	Tbl_stat	*tblstatp;
	Tbl		tbl;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif	/* ! TEXT_DOMAIN */

	(void) memset((char *)&tbl, 0, sizeof (tbl));

	type[1] = '\0';
	major = pflag = sflag = dflag = eflag = mflag = nflag = 0;
	while ((c = getopt(argc, argv, "ACDIMPRr:p:s:m:n:e:d:")) != -1) {
		switch (c) {
		case 'A':
			/* Add an entry */
			major = DT_OPT_ADD;
			break;
		case 'C':
			/* Create an empty dhcptab */
			major = DT_OPT_CREAT;
			break;
		case 'D':
			/* Delete an entry */
			major = DT_OPT_DELETE;
			break;
		case 'M':
			/* Modify an entry */
			major = DT_OPT_MODIFY;
			break;
		case 'P':
			/* Print the contents of the dhcptab */
			major = DT_OPT_DISPLAY;
			break;
		case 'R':
			/* Remove the dhcptab */
			major = DT_OPT_DESTROY;
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
				(void) fprintf(stderr, gettext(MSG_RESRC_2LONG),
				    MAXPATHLEN);
				return (DD_CRITICAL);
			} else
				(void) strcpy(path, optarg);
			break;
		case 's':
			/* symbol name */
			sflag = 1;
			if (strlen(optarg) > DT_MAX_SYMBOL_LEN) {
				(void) fprintf(stderr, gettext(MSG_SYM_2LONG),
				    DT_MAX_SYMBOL_LEN);
				return (DD_WARNING);
			} else {
				(void) strcpy(symbuf, optarg);
				type[0] = DT_DHCP_SYMBOL;
				keyp = symbuf;
			}
			break;
		case 'd':
			/* definition */
			dflag = 1;
			if (strlen(optarg) > (DT_MAX_CMD_LEN)) {
				(void) fprintf(stderr, gettext(MSG_DEF_2LONG),
				    DT_MAX_CMD_LEN);
				return (DD_WARNING);
			} else
				(void) strcpy(defbuf, optarg);
			break;
		case 'e':
			/* symbol value pair */
			eflag = 1;
			if (strlen(optarg) > DT_MAX_CMD_LEN) {
				(void) fprintf(stderr,
				    gettext(MSG_SYMVAL_2LONG), DT_MAX_CMD_LEN);
				return (DD_WARNING);
			} else
				(void) strcpy(editbuf, optarg);
			break;
		case 'm':
			/* macro name */
			mflag = 1;
			if (strlen(optarg) > DT_MAX_MACRO_LEN) {
				(void) fprintf(stderr, gettext(MSG_MACRO_2LONG),
				    DT_MAX_MACRO_LEN);
				return (DD_WARNING);
			} else {
				(void) strcpy(macro, optarg);
				keyp = macro;
				type[0] = DT_DHCP_MACRO;
			}
			break;
		case 'n':
			/* new macro name */
			nflag = 1;
			if (strlen(optarg) > DT_MAX_MACRO_LEN) {
				(void) fprintf(stderr, gettext(MSG_NMAC_2LONG),
				    DT_MAX_MACRO_LEN);
				return (DD_WARNING);
			} else
				(void) strcpy(newmacro, optarg);
			break;

		default:
			(void) fprintf(stderr, gettext(MSG_USAGE));
			return (DD_CRITICAL);
		}
	}

	if (pflag) {
		if (ns == TBL_NS_DFLT) {
			ns = dd_ns(&tbl_err, &pathp);
			if (ns == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_UNKN_RESRC_TYPE));
				return (DD_CRITICAL);
			}
		}
		if (ns == TBL_NS_UFS) {
			(void) sprintf(namebuf, "%s/dhcptab", path);
			namep = namebuf;
		} else {
			(void) strcpy(dombuf, path);
			domp = dombuf;
		}
	}

	/*
	 * Validate options
	 */

	if (dflag) {
		if (sflag) {
			if (verify_symbol(defbuf) != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_SYM_DEF_ERR));
				return (DD_WARNING);
			}
		}
		if (mflag) {
			if (verify_macro(defbuf) != 0) {
				(void) fprintf(stderr,
				    gettext(MSG_MAC_DEF_ERR));
				return (DD_WARNING);
			}
		}
	}
	switch (major) {
	case DT_OPT_ADD:
		if (dflag && !(nflag || eflag) && ((sflag + mflag) == 1)) {
			err = add_dd_entry(TBL_DHCPTAB, ns, namep, domp,
			    &tbl_err, keyp, type, defbuf);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr, gettext(MSG_ADD_FAILED),
				    disp_err(tbl_err));
				if (tbl_err == TBL_ENTRY_EXISTS)
					err = DD_EEXISTS;
				else
					err = DD_WARNING;
			} else
				err = DD_SUCCESS;
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	case DT_OPT_CREAT:
		if (!(dflag || nflag || eflag || sflag || mflag)) {
			if (stat_dd(TBL_DHCPTAB, ns, namep, domp,
			    &tbl_err, &tblstatp) == TBL_SUCCESS) {
				(void) fprintf(stderr, gettext(MSG_DT_EXISTS),
				    disp_err(tbl_err));
				free_dd_stat(tblstatp);
				err = DD_EEXISTS;
				break;
			}
			if (make_dd(TBL_DHCPTAB, ns, namep, domp, &tbl_err,
			    NULL, NULL) != TBL_SUCCESS) {
				(void) fprintf(stderr,
				    gettext(MSG_CREAT_FAILED),
				    disp_err(tbl_err));
				err = DD_WARNING;
			} else
				err = DD_SUCCESS;
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	case DT_OPT_DELETE:
		if (!(dflag || nflag || eflag) && ((sflag + mflag) == 1)) {
			err = rm_dd_entry(TBL_DHCPTAB, ns, namep, domp,
			    &tbl_err, keyp, type);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr, gettext(MSG_DEL_FAILED),
				    disp_err(tbl_err));
				if (tbl_err == TBL_NO_ENTRY)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
			} else
				err = DD_SUCCESS;
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	case DT_OPT_MODIFY:
		if ((sflag && !mflag) && ((nflag + dflag) == 1)) {
			/* Modify symbol */
			if (nflag) {
				/* Find the existing entry */
				err = list_dd(TBL_DHCPTAB, ns, namep, domp,
				    &tbl_err, &tbl, keyp, type);
				if (err == TBL_FAILURE) {
					(void) fprintf(stderr,
					    gettext(MSG_MOD_SYM_NOEXIST),
					    keyp, disp_err(tbl_err));
					if (tbl_err == TBL_NO_ENTRY)
						err = DD_ENOENT;
					else
						err = DD_WARNING;
					break;
				}
				/* assume only one row */
				err = mod_dd_entry(TBL_DHCPTAB, ns, namep, domp,
				    &tbl_err, keyp, type, newmacro, type,
				    tbl.ra[0]->ca[2]);
				free_dd(&tbl);
			} else {
				err = mod_dd_entry(TBL_DHCPTAB, ns, namep, domp,
				    &tbl_err, keyp, type, keyp, type, defbuf);
			}
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_MODSYM_FAILED),
				    disp_err(tbl_err));
				if (tbl_err == TBL_NO_ENTRY)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
		} else if ((!sflag && mflag) &&
		    ((nflag + dflag + eflag) == 1)) {
			/* Find the existing entry */
			err = list_dd(TBL_DHCPTAB, ns, namep, domp, &tbl_err,
			    &tbl, keyp, type);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr,
				    gettext(MSG_MODMAC_NOEXIST),
				    keyp, disp_err(tbl_err));
				if (tbl_err == TBL_NO_ENTRY)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
			/* Modify macro */
			if (nflag) {
				/* assume only one row */
				err = mod_dd_entry(TBL_DHCPTAB, ns, namep, domp,
				    &tbl_err, keyp, type, newmacro, type,
				    tbl.ra[0]->ca[2]);
			} else {
				if (eflag) {
					if (edit_macro(tbl.ra[0]->ca[2],
					    editbuf, defbuf, DT_MAX_CMD_LEN * 3,
					    &tbl_err) != 0) {
						(void) fprintf(stderr,
						    gettext(MSG_MODMAC_FAILED),
						    editbuf, disp_err(tbl_err));
						if (tbl_err == TBL_NO_ENTRY)
							err = DD_ENOENT;
						else
							err = DD_WARNING;
						free_dd(&tbl);
						break;
					}
				}
				err = mod_dd_entry(TBL_DHCPTAB, ns, namep, domp,
				    &tbl_err, keyp, type, keyp, type, defbuf);
			}
			free_dd(&tbl);
			if (err == TBL_FAILURE) {
				(void) fprintf(stderr, gettext(MSG_MOD_FAILED),
				    disp_err(tbl_err));
				if (tbl_err == TBL_NO_ENTRY)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	case DT_OPT_DESTROY:
		if (!(dflag || nflag || eflag || sflag || mflag)) {
			tblstatp = NULL;
			if (stat_dd(TBL_DHCPTAB, ns, namep, domp,
			    &tbl_err, &tblstatp) != TBL_SUCCESS ||
			    check_dd_access(tblstatp, &tbl_err) != 0) {
				(void) fprintf(stderr, gettext(MSG_RM_FAILED),
				    disp_err(tbl_err));
				if (tblstatp != NULL)
					free_dd_stat(tblstatp);
				err = tbl_err;
				if (tbl_err == TBL_NO_TABLE)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
			free_dd_stat(tblstatp);
			err = del_dd(TBL_DHCPTAB, ns, namep, domp, &tbl_err);
			if (err == TBL_SUCCESS)
				err = DD_SUCCESS;
			else {
				(void) fprintf(stderr, gettext(MSG_RM_FAILED),
				    disp_err(tbl_err));
				err = DD_WARNING;
			}
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	case DT_OPT_DISPLAY:
		if (!(dflag || nflag || eflag || sflag || mflag)) {
			tblstatp = NULL;
			if (stat_dd(TBL_DHCPTAB, ns, namep, domp,
			    &tbl_err, &tblstatp) != TBL_SUCCESS) {
				(void) fprintf(stderr, gettext(MSG_DISP_FAILED),
				    disp_err(tbl_err));
				if (tblstatp != NULL)
					free_dd_stat(tblstatp);
				err = tbl_err;
				if (tbl_err == TBL_NO_TABLE)
					err = DD_ENOENT;
				else
					err = DD_WARNING;
				break;
			}
			free_dd_stat(tblstatp);
			err = list_dd(TBL_DHCPTAB, ns, namep, domp, &tbl_err,
			    &tbl, NULL, NULL);
			if (err == TBL_SUCCESS) {
				(void) fprintf(stdout, "%-20s\t%-8s\t%s\n",
				    gettext("Name"), gettext("Type"),
				    gettext("Value"));
				(void) fprintf(stdout,
"==================================================\n");
				for (i = tbl.rows - 1; i >= 0; i--) {
					if (*(tbl.ra[i]->ca[1]) ==
					    DT_DHCP_SYMBOL)
						Type = gettext("Symbol");
					else
						Type = gettext("Macro");

					(void) fprintf(stdout,
					    "%-20s\t%-8s\t%s\n",
					    tbl.ra[i]->ca[0], Type,
					    tbl.ra[i]->ca[2]);
				}
				free_dd(&tbl);
			}
		} else {
			(void) fprintf(stderr, gettext(MSG_USAGE));
			err = DD_CRITICAL;
		}
		break;
	default:
		(void) fprintf(stderr, gettext(MSG_USAGE));
		err = DD_CRITICAL;
		break;
	}

	return (err);
}

/*
 * Verify that the symbol definition handed in is valid.
 *
 * Symbol definitions are of the form: Context,Code,Type,Granularity,Maximum
 *
 * Context can be Vendor=String or Site.
 *
 * Code is a decimal digit from 128-254 for Site, or 1-254 for Vendor.
 *
 * Type can be ASCII, IP, NUMBER, BOOLEAN, OCTET.
 *
 * Granularity is a decimal digit.
 *
 * Maximum is a decimal digit.
 *
 * Embedded newlines are not permitted.
 *
 * Returns 0 for success, 1 otherwise.
 */
static int
verify_symbol(char *symp)
{
	register int context, code, type, gran, max, err = EINVAL, count;
	register char c, *tp, *wp;

	if (symp == NULL)
		return (EINVAL);

	if ((tp = (char *)malloc(strlen(symp) + 1)) == NULL)
		return (ENOMEM);

	(void) strcpy(tp, symp);

	context = code = type = gran = max = count = 0;
	for (wp = strtok(tp, ","); wp != NULL; wp = strtok(NULL, ",")) {
		if (count == 0 && context == 0) {
			if (strncmp(wp, DT_CONTEXT_EXTEND,
			    strlen(DT_CONTEXT_EXTEND)) == 0 ||
			    strncmp(wp, DT_CONTEXT_SITE,
			    strlen(DT_CONTEXT_SITE)) == 0 ||
			    strncmp(wp, DT_CONTEXT_VEND,
			    strlen(DT_CONTEXT_VEND)) == 0) {
				count++;
				context = 1;
				c = *wp;
				continue;
			} else {
				(void) fprintf(stderr, gettext(MSG_BAD_CNTXT));
				break;
			}
		}
		if (count == 1 && code == 0) {
			if (!isdigit(*wp)) {
				(void) fprintf(stderr, gettext(MSG_BAD_CODE));
				break;
			}
			code = atoi(wp);
			if ((c == 'E' && (code > DHCP_LAST_STD &&
			    code < DHCP_SITE_OPT)) ||
			    (c == 'V' && (code > CD_PAD && code < CD_END)) ||
			    (c == 'S' && (code > (DHCP_SITE_OPT - 1) &&
			    code < CD_END))) {
				count++;
				continue;
			} else {
				switch (c) {
				case 'E':
					(void) fprintf(stderr,
					    gettext(MSG_BAD_EXTEND),
					    DHCP_LAST_STD + 1);
					break;
				case 'S':
					(void) fprintf(stderr,
					    gettext(MSG_BAD_SITE));
					break;
				case 'V':
					(void) fprintf(stderr,
					    gettext(MSG_BAD_VEND));
					break;
				}
				code = 0;
				break;
			}
		}
		if (count == 2 && type == 0) {
			if (strcmp(wp, DT_ASCII) == 0 ||
			    strcmp(wp, DT_IP) == 0 ||
			    strcmp(wp, DT_BOOL) == 0 ||
			    strcmp(wp, DT_NUM) == 0 ||
			    strcmp(wp, DT_OCTET) == 0) {
				count++;
				type = 1;
				continue;
			} else {
				(void) fprintf(stderr, gettext(MSG_BAD_VAL),
				    DT_ASCII, DT_IP, DT_BOOL, DT_NUM, DT_OCTET);
				break;
			}
		}
		if (count == 3 && gran == 0) {
			if (!isdigit(*wp) || (gran = atoi(wp)) < 0) {
				(void) fprintf(stderr, gettext(MSG_BAD_GRAN));
				break;
			} else {
				gran = 1;
				count++;
				continue;
			}
		}
		if (count == 4 && max == 0) {
			if (!isdigit(*wp) || (max = atoi(wp)) < 0) {
				(void) fprintf(stderr, gettext(MSG_BAD_MAX));
				break;
			} else {
				max = 1;
				err = 0;
				count++;
				continue;
			}
		}
	}
	free(tp);
	if (err == 0 && count == 5 && context && code && type && gran && max)
		return (0);
	return (err);
}

/*
 * Verify that the macro definition handed in is valid.
 *
 * Macro definitions are of the form: :symname=value:
 * values can be quoted strings. No checking if symname is a valid symbol is
 * performed. Embedded newlines are not permitted in strings unless they are
 * escaped with a backslash.
 *
 * Returns 0 for success, 1 otherwise.
 */
static int
verify_macro(char *mp)
{
	register int err = 0;
	register char *tp, *cp, *wp;

	if (mp == NULL || (*mp != ':' || mp[strlen(mp) - 1] != ':') ||
	    strlen(mp) <= 2)
		return (EINVAL);

	if ((tp = (char *)malloc(strlen(mp) + 1)) == NULL)
		return (ENOMEM);

	(void) strcpy(tp, mp);
	for (wp = keyval(tp), cp = wp; wp != NULL; wp = keyval(NULL)) {
		if (cp > wp)
			continue;
		if ((cp = strchr(wp, '=')) != NULL) {
			*cp++ = '\0';
			if (*cp == '"') {
				if (verify_string(cp) != 0) {
					(void) fprintf(stderr,
					    gettext(MSG_BAD_STRING));
					err = EINVAL;
					break;
				}
				/* skip any colons in the string. */
				do {
					cp++;
				} while (*cp != '\0' && *cp != '"');
				if (*cp == '\0') {
					(void) fprintf(stderr,
					    gettext(MSG_NO_QUOTE));
					err = EINVAL;
					break;
				} else
					cp++;
			}
		}
		if (strlen(wp) > DT_MAX_SYMBOL_LEN) {
			(void) fprintf(stderr, gettext(MSG_SYM_2BIG), wp,
			    DT_MAX_SYMBOL_LEN);
			err = E2BIG;
			break;
		}
	}
	free(tp);
	return (err);
}

/*
 * Scan macro definition, and either Add, delete, or modify the specified
 * symbol's definition.
 * Returns 0 for success, nonzero otherwise.
 */
static int
edit_macro(char *oldp, char *cmdp, char *bufp, int buflen, int *errp)
{
	register int found = FALSE;
	register char *ap, *bp, *ep, *sp, *tp, *vp;

	if ((strlen(oldp) + strlen(cmdp)) > buflen) {
		*errp = TBL_TOO_BIG;
		return (1);
	}
	if ((tp = strchr(cmdp, '=')) == NULL) {
		(void) fprintf(stderr, gettext(MSG_BAD_SYM_FORM), cmdp);
		*errp = TBL_BAD_SYNTAX;
		return (1);
	} else {
		sp = cmdp;
		*tp++ = '\0';
		vp = tp;
	}
	if (*vp == '"' && verify_string(vp) != 0) {
		(void) fprintf(stderr, gettext(MSG_BAD_STRING));
		*errp = TBL_BAD_SYNTAX;
		return (1);
	}

	for (ap = keyval(oldp), bp = bufp; ap != NULL; ap = keyval(NULL)) {
		ep = strchr(ap, '=');
		if (ep != NULL)
			*ep++ = '\0';
		if (strcmp(sp, ap) == 0) {
			found = TRUE;
			if (*vp == '\0')
				continue;
			else {
				if (strcmp(vp, DT_NO_VALUE) == 0)
					(void) sprintf(bp, ":%s", sp);
				else
					(void) sprintf(bp, ":%s=%s", sp, vp);
			}
		} else {
			if (ep != NULL) {
				--ep;
				*ep = '=';
			}
			(void) sprintf(bp, ":%s", ap);
		}
		bp += strlen(bp);
	}
	if (bp == bufp) {
		*errp = TBL_NO_ENTRY;
		return (1);
	} else {
		if (!found) {
			if (*vp != '\0') {
				if (strcmp(vp, DT_NO_VALUE) == 0)
					(void) sprintf(bp, ":%s", sp);
				else
					(void) sprintf(bp, ":%s=%s", sp, vp);
				bp += strlen(bp);
			}
		}
		*bp = ':';
	}
	return (0);
}

/*
 * Verifies a string value: Must be surrounded by double quotes, and any
 * newline characters must be escaped with backslashes.
 *
 * Returns 0 if string is verified, nonzero otherwise.
 */
static int
verify_string(char *sp)
{
	char	*tp;

	if (*sp != '"' && sp[strlen(sp) - 1] != '"')
		return (1);
	for (tp = sp; *tp != '\0'; tp++) {
		if (*tp == '\n' && *(tp - 1) != '\\')
			return (1);
	}
	return (0);
}

/*
 * like strtok, only hardwired for colons (:), and deals with quoted strings
 * correctly.
 */
static char *
keyval(char *argp)
{
	register char	*ap, *bp;
	register int	quote;
	static char	*location = NULL;

	if (argp != NULL)
		ap = bp = argp;
	else
		ap = bp = location;

	if (ap != NULL && *ap == ':') {
		ap++;
		bp = ap;
	}

	for (quote = FALSE; bp != NULL && *bp != '\0'; bp++) {
		if (*bp == '"' || quote) {
			if (*bp == '"' && quote)
				quote = FALSE;
			else
				quote = TRUE;
		} else {
			if (*bp == ':') {
				*bp++ = '\0';
				if (*bp != '\0')
					location = bp;
				else
					location = NULL;
				return (ap);
			}
		}
	}
	return (NULL);
}
