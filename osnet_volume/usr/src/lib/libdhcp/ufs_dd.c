/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ufs_dd.c	1.37	99/08/27 SMI"

/*
 * This module contains the basic functions for manipulating tables stored
 * in the filesystem.  Make, delete, stat and list tables; add, modify and
 * remove entries are included, along with common support functions for
 * locking & parsing.
 */

#include <unistd.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <dd_impl.h>

static u_int tbl_type;

/*
 * Function to piece together the complete table name.  If the name passed
 * contains a '/' assume that it's meant to force us into a very specific
 * location, so just spit it back.
 */
static void
build_tbl_name(char *name, char *prefix, char *suffix, char *buf)
{
	if (strchr(name, '/') == NULL) {
		(void) sprintf(buf, "%s%s%s", prefix, name, suffix);
	} else {
		(void) strcpy(buf, name);
	}
}

/* ARGSUSED */
static char *
get_db_line(
	char *buff,			/* Where to stuff the line */
	int buffsize,			/* Size of buffer */
	FILE *fp,			/* File to read from */
	struct tbl_trans_data *ttp)	/* Translation/format structure */
{
	char	*cp = buff;
	int	l;
	char	*status;
	long	pos = -1;

	while ((status = fgets(cp, (buffsize - (cp - buff)), fp)) != NULL) {
		if (((l = strlen(cp)) < 2) || ((cp[l - 2]) != '\\'))
			break;
		else
			cp += l - 2;
	}
	if ((status == NULL) && (pos != -1)) {
		(void) fseek(fp, pos, SEEK_SET);
		(void) strcpy(cp, "\n");
		status = cp;
	}
	return (status);
}

/*
 * Function to retrieve a token from a line; similar to strtok but considers
 * multiple consecutive non-whitespace separators to define empty columns
 * rather than being a single separator.  Also strips leading whitespace.
 */

#define	WHITESPACE	" \t\r\n\f\v"
#define	WHITESPACELEN	sizeof (WHITESPACE)

static char *
gettok(char *buff, char *span)
{
	static char	*cp = "";
	char		*rp;
	int		len;

	if (buff != NULL)
		cp = buff;

	cp += strspn(cp, WHITESPACE);	/* Get past leading whitespace */
	if (*cp == '\0')
		return (NULL);

	len = strcspn(cp, span);	/* Find next separator */
	rp = cp;
	cp += len;
	if (*cp != '\0') {
		*cp = '\0';
		++cp;
	}
	return (rp);
}

/*
 * Function to parse a line into a series of columns.  Handles the special
 * comment separators used by most of these tables.
 */
static int
_parse_db_buffer(char *buff, Row **rp, struct tbl_fmt *tfp,
    struct tbl_trans_data *ttp)
{
	ushort_t	colnum = 0;
	char		*t, *tok;
	char		*cp;
	int		l;
	char		*column_sep = ttp->column_sep;
	char		*comment_sep = ttp->comment_sep;

	/*
	 * Don't try parsing empty lines
	 */
	if ((l = strlen(buff)) == 0)
		return (0);

	if ((*rp = _dd_new_row()) == NULL)
		return (-ENOMEM);

	/*
	 * Duplicate for working copy and remove trailing newline
	 */
	cp = strdup(buff);
	if (cp[l - 1] == '\n')
		cp[l - 1] = '\0';

	/*
	 * Search for comment if a separator was given.
	 */
	if ((comment_sep != NULL) &&
	    ((t = strstr(cp, comment_sep)) != NULL)) {
		if ((tfp->comment_col >= 0) &&
		    (((*rp)->ca[tfp->cfmts[tfp->comment_col].argno] =
		    strdup((t+strlen(comment_sep)))) == NULL)) {
			_dd_free_row(*rp);
			(void) free(cp);
			return (-ENOMEM);
		}
		*t = '\0';
	}

	/*
	 * Now break out each column and stick it in a struct.
	 */
	for (tok = gettok(cp, column_sep); tok != NULL;
	    tok = gettok(NULL, column_sep)) {
		if (_dd_set_col_val(*rp, tfp->cfmts[colnum].argno, tok,
		    ttp->alias_sep)) {
			_dd_free_row(*rp);
			(void) free(cp);
			return (-ENOMEM);
		}
		/*
		 * Very specific assumption made about aliases here; they're
		 * either the last thing on the line, or followed only by the
		 * comment, which we took care of above.
		 */
		if (colnum != tfp->alias_col)
			++colnum;
	}
	(void) free(cp);
	return (colnum);
}

/*
 * Function to do a match on two rows.  Each column in searchp is
 * allowed to match a range of columns in entryp.  If each column in searchp
 * which has a value specified matches with a column in entryp from the
 * range specified, then the match is an EXACT_MATCH.  If any columns which have
 * a value specified don't match, while others do, then it is a MIX_MATCH.
 * Failure to match any columns is a NO_MATCH.
 */
static int
_match_entry(Row *searchp, Row *entryp, char **mp, struct tbl_fmt *tfp)
{
	int	s, e;
	char	*sp, *ep;
	int	ret = 0;
	int	status;
	int	cm;
	int	(*fn)();
	char	*tok;
	char	*cp;

	*mp = NULL;
	for (s = 0; s < tfp->cols; ++s) {
		if ((tfp->cfmts[s].argno < TBL_MAX_COLS) &&
		    ((sp = searchp->ca[tfp->cfmts[s].argno]) != NULL)) {
			cm = COL_NOT_MATCHED;
			for (e = tfp->cfmts[s].first_match;
			    e <= tfp->cfmts[s].last_match; ++e) {
				if ((ep = entryp->ca[tfp->cfmts[e].argno])
				    != NULL) {
					if ((tfp->cfmts[s].flags &
					    COL_CASEI))
						fn = strcasecmp;
					else
						fn = strcmp;
					if ((e == tfp->alias_col) &&
					    (strstr(ep, sp) != NULL)) {
						cp = strdup(ep);
						for (tok = gettok(cp,
						    DEFAULT_COLUMN_SEP);
						    tok != NULL;
						    tok = gettok(NULL,
						    DEFAULT_COLUMN_SEP))
							if ((status = (*fn)(sp,
							    tok)) == 0)
								break;
						(void) free(cp);
					} else
						status = (*fn)(sp, ep);
					if (status == 0) {
						*mp = sp;
						cm = COL_MATCHED;
						break;
					}
				}
			}
			ret |= cm;
		}
	}

	return (ret);
}

/*
 * Function to construct a table entry from the data arg list.
 */
/* ARGSUSED */
static int
format_entry(char *buff, int buffsize, char **oa, struct tbl_trans_data *ttp)
{
	char		*cp = buff, *arg;
	int		len;
	int		i, j;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];

	buff[0] = '\0';
	for (i = 0; i < tfp->cols; ++i) {
		j = tfp->cfmts[i].argno;
		if (j == TBL_MAX_COLS)
			arg = "x";
		else
			arg = oa[j];
		if ((arg != NULL) && (len = strlen(arg))) {
			if (i != 0)
				*cp++ = ttp->column_sep[0];
			if (i == tfp->comment_col) {
				if (!isspace(ttp->column_sep[0]))
					--cp;
				*cp++ = ttp->comment_sep[0];
				(void) strcpy(cp, arg);
				cp += len;
			} else {
				(void) strcpy(cp, arg);
				cp += len;
			}
		} else if (!isspace(ttp->column_sep[0]))
			*cp++ = ttp->column_sep[0];
	}
	*cp++ = '\n';
	*cp = '\0';
	return (0);
}

#define	LCK_WAIT_TIME	2	/* Wait 2 seconds for locks */

static void
remove_component(char *path)
{
	char *p;

	p = strrchr(path, '/'); 		/* find last '/' 	*/
	if (p == NULL) {
		*path = '\0';			/* set path to null str	*/
	} else {
		*p = '\0';			/* zap it 		*/
	}
}

static char *
_basename(char *path)
{
	char *p;

	p = strrchr(path, '/');
	if (p == NULL)
		p = path;
	else
		++p;
	return (p);
}

/*
 * Function to traverse a symlink path to find the real file at the end of
 * the rainbow.
 */
static int
trav_link(char **path)
{
	static char	newpath[MAXPATHLEN];
	char		lastpath[MAXPATHLEN];
	int		len;
	char		*tp;

	(void) strcpy(lastpath, *path);
	while ((len = readlink(*path, newpath, sizeof (newpath))) != -1) {
		newpath[len] = '\0';
		if (newpath[0] != '/') {
			tp = strdup(newpath);
			remove_component(lastpath);
			(void) sprintf(newpath, "%s/%s", lastpath, tp);
			(void) free(tp);
		}
		(void) strcpy(lastpath, newpath);
		*path = newpath;
	}

	/*
	 * ENOENT or EINVAL is the normal exit case of the above loop.
	 */
	if (errno == ENOENT || errno == EINVAL)
		return (0);
	return (-1);
}

/*
 * Function to generate a temporary pathname to use.  Based on tempnam(3),
 * which we don't use because we need absolute control to ensure that it's
 * placed in the directory specified so that rename() will work.
 */

static char *seed = "AAA";

char *
_dd_tempfile(const char *dir)
{
	char *buf, *cp;

	buf = (char *)malloc(MAXPATHLEN);
	if (buf == NULL)
		return (NULL);
	(void) strcpy(buf, dir);
	(void) strcat(buf, "/");
	(void) strcat(buf, seed);
	(void) strcat(buf, "XXXXXX");
	for (cp = seed; *cp == 'Z'; *cp++ = 'A');
	if (*cp != '\0')
		++* cp;
	if (*mktemp(buf) == '\0') {
		free(buf);
		return (NULL);
	}
	return (buf);
}

/* ARGSUSED */
static void
almhdlr(int sig)
{
}

/*
 * Function to lock a database against conflicting accesses.  Based on
 * lckpwdf(). Place lock file in same directory as tables.
 */
int
_dd_lock_db(char *db, int type, int *fdp)
{
	int		status;
	char		*lock_path;
	struct flock	flock;
	char		*bp, *dp;
	mode_t		mode, cmask = 0;

	lock_path = (char *)malloc(strlen(db) + 7);
	dp = strdup(db);
	remove_component(dp);
	bp = _basename(db);
	mode = 0666;

	(void) sprintf(lock_path, "%s/.%s.lock", dp, bp);
	free(dp);
	cmask = umask(cmask);
	*fdp = open(lock_path, (O_RDWR | O_CREAT | O_TRUNC), mode);
	cmask = umask(cmask);
	if (*fdp == -1) {
		(void) free(lock_path);
		return (-1);
	} else {
		flock.l_type = type;
		flock.l_whence = flock.l_start = flock.l_len = 0;
		(void) sigset(SIGALRM, almhdlr);
		(void) alarm(LCK_WAIT_TIME);
		status = fcntl(*fdp, F_SETLKW, (int)&flock);
		(void) alarm(0);
		(void) sigset(SIGALRM, SIG_DFL);
		(void) free(lock_path);
		return (status);
	}
}

/*
 * Function to unlock database previously locked with _dd_lock_db
 */
int
_dd_unlock_db(int *fdp)
{
	struct flock flock;

	if (*fdp == -1)
		return (-1);
	else {
		flock.l_type = F_UNLCK;
		flock.l_whence = flock.l_start = flock.l_len = 0;
		(void) fcntl(*fdp, F_SETLK, (int)&flock);
		(void) close(*fdp);
		*fdp = -1;
		return (0);
	}
}

/*
 * Comparison function for use with qsort.  Compares column 0.
 */
int
_dd_compare_ufs_col0(Row **ra, Row **rb)
{
	if (((*ra)->ca[0] == NULL) && ((*rb)->ca[0] == NULL))
		return (0);
	if ((*ra)->ca[0] == NULL)
		return (-1);
	if ((*rb)->ca[0] == NULL)
		return (1);
	if ((_dd_ttd[tbl_type]->fmts[TBL_NS_UFS].cfmts[0].flags & COL_CASEI))
		return (strcasecmp((*ra)->ca[0], (*rb)->ca[0]));
	return (strcmp((*ra)->ca[0], (*rb)->ca[0]));
}

/*
 * Comparison function for use with qsort.  Compares column 1.
 */
int
_dd_compare_ufs_col1(Row **ra, Row **rb)
{
	if (((*ra)->ca[1] == NULL) && ((*rb)->ca[1] == NULL))
		return (0);
	if ((*ra)->ca[1] == NULL)
		return (-1);
	if ((*rb)->ca[1] == NULL)
		return (1);
	if ((_dd_ttd[tbl_type]->fmts[TBL_NS_UFS].cfmts[1].flags & COL_CASEI))
		return (strcasecmp((*ra)->ca[1], (*rb)->ca[1]));
	return (strcmp((*ra)->ca[1], (*rb)->ca[1]));
}

/*
 * Comparison function for qsort for the dhcptab.  Ensures that symbol
 * definitions precede macro names.
 */
int
_dd_compare_ufs_dhcptab(Row **ra, Row **rb)
{
	int as, bs;

	as = ((*ra)->ca[1] != NULL && *((*ra)->ca[1]) == DT_DHCP_SYMBOL);
	bs = ((*rb)->ca[1] != NULL && *((*rb)->ca[1]) == DT_DHCP_SYMBOL);

	if (as == bs) {
		if ((*ra)->ca[0] == NULL && (*rb)->ca[0] == NULL)
			return (0);
		if ((*ra)->ca[0] == NULL)
			return (-1);
		if ((*rb)->ca[0] == NULL)
			return (1);
		return (strcasecmp((*ra)->ca[0], (*rb)->ca[0]));
	} else if (!as) {
		return (1);
	} else if (!bs) {
		return (-1);
	}
	return (0);
}

/*
 * Function to list an entire database.
 */
int
_list_dd_ufs(char *name, int *tbl_err, Tbl *tbl, struct tbl_trans_data *ttp,
    char **args)
{
	FILE		*ifp;
	char		buff[BUFSIZ * 2];
	Row		*rowp, *mr = NULL;
	int		i;
	int		status = 0;
	boolean_t	found = B_FALSE, match_flag = B_FALSE;
	char		*mp;
	int		fd;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, buff);
	if (_dd_lock_db(buff, F_RDLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		return (TBL_FAILURE);
	}
	/*
	 * Process file, line at a time.
	 */
	if ((ifp = fopen(buff, "r")) != NULL) {
		/*
		 * First construct the "match" row, which represents the
		 * criteria we're searching on.  Each row of the table
		 * is compared to it.
		 */
		if ((mr = _dd_new_row()) == NULL) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_MEMORY;
			(void) fclose(ifp);
			(void) _dd_unlock_db(&fd);
			return (TBL_FAILURE);
		}
		for (i = 0; i < tfp->cols; ++i) {
			if ((tfp->cfmts[i].flags & COL_KEY) &&
			    (args[tfp->cfmts[i].m_argno] != NULL) &&
			    strlen(args[tfp->cfmts[i].m_argno])) {
				mr->ca[tfp->cfmts[i].argno] =
				    args[tfp->cfmts[i].m_argno];
				match_flag = B_TRUE;
			}
		}
		/*
		 * For each line that matches the search criteria,
		 * append it to the output tbl structure.
		 */
		while (get_db_line(buff, sizeof (buff), ifp, ttp) != NULL) {
			if ((status = _parse_db_buffer(buff, &rowp, tfp, ttp))
			    > 0) {
				if (match_flag) {
					if (_match_entry(mr, rowp, &mp, tfp) ==
					    EXACT_MATCH) {
						found = B_TRUE;
					} else {
						_dd_free_row(rowp);
						rowp = NULL;
						continue;
					}
				}
				if (_dd_append_row(tbl, rowp)) {
					if (tbl_err != NULL)
						*tbl_err = TBL_NO_MEMORY;
					(void) fclose(ifp);
					(void) _dd_unlock_db(&fd);
					_dd_free_row(rowp);
					(void) free(mr);
					return (TBL_FAILURE);
				}
			} else if (status < 0) {
				/* Row has been freed by _parse_db_buffer() */
				if (tbl_err != NULL)
					*tbl_err = TBL_NO_MEMORY;
				(void) fclose(ifp);
				(void) _dd_unlock_db(&fd);
				(void) free(mr);
				return (TBL_FAILURE);
			} else {
				/* status is 0, empty line or comment line */
				if (rowp != NULL) {
					_dd_free_row(rowp);
					rowp = NULL;
				}
			}
		}
		(void) free(mr);
		if (ferror(ifp)) {
			if (tbl_err != NULL)
				*tbl_err = TBL_READ_ERROR;
			(void) fclose(ifp);
			(void) _dd_unlock_db(&fd);
			return (TBL_FAILURE);
		}
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_NO_TABLE;
		(void) _dd_unlock_db(&fd);
		return (TBL_FAILURE);
	}

	(void) fclose(ifp);
	(void) _dd_unlock_db(&fd);

	if (match_flag && !found) {
		for (i = 0, mp = args[i]; (mp == NULL) || !strlen(mp);
		    mp = args[++i]);
		if (tbl_err != NULL)
			*tbl_err = TBL_NO_ENTRY;
		return (TBL_FAILURE);
	}
	if (tbl->rows > 1)
		qsort(tbl->ra, tbl->rows, sizeof (Row *), tfp->sort_function);

	return (TBL_SUCCESS);
}

/*
 * Macro to do error cleanup of open files in *_dd_ufs
 */
#define	cleanup_files {							\
	(void) fclose(ifp);						\
	(void) _dd_unlock_db(&fd);					\
	(void) fclose(ofp);						\
	(void) unlink(tmp);						\
	(void) free(tmp);						\
	(void) free(mr);						\
	(void) free(mr2);						\
}

/*
 * Macro to do the buffer writes in *_dd_ufs
 */

#define	write_buff {							\
	if (fputs(buff, ofp) == EOF) {					\
		if (tbl_err != NULL)					\
			*tbl_err = TBL_WRITE_ERROR;			\
		cleanup_files;						\
		return (TBL_FAILURE);					\
	}								\
}

/*
 * Function to add an entry to a table in the filesystem.
 */

int
_add_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp, char **args)
{
	FILE		*ifp, *ofp;
	char		*tmpdir, *tmp;
	char		buff[BUFSIZ * 4], tbuf[MAXPATHLEN + 1];
	Row		*rp = NULL, *mr = NULL, *mr2 = NULL;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];
	char		*tdb, *db, *mp;
	struct stat	sb;
	boolean_t	nis_entry_seen = B_FALSE;
	long		cur_pos, nis_pos;
	int		status, i, fd = -1;

	/*
	 * Generate temporary file name to use.  We make sure it's in the same
	 * directory as the db we're processing so that we can use rename to
	 * do the replace later.  Otherwise we run the risk of being on the
	 * wrong filesystem and having rename() fail for that reason.
	 */
	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, tbuf);
	tdb = db = tbuf;
	if (trav_link(&tdb) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_READLINK_ERROR;
		return (TBL_FAILURE);
	}
	tmpdir = strdup(tdb);
	remove_component(tmpdir);
	if (strlen(tmpdir) == 0)
		(void) strcat(tmpdir, ".");
	tmp = _dd_tempfile(tmpdir);
	(void) free(tmpdir);
	if ((ofp = fopen(tmp, "w")) == NULL) {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) setbuf(ofp, NULL);	/* Make stream unbuffered */

	/*
	 * Preserve permissions of current file if it exists; otherwise it's
	 * up to the caller to set umask or do a chmod later.
	 */
	if ((status = stat(tdb, &sb)) == 0) {
		if (fchmod(fileno(ofp), sb.st_mode) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHMOD_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
		if (fchown(fileno(ofp), sb.st_uid, sb.st_gid) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHOWN_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
	} else if (errno != ENOENT) {
		if (tbl_err != NULL)
			*tbl_err = TBL_STAT_ERROR;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	if (_dd_lock_db(db, F_WRLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	/*
	 * Process file, line at a time.
	 */
	if ((ifp = fopen(db, "r+")) != NULL) {
		/*
		 * Set up the "match" rows we use for duplicate detection.
		 * mr is a row containing all the info passed in, mr2 just
		 * contains the columns which are supposed to be unique.
		 */
		if (((mr = _dd_new_row()) == NULL) ||
		    ((mr2 = _dd_new_row()) == NULL)) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_MEMORY;
			cleanup_files;
			return (TBL_FAILURE);
		}
		for (i = 0; i < tfp->cols; ++i) {
			if ((tfp->cfmts[i].flags & COL_KEY) &&
			    (args[tfp->cfmts[i].argno] != NULL) &&
			    strlen(args[tfp->cfmts[i].argno])) {
				mr->ca[tfp->cfmts[i].argno] =
				    args[tfp->cfmts[i].argno];
				if ((tfp->cfmts[i].flags & COL_UNIQUE))
					mr2->ca[tfp->cfmts[i].argno] =
					    mr->ca[tfp->cfmts[i].argno];
			}
		}
		for (cur_pos = 0;
		    get_db_line(buff, sizeof (buff), ifp, ttp) != NULL;
		    cur_pos = ftell(ifp)) {
			if (ttp->yp_compat && !nis_entry_seen) {
				if (strchr("+-", buff[0]) != NULL) {
					nis_pos = cur_pos;
					nis_entry_seen = B_TRUE;
				}
			}
			if ((status = _parse_db_buffer(buff, &rp, tfp, ttp))
			    < 0) {
				if (tbl_err != NULL)
					*tbl_err = TBL_NO_MEMORY;
				cleanup_files;
				return (TBL_FAILURE);
			}

			/*
			 * Detect illegal duplication here.  First match the
			 * current row against the list of unique keys; if
			 * any match, error.  If no matches there, match
			 * against the complete list of keys and if it's
			 * a complete match, error, otherwise continue.
			 */

			if (((status = _match_entry(mr2, rp, &mp, tfp)) &
			    COL_MATCHED) || ((status = _match_entry(mr, rp,
			    &mp, tfp)) == EXACT_MATCH)) {
				if (tbl_err != NULL)
					*tbl_err = TBL_ENTRY_EXISTS;
				cleanup_files;
				_dd_free_row(rp);
				return (TBL_FAILURE);
			} else if (!nis_entry_seen) {
				write_buff;
			}
			if (rp != NULL) {
				_dd_free_row(rp);
				rp = NULL;
			}
		}
	/*
	 * Absence of the file is not an error; we just end up creating it.
	 */
	} else if (errno != ENOENT) {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}

	/*
	 * Now add the entry.
	 */
	if (format_entry(buff, sizeof (buff), args, ttp) != 0) {
		if (tbl_err != NULL)
			*tbl_err = TBL_TOO_BIG;
		cleanup_files;
		return (TBL_FAILURE);
	}
	write_buff;
	/*
	 * If we paused copying because of NIS, rewind and bulk copy
	 * the remaining entries.
	 */
	if (nis_entry_seen) {
		if (fseek(ifp, nis_pos, SEEK_SET) != 0) {
			if (tbl_err != NULL)
				*tbl_err = TBL_READ_ERROR;
			cleanup_files;
			return (TBL_FAILURE);
		}
		while ((status = fread(buff, sizeof (char), sizeof (buff),
		    ifp)) > 0) {
			if (fwrite(buff, sizeof (char), status, ofp) != status)
				break;
		}
		if (!feof(ifp) && ferror(ifp)) {
			if (tbl_err != NULL)
				*tbl_err = TBL_READ_ERROR;
			cleanup_files;
			return (TBL_FAILURE);
		} else if (ferror(ofp)) {
			if (tbl_err != NULL)
				*tbl_err = TBL_WRITE_ERROR;
			cleanup_files;
			return (TBL_FAILURE);
		}
	}

	(void) fclose(ifp);
	(void) fsync(fileno(ofp));
	(void) fclose(ofp);
	(void) free(mr);
	(void) free(mr2);

	/*
	 * Rename the tmp file over the original, and we're done.
	 */
	if (rename(tmp, tdb) != 0) {
		if (tbl_err != NULL)
			*tbl_err = TBL_RENAME_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) _dd_unlock_db(&fd);
	(void) free(tmp);
	return (TBL_SUCCESS);
}

/*
 * Function to remove entries from a filesystem table.
 */
int
_rm_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp, char **args)
{
	FILE		*ifp, *ofp;
	char		*tmpdir, *tmp;
	char		buff[BUFSIZ * 2], tbuf[MAXPATHLEN + 1];
	Row		*rp = NULL, *mr = NULL, *mr2 = NULL;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];
	char		*tdb, *db, *mp;
	struct stat	sb;
	int		i, fd = -1;
	boolean_t	found = B_FALSE, match_flag = B_FALSE;

	/*
	 * Generate temporary file name to use.  We make sure it's in the same
	 * directory as the db we're processing so that we can use rename to
	 * do the replace later.  Otherwise we run the risk of being on the
	 * wrong filesystem and having rename() fail for that reason.
	 */
	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, tbuf);
	tdb = db = tbuf;
	if (trav_link(&tdb) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_READLINK_ERROR;
		return (TBL_FAILURE);
	}
	tmpdir = strdup(tdb);
	remove_component(tmpdir);
	if (strlen(tmpdir) == 0)
		(void) strcat(tmpdir, ".");
	tmp = _dd_tempfile(tmpdir);
	(void) free(tmpdir);
	if ((ofp = fopen(tmp, "w")) == NULL) {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) setbuf(ofp, NULL);	/* Make stream unbuffered */

	/*
	 * Preserve permissions of current file if it exists; otherwise it's
	 * up to the caller to set umask or do a chmod later.
	 */
	if (stat(tdb, &sb) == 0) {
		if (fchmod(fileno(ofp), sb.st_mode) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHMOD_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
		if (fchown(fileno(ofp), sb.st_uid, sb.st_gid) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHOWN_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_STAT_ERROR;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	if (_dd_lock_db(db, F_WRLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	/*
	 * Process file, line at a time.
	 */
	if ((ifp = fopen(db, "r+")) != NULL) {
		/*
		 * Set up the "match" row we use for locating the entry.
		 */
		if ((mr = _dd_new_row()) == NULL) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_MEMORY;
			cleanup_files;
			return (TBL_FAILURE);
		}
		for (i = 0; i < tfp->cols; ++i) {
			if ((tfp->cfmts[i].flags & COL_KEY) &&
			    (args[tfp->cfmts[i].m_argno] != NULL) &&
			    strlen(args[tfp->cfmts[i].m_argno])) {
				mr->ca[tfp->cfmts[i].argno] =
				    args[tfp->cfmts[i].m_argno];
				match_flag = B_TRUE;
			}
		}
		if (!match_flag) {
			if (tbl_err != NULL)
				*tbl_err = TBL_MATCH_CRITERIA_BAD;
			cleanup_files;
			return (TBL_FAILURE);
		}
		while (get_db_line(buff, sizeof (buff), ifp, ttp) != NULL) {
			if (_parse_db_buffer(buff, &rp, tfp, ttp) < 0) {
				if (tbl_err != NULL)
					*tbl_err = TBL_NO_MEMORY;
				cleanup_files;
				return (TBL_FAILURE);
			}

			/*
			 * Compare vs. match criteria; if this is it we just
			 * skip writing it to the tmp file and continue.
			 */
			if (_match_entry(mr, rp, &mp, tfp) == EXACT_MATCH)
				found = B_TRUE;
			else {
				write_buff;
			}
			if (rp != NULL) {
				_dd_free_row(rp);
				rp = NULL;
			}
		}
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) fclose(ofp);
		(void) fclose(ifp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	if (!found) {
		for (i = 0, mp = args[i]; (mp == NULL) || !strlen(mp);
		    mp = args[++i]);
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ENTRY;
		(void) free(mr);
		(void) _dd_unlock_db(&fd);
		(void) fclose(ofp);
		(void) fclose(ifp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}

	(void) free(mr);
	(void) fclose(ifp);
	(void) fsync(fileno(ofp));
	(void) fclose(ofp);

	/*
	 * Rename the tmp file over the original, and we're done.
	 */
	if (rename(tmp, tdb) != 0) {
		if (tbl_err != NULL)
			*tbl_err = TBL_RENAME_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) _dd_unlock_db(&fd);
	(void) free(tmp);
	return (TBL_SUCCESS);
}

#undef cleanup_files
/*
 * Macro to do error cleanup of open files in _mod_dd_ufs
 */
#define	cleanup_files {							\
	(void) fclose(ifp);						\
	(void) _dd_unlock_db(&fd);					\
	(void) fclose(ofp);						\
	(void) unlink(tmp);						\
	(void) free(tmp);						\
	(void) free(mr);						\
	(void) free(mr2);						\
	(void) free(mr3);						\
}

/*
 * Function to modify entries in a filesystem table.
 */
int
_mod_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp, char **args)
{
	FILE		*ifp, *ofp;
	char		*tmpdir, *tmp;
	char		buff[BUFSIZ * 4], tbuf[MAXPATHLEN + 1];
	Row		*rp = NULL, *mr = NULL, *mr2 = NULL, *mr3 = NULL;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];
	char		*tdb, *db, *mp;
	struct stat	sb;
	int		i, fd = -1;
	boolean_t	found = B_FALSE;

	/*
	 * Generate temporary file name to use.  We make sure it's in the same
	 * directory as the db we're processing so that we can use rename to
	 * do the replace later.  Otherwise we run the risk of being on the
	 * wrong filesystem and having rename() fail for that reason.
	 */
	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, tbuf);
	tdb = db = tbuf;
	if (trav_link(&tdb) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_READLINK_ERROR;
		return (TBL_FAILURE);
	}
	tmpdir = strdup(tdb);
	remove_component(tmpdir);
	if (strlen(tmpdir) == 0)
		(void) strcat(tmpdir, ".");
	tmp = _dd_tempfile(tmpdir);
	(void) free(tmpdir);
	if ((ofp = fopen(tmp, "w")) == NULL) {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) setbuf(ofp, NULL);	/* Make stream unbuffered */

	/*
	 * Preserve permissions of current file if it exists; otherwise it's
	 * up to the caller to set umask or do a chmod later.
	 */
	if (stat(tdb, &sb) == 0) {
		if (fchmod(fileno(ofp), sb.st_mode) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHMOD_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
		if (fchown(fileno(ofp), sb.st_uid, sb.st_gid) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHOWN_ERROR;
			(void) fclose(ofp);
			(void) unlink(tmp);
			(void) free(tmp);
			return (TBL_FAILURE);
		}
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_STAT_ERROR;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	if (_dd_lock_db(db, F_WRLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		(void) fclose(ofp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	/*
	 * Process file, line at a time.
	 */
	if ((ifp = fopen(db, "r+")) != NULL) {
		/*
		 * Set up the "match" rows we use for duplicate detection and
		 * locating the entry we're to modify.
		 * mr is the row we're looking for
		 * mr2 contains all the keys in the new data
		 * mr3 contains the keys in the new data which are supposed
		 * to be unique
		 */
		if (((mr = _dd_new_row()) == NULL) ||
		    ((mr2 = _dd_new_row()) == NULL) ||
		    ((mr3 = _dd_new_row()) == NULL)) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_MEMORY;
			cleanup_files;
			return (TBL_FAILURE);
		}
		for (i = 0; i < tfp->cols; ++i) {
			if ((tfp->cfmts[i].flags & COL_KEY)) {
				if ((args[tfp->cfmts[i].m_argno] != NULL) &&
				    strlen(args[tfp->cfmts[i].m_argno]))
					mr->ca[tfp->cfmts[i].argno] =
					    args[tfp->cfmts[i].m_argno];
				if ((args[tfp->cfmts[i].argno+ttp->search_args]
				    != NULL) &&
				    strlen(args[tfp->cfmts[i].argno+ttp->
					search_args])) {
					mr2->ca[tfp->cfmts[i].argno] =
					    args[tfp->cfmts[i].argno +
					    ttp->search_args];
					if ((tfp->cfmts[i].flags & COL_UNIQUE))
						mr3->ca[tfp->cfmts[i].argno] =
						    mr2->ca[tfp->
						    cfmts[i].argno];
				}
			}
		}
		while (get_db_line(buff, sizeof (buff), ifp, ttp) != NULL) {
			if (_parse_db_buffer(buff, &rp, tfp, ttp) < 0) {
				if (tbl_err != NULL)
					*tbl_err = TBL_NO_MEMORY;
				cleanup_files;
				return (TBL_FAILURE);
			}

			/*
			 * Now see if this is the one we're supposed to
			 * replace.  If so, replace it.
			 */
			if (_match_entry(mr, rp, &mp, tfp) == EXACT_MATCH) {
				if (format_entry(buff, sizeof (buff),
				    &args[ttp->search_args], ttp) != 0) {
					if (tbl_err != NULL)
						*tbl_err = TBL_TOO_BIG;
					cleanup_files;
					return (TBL_FAILURE);
				}
				write_buff;
				_dd_free_row(rp);
				found = B_TRUE;
			/*
			 * Detect illegal duplication here.  First match the
			 * current row against the list of unique keys; if
			 * any match, error.  If no matches there, match
			 * against the complete list of keys and if it's
			 * a complete match, error, otherwise continue.
			 */
			} else if ((_match_entry(mr3, rp, &mp, tfp) &
			    COL_MATCHED) || (_match_entry(mr2, rp, &mp, tfp) ==
			    EXACT_MATCH)) {
				if (tbl_err != NULL)
					*tbl_err = TBL_ENTRY_EXISTS;
				cleanup_files;
				_dd_free_row(rp);
				return (TBL_FAILURE);
			} else {
				write_buff;
				_dd_free_row(rp);
			}
		}
		(void) free(mr);
		(void) free(mr2);
		(void) free(mr3);
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) fclose(ofp);
		(void) fclose(ifp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	if (!found) {
		for (i = 0, mp = args[i]; (mp == NULL) || !strlen(mp);
		    mp = args[++i]);
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ENTRY;
		(void) _dd_unlock_db(&fd);
		(void) fclose(ofp);
		(void) fclose(ifp);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}

	(void) fclose(ifp);
	(void) fsync(fileno(ofp));
	(void) fclose(ofp);

	/*
	 * Rename the tmp file over the original, and we're done.
	 */
	if (rename(tmp, tdb) != 0) {
		if (tbl_err != NULL)
			*tbl_err = TBL_RENAME_ERROR;
		(void) _dd_unlock_db(&fd);
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	(void) _dd_unlock_db(&fd);
	(void) free(tmp);
	return (TBL_SUCCESS);
}

#undef cleanup_files
/*
 * Macro to do error cleanup of open files in _make_dd_ufs
 */
#define	cleanup_files {							\
	(void) _dd_unlock_db(&fd);					\
	(void) unlink(tmp);						\
	(void) free(tmp);						\
}
/*
 * Function to create a new database.
 */
int
_make_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp, char *user,
    char *group)
{
	FILE		*ofp;
	char		*tmpdir, *tmp;
	char		tbuf[MAXPATHLEN + 1];
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];
	mode_t		mode;
	uid_t		luid;
	gid_t		lgid;
	char		*tdb, *db;
	struct stat	sb;
	struct passwd	*pw;
	struct group	*gr;
	int		tstat, fd = -1;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, tbuf);
	tdb = db = tbuf;
	if (trav_link(&tdb) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_READLINK_ERROR;
		return (TBL_FAILURE);
	}
	tmpdir = strdup(tdb);
	remove_component(tmpdir);
	if (strlen(tmpdir) == 0)
		(void) strcat(tmpdir, ".");
	/*
	 * Try creating the directory; we ignore errors here in order
	 * to not change the error cases from previous releases
	 */
	(void) mkdirp(tmpdir, 0755);

	tmp = _dd_tempfile(tmpdir);
	(void) free(tmpdir);
	if (_dd_lock_db(db, F_WRLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		(void) unlink(tmp);
		(void) free(tmp);
		return (TBL_FAILURE);
	}
	/*
	 * Check if file already exists.  Return error if so.
	 */
	tstat = stat(tbuf, &sb);
	if (tstat == 0) {
		if (tbl_err != NULL)
			*tbl_err = TBL_TABLE_EXISTS;
		cleanup_files;
		return (TBL_FAILURE);
	} else if (errno != ENOENT) {
		if (errno == EACCES) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ACCESS;
		} else {
			if (tbl_err != NULL)
				*tbl_err = TBL_STAT_ERROR;
		}
		cleanup_files;
		return (TBL_FAILURE);
	}
	/*
	 * Check owning user and group names.
	 */
	luid = -1;
	if (user != (char *)NULL) {
		if ((pw = getpwnam(user)) != (struct passwd *)NULL)
			luid = pw->pw_uid;
		else {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_USER;
			cleanup_files;
			return (TBL_FAILURE);
		}
	}
	lgid = -1;
	if (group != (char *)NULL) {
		if ((gr = getgrnam(group)) != (struct group *)NULL)
			lgid = gr->gr_gid;
		else {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_GROUP;
			cleanup_files;
			return (TBL_FAILURE);
		}
	}
	/*
	 * Create a new temporary file.
	 */
	if ((ofp = fopen(tmp, "w")) == NULL) {
		if (errno == EACCES)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ACCESS;
		else
			if (tbl_err != NULL)
				*tbl_err = TBL_OPEN_ERROR;
		cleanup_files;
		return (TBL_FAILURE);
	}
	/*
	 * Set permissions and ownership of new file.
	 */
	mode = (mode_t)0664;
	if (fchmod(fileno(ofp), mode) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_CHMOD_ERROR;
		(void) fclose(ofp);
		cleanup_files;
		return (TBL_FAILURE);
	}
	if ((luid != -1) || (lgid != -1)) {
		if (fchown(fileno(ofp), luid, lgid) == -1) {
			if (tbl_err != NULL)
				*tbl_err = TBL_CHOWN_ERROR;
			(void) fclose(ofp);
			cleanup_files;
			return (TBL_FAILURE);
		}
	}
	/*
	 * Rename temporary file to our table.
	 */
	(void) fclose(ofp);
	if (rename(tmp, tdb) != 0) {
		if (errno == EACCES)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ACCESS;
		else
			if (tbl_err != NULL)
				*tbl_err = TBL_RENAME_ERROR;
		cleanup_files;
		return (TBL_FAILURE);
	}
	(void) _dd_unlock_db(&fd);
	(void) free(tmp);
	return (TBL_SUCCESS);
}

/*
 * Function to delete an entire database.
 */
int
_del_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp)
{
	char		buff[BUFSIZ * 2];
	char		*tdb;
	int		fd;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, buff);
	tdb = buff;
	if (trav_link(&tdb) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_READLINK_ERROR;
		return (TBL_FAILURE);
	}
	if (_dd_lock_db(buff, F_WRLCK, &fd) == -1) {
		if (tbl_err != NULL)
			*tbl_err = TBL_IS_BUSY;
		return (TBL_FAILURE);
	}
	/*
	 * Remove file by unlinking its directory entry.
	 *	NB: We are not cleaning up any links to the file!
	 */
	if (unlink(tdb) != 0) {
		if (errno == ENOENT)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_TABLE;
		else if (errno == EACCES)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ACCESS;
		else
			if (tbl_err != NULL)
				*tbl_err = TBL_UNLINK_ERROR;
		(void) _dd_unlock_db(&fd);
		return (TBL_FAILURE);
	}
	(void) _dd_unlock_db(&fd);
	return (TBL_SUCCESS);
}

/*
 * Function to return the status of a database file.
 */
int
_stat_dd_ufs(char *name, int *tbl_err, struct tbl_trans_data *ttp,
    Tbl_stat **tbl_stpp)
{
	char		buff[BUFSIZ * 2];
	struct stat	sb;
	struct tbl_fmt	*tfp = &ttp->fmts[TBL_NS_UFS];
	Tbl_stat	*tbl_st;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	build_tbl_name(name, tfp->prefix, tfp->suffix, buff);
	/*
	 * Check for existence of database file.
	 */
	if (stat(buff, &sb) != 0) {
		if (errno == ENOENT)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_TABLE;
		else if (errno == EACCES)
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_ACCESS;
		else
			if (tbl_err != NULL)
				*tbl_err = TBL_STAT_ERROR;
		return (TBL_FAILURE);
	}
	/*
	 * Build table status structure.
	 */
	if ((tbl_st = (Tbl_stat *)malloc(sizeof (struct tbl_stat))) ==
	    (Tbl_stat *)NULL) {
		if (tbl_err != NULL)
			*tbl_err = TBL_NO_MEMORY;
	}
	tbl_st->name = strdup(name);
	tbl_st->ns = TBL_NS_UFS;
	tbl_st->perm.ufs.mode = sb.st_mode;
	tbl_st->perm.ufs.owner_uid = sb.st_uid;
	tbl_st->perm.ufs.owner_gid = sb.st_gid;
	tbl_st->atime = sb.st_atime;
	tbl_st->mtime = sb.st_mtime;
	/*
	 * Return pointer to table stat structure.
	 */
	*tbl_stpp = tbl_st;
	return (TBL_SUCCESS);
}

/*
 * Function to return the list of DHCP network tables when filesystem is
 * used as the data store.  Look for all files which contain only numbers
 * and underscores in their name, residing in the DHCP data directory.
 * Returns a NULL-terminated list of char *'s if successful; failures
 * return NULL and set tbl_err appropriately.
 */
char **
_dd_ls_ufs(int *tbl_err, const char *pathp)
{
	char		**outlist = NULL;
	int		outcnt = 0;
	DIR		*dirp;
	struct dirent	*de;

	dirp = opendir(pathp);
	if (dirp == NULL) {
		if (errno == EACCES) {
			*tbl_err = TBL_NO_ACCESS;
		} else if (errno == ENOENT) {
			*tbl_err = TBL_NO_ENTRY;
		} else if (errno == ELOOP) {
			*tbl_err = TBL_READLINK_ERROR;
		} else {
			*tbl_err = TBL_OPEN_ERROR;
		}
		return (NULL);
	}
	errno = 0;
	while ((de = readdir(dirp)) != NULL) {
		/* Network tables have names consisting only of numbers and _ */
		if (strspn(de->d_name, "0123456789_") ==
		    strlen(de->d_name)) {
			++outcnt;
			outlist = realloc(outlist, sizeof (char *) * outcnt);
			if (outlist == NULL) {
				*tbl_err = TBL_NO_MEMORY;
				(void) closedir(dirp);
				return (NULL);
			}
			outlist[outcnt - 1] = strdup(de->d_name);
			if (outlist[outcnt-1] == NULL) {
				*tbl_err = TBL_NO_MEMORY;
				(void) closedir(dirp);
				--outcnt; /* Change from count to index */
				while (--outcnt >= 0) {
					free(outlist[outcnt]);
				}
				return (NULL);
			}
		}
	}
	if (errno != 0) {
		if (errno == ENOENT) {
			*tbl_err = TBL_NO_ENTRY;
		} else {
			*tbl_err = TBL_READ_ERROR;
		}
		(void) closedir(dirp);
		while (--outcnt >= 0) {
			free(outlist[outcnt]);
		}
		return (NULL);
	}

	(void) closedir(dirp);

	/* Terminate list with a null entry */
	outlist = realloc(outlist, sizeof (char *) * ++outcnt);
	if (outlist == NULL) {
		*tbl_err = TBL_NO_MEMORY;
		return (NULL);
	}
	outlist[outcnt - 1] = NULL;
	return (outlist);
}
