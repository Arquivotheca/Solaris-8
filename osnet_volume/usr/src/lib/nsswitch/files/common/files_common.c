/*
 * Copyright (c) 1995-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Common code and structures used by name-service-switch "files" backends.
 */

#pragma	ident	"@(#)files_common.c	1.30	99/10/11 SMI"

/*
 * An implementation that used mmap() sensibly would be a wonderful thing,
 *   but this here is just yer standard fgets() thang.
 */

#include "files_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>

/*ARGSUSED*/
nss_status_t
_nss_files_setent(be, dummy)
	files_backend_ptr_t	be;
	void			*dummy;
{
	if (be->f == 0) {
		if (be->filename == 0) {
			/* Backend isn't initialized properly? */
			return (NSS_UNAVAIL);
		}
		if ((be->f = fopen(be->filename, "r")) == 0) {
			return (NSS_UNAVAIL);
		}
	} else {
		rewind(be->f);
	}
	return (NSS_SUCCESS);
}

/*ARGSUSED*/
nss_status_t
_nss_files_endent(be, dummy)
	files_backend_ptr_t	be;
	void			*dummy;
{
	if (be->f != 0) {
		fclose(be->f);
		be->f = 0;
	}
	if (be->buf != 0) {
		free(be->buf);
		be->buf = 0;
	}
	return (NSS_SUCCESS);
}

/*
 * This routine reads a line, including the processing of continuation
 * characters.  It always leaves (or inserts) \n\0 at the end of the line.
 * It returns the length of the line read, excluding the \n\0.  Who's idea
 * was this?
 * Returns -1 on EOF.
 *
 * Note that since each concurrent call to _nss_files_read_line has
 * it's own FILE pointer, we can use getc_unlocked w/o difficulties,
 * a substantial performance win.
 */
int
_nss_files_read_line(f, buffer, buflen)
	FILE			*f;
	char			*buffer;
	int			buflen;
{
	int			linelen;	/* 1st unused slot in buffer */
	int			c;

	/*CONSTCOND*/
	while (1) {
		linelen = 0;
		while (linelen < buflen - 1) {	/* "- 1" saves room for \n\0 */
			switch (c = getc_unlocked(f)) {
			case EOF:
				if (linelen == 0 ||
				    buffer[linelen - 1] == '\\') {
					return (-1);
				} else {
					buffer[linelen    ] = '\n';
					buffer[linelen + 1] = '\0';
					return (linelen);
				}
			case '\n':
				if (linelen > 0 &&
				    buffer[linelen - 1] == '\\') {
					--linelen;  /* remove the '\\' */
				} else {
					buffer[linelen    ] = '\n';
					buffer[linelen + 1] = '\0';
					return (linelen);
				}
				break;
			default:
				buffer[linelen++] = c;
			}
		}
		/* Buffer overflow -- eat rest of line and loop again */
		/* ===> Should syslog() */
		do {
			c = getc_unlocked(f);
			if (c == EOF) {
				return (-1);
			}
		} while (c != '\n');
	}
	/*NOTREACHED*/
}

/*
 * used only for getgroupbymem() now.
 */
nss_status_t
_nss_files_do_all(be, args, filter, func)
	files_backend_ptr_t	be;
	void			*args;
	const char		*filter;
	files_do_all_func_t	func;
{
	char			*buffer;
	int			buflen;
	nss_status_t		res;

	if (be->buf == 0 &&
		(be->buf = malloc(be->minbuf)) == 0) {
		return (NSS_UNAVAIL);
	}
	buffer = be->buf;
	buflen = be->minbuf;

	if ((res = _nss_files_setent(be, 0)) != NSS_SUCCESS) {
		return (res);
	}

	res = NSS_NOTFOUND;

	do {
		int		linelen;

		if ((linelen = _nss_files_read_line(be->f, buffer,
		    buflen)) < 0) {
			/* End of file */
			break;
		}
		if (filter != 0 && strstr(buffer, filter) == 0) {
			/*
			 * Optimization:  if the entry doesn't contain the
			 *   filter string then it can't be the entry we want,
			 *   so don't bother looking more closely at it.
			 */
			continue;
		}
		res = (*func)(buffer, linelen, args);

	} while (res == NSS_NOTFOUND);

	_nss_files_endent(be, 0);
	return (res);
}

/*
 * Could implement this as an iterator function on top of _nss_files_do_all(),
 *   but the shared code is small enough that it'd be pretty silly.
 */
nss_status_t
_nss_files_XY_all(be, args, netdb, filter, check)
	files_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	int			netdb;		/* whether it uses netdb */
						/* format or not */
	const char		*filter;	/* advisory, to speed up */
						/* string search */
	files_XY_check_func	check;	/* NULL means one-shot, for getXXent */
{
	nss_status_t		res;
	int	parsestat;
	int (*func)();

	if (be->buf == 0 &&
		(be->buf = malloc(be->minbuf)) == 0) {
		return (NSS_UNAVAIL); /* really panic, malloc failed */
	}

	if (check != 0 || be->f == 0) {
		if ((res = _nss_files_setent(be, 0)) != NSS_SUCCESS) {
			return (res);
		}
	}

	res = NSS_NOTFOUND;

	/*CONSTCOND*/
	while (1) {
		char		*instr	= be->buf;
		int		linelen;

		if ((linelen = _nss_files_read_line(be->f, instr,
		    be->minbuf)) < 0) {
			/* End of file */
			args->returnval = 0;
			args->erange    = 0;
			break;
		}
		if (filter != 0 && strstr(instr, filter) == 0) {
			/*
			 * Optimization:  if the entry doesn't contain the
			 *   filter string then it can't be the entry we want,
			 *   so don't bother looking more closely at it.
			 */
			continue;
		}
		if (netdb) {
			char		*first;
			char		*last;

			if ((last = strchr(instr, '#')) == 0) {
				last = instr + linelen;
			}
			*last-- = '\0';		/* Nuke '\n' or #comment */

			/*
			 * Skip leading whitespace.  Normally there isn't
			 *   any, so it's not worth calling strspn().
			 */
			for (first = instr;  isspace(*first);  first++) {
				;
			}
			if (*first == '\0') {
				continue;
			}
			/*
			 * Found something non-blank on the line.  Skip back
			 * over any trailing whitespace;  since we know
			 * there's non-whitespace earlier in the line,
			 * checking for termination is easy.
			 */
			while (isspace(*last)) {
				--last;
			}

			linelen = last - first + 1;
			if (first != instr) {
					instr = first;
			}
		}

		args->returnval = 0;

		func = args->str2ent;
		parsestat = (*func)(instr, linelen, args->buf.result,
					args->buf.buffer, args->buf.buflen);

		if (parsestat == NSS_STR_PARSE_SUCCESS) {
			args->returnval = args->buf.result;
			if (check == 0 || (*check)(args)) {
				res = NSS_SUCCESS;
				break;
			}
		} else if (parsestat == NSS_STR_PARSE_ERANGE) {
			args->erange = 1;	/* should we just skip this */
						/* one long line ?? */
			break;
		} /* else if (parsestat == NSS_STR_PARSE_PARSE) don't care ! */
	}

	/*
	 * stayopen is set to 0 by default in order to close the opened
	 * file.  Some applications may break if it is set to 1.
	 */
	if (check != 0 && !args->stayopen) {
		(void) _nss_files_endent(be, 0);
	}

	return (res);
}

/*
 * File hashing support.  Critical for sites with large (e.g. 1000+ lines)
 * /etc/passwd or /etc/group files.  Currently only used by getpw*() and
 * getgr*() routines, but any files backend can use this stuff.
 */
static void
_nss_files_hash_destroy(files_hash_t *fhp)
{
	free(fhp->fh_table);
	fhp->fh_table = NULL;
	free(fhp->fh_line);
	fhp->fh_line = NULL;
	free(fhp->fh_file_start);
	fhp->fh_file_start = NULL;
}
#ifdef PIC
/*
 * It turns out the hashing stuff really needs to be disabled for processes
 * other than the nscd; the consumption of swap space and memory is otherwise
 * unacceptable when the nscd is killed w/ a large passwd file (4M) active.
 * See 4031930 for details.
 * So we just use this psuedo function to enable the hashing feature.  Since
 * this function name is private, we just create a function w/ the name
 *  __nss_use_files_hash in the nscd itself and everyone else uses the old
 * interface.
 * We also disable hashing for .a executables to avoid problems with large
 * files....
 */

#pragma weak __nss_use_files_hash

extern void  __nss_use_files_hash(void);
#endif /* pic */

nss_status_t
_nss_files_XY_hash(files_backend_ptr_t be, nss_XbyY_args_t *args,
	int netdb, files_hash_t *fhp, int hashop, files_XY_check_func check)
{
	int fd, retries, ht;
	u_int hash, line, f;
	files_hashent_t *hp, *htab;
	char *cp, *first, *last;
	nss_XbyY_args_t xargs;
	struct stat64 st;

#ifndef PIC
	return (_nss_files_XY_all(be, args, netdb, 0, check));
}
#else
	if (__nss_use_files_hash == 0)
		return (_nss_files_XY_all(be, args, netdb, 0, check));

	mutex_lock(&fhp->fh_lock);
retry:
	retries = 100;
	while (stat64(be->filename, &st) < 0) {
		/*
		 * On a healthy system this can't happen except during brief
		 * periods when the file is being modified/renamed.  Keep
		 * trying until things settle down, but eventually give up.
		 */
		if (--retries == 0)
			goto unavail;
		poll(0, 0, 100);
	}

	if (st.st_mtim.tv_sec == fhp->fh_mtime.tv_sec &&
	    st.st_mtim.tv_nsec == fhp->fh_mtime.tv_nsec &&
	    fhp->fh_table != NULL) {
		htab = &fhp->fh_table[hashop * fhp->fh_size];
		hash = fhp->fh_hash_func[hashop](args, 1);
		for (hp = htab[hash % fhp->fh_size].h_first; hp != NULL;
		    hp = hp->h_next) {
			if (hp->h_hash != hash)
				continue;
			line = hp - htab;
			if ((*args->str2ent)(fhp->fh_line[line].l_start,
			    fhp->fh_line[line].l_len, args->buf.result,
			    args->buf.buffer, args->buf.buflen) ==
			    NSS_STR_PARSE_SUCCESS) {
				args->returnval = args->buf.result;
				if ((*check)(args)) {
					mutex_unlock(&fhp->fh_lock);
					return (NSS_SUCCESS);
				}
			} else {
				args->erange = 1;
			}
		}
		args->returnval = 0;
		mutex_unlock(&fhp->fh_lock);
		return (NSS_NOTFOUND);
	}

	_nss_files_hash_destroy(fhp);

	if (st.st_size > SSIZE_MAX)
		goto unavail;

	if ((fhp->fh_file_start = malloc((ssize_t)st.st_size + 1)) == NULL)
		goto unavail;

	if ((fd = open(be->filename, O_RDONLY)) < 0)
		goto unavail;

	if (read(fd, fhp->fh_file_start, (ssize_t)st.st_size) !=
	    (ssize_t)st.st_size) {
		close(fd);
		goto retry;
	}

	close(fd);

	fhp->fh_file_end = fhp->fh_file_start + (off_t)st.st_size;
	*fhp->fh_file_end = '\n';
	fhp->fh_mtime = st.st_mtim;

	/*
	 * If the file changed since we read it, or if it's less than
	 * 1-2 seconds old, don't trust it; its modification may still
	 * be in progress.  The latter is a heuristic hack to minimize
	 * the likelihood of damage if someone modifies /etc/mumble
	 * directly (as opposed to editing and renaming a temp file).
	 *
	 * Note: the cast to u_int is there in case (1) someone rdated
	 * the system backwards since the last modification of /etc/mumble
	 * or (2) this is a diskless client whose time is badly out of sync
	 * with its server.  The 1-2 second age hack doesn't cover these
	 * cases -- oh well.
	 */
	if (stat64(be->filename, &st) < 0 ||
	    st.st_mtim.tv_sec != fhp->fh_mtime.tv_sec ||
	    st.st_mtim.tv_nsec != fhp->fh_mtime.tv_nsec ||
	    (u_int)(time(0) - st.st_mtim.tv_sec + 2) < 4) {
		poll(0, 0, 1000);
		goto retry;
	}

	line = 1;
	for (cp = fhp->fh_file_start; cp < fhp->fh_file_end; cp++)
		if (*cp == '\n')
			line++;

	for (f = 2; f * f <= line; f++) {	/* find next largest prime */
		if (line % f == 0) {
			f = 1;
			line++;
		}
	}

	fhp->fh_size = line;
	fhp->fh_line = malloc(line * sizeof (files_linetab_t));
	fhp->fh_table = calloc(line * fhp->fh_nhtab, sizeof (files_hashent_t));
	if (fhp->fh_line == NULL || fhp->fh_table == NULL)
		goto unavail;

	xargs = *args;
	xargs.buf.result = malloc(fhp->fh_resultsize + fhp->fh_bufsize);
	if (xargs.buf.result == NULL)
		goto unavail;
	xargs.buf.buffer = (char *)xargs.buf.result + fhp->fh_resultsize;
	xargs.buf.buflen = fhp->fh_bufsize;
	xargs.returnval = xargs.buf.result;

	line = 0;
	cp = fhp->fh_file_start;
	while (cp < fhp->fh_file_end) {
		first = cp;
		while (*cp != '\n')
			cp++;
		if (cp > first && *(cp - 1) == '\\') {
			memmove(first + 2, first, cp - first - 1);
			cp = first + 2;
			continue;
		}
		last = cp;
		*cp++ = '\0';
		if (netdb) {
			if ((last = strchr(first, '#')) == 0)
				last = cp - 1;
			*last-- = '\0';		/* nuke '\n' or #comment */
			while (isspace(*first))	/* nuke leading whitespace */
				first++;
			if (*first == '\0')	/* skip content-free lines */
				continue;
			while (isspace(*last))	/* nuke trailing whitespace */
				--last;
			*++last = '\0';
		}
		if ((*xargs.str2ent)(first, last - first,
		    xargs.buf.result, xargs.buf.buffer, xargs.buf.buflen) !=
		    NSS_STR_PARSE_SUCCESS)
			continue;
		for (ht = 0; ht < fhp->fh_nhtab; ht++) {
			hp = &fhp->fh_table[ht * fhp->fh_size + line];
			hp->h_hash = fhp->fh_hash_func[ht](&xargs, 0);
		}
		fhp->fh_line[line].l_start = first;
		fhp->fh_line[line++].l_len = last - first;
	}
	free(xargs.buf.result);

	/*
	 * Populate the hash tables in reverse order so that the hash chains
	 * end up in forward order.  This ensures that hashed lookups find
	 * things in the same order that a linear search of the file would.
	 * This is essential in cases where there could be multiple matches.
	 * For example: until 2.7, root and smtp both had uid 0; but we
	 * certainly wouldn't want getpwuid(0) to return smtp.
	 */
	for (ht = 0; ht < fhp->fh_nhtab; ht++) {
		htab = &fhp->fh_table[ht * fhp->fh_size];
		for (hp = &htab[line - 1]; hp >= htab; hp--) {
			u_int bucket = hp->h_hash % fhp->fh_size;
			hp->h_next = htab[bucket].h_first;
			htab[bucket].h_first = hp;
		}
	}

	goto retry;

unavail:
	_nss_files_hash_destroy(fhp);
	mutex_unlock(&fhp->fh_lock);
	return (NSS_UNAVAIL);
}
#endif PIC

nss_status_t
_nss_files_getent_rigid(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, args, 0, 0, 0));
}

nss_status_t
_nss_files_getent_netdb(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, args, 1, 0, 0));
}

/*ARGSUSED*/
nss_status_t
_nss_files_destr(be, dummy)
	files_backend_ptr_t	be;
	void			*dummy;
{
	if (be != 0) {
		if (be->f != 0) {
			_nss_files_endent(be, 0);
		}
		if (be->hashinfo != NULL) {
			mutex_lock(&be->hashinfo->fh_lock);
			if (--be->hashinfo->fh_refcnt == 0)
				_nss_files_hash_destroy(be->hashinfo);
			mutex_unlock(&be->hashinfo->fh_lock);
		}
		free(be);
	}
	return (NSS_SUCCESS);	/* In case anyone is dumb enough to check */
}

nss_backend_t *
_nss_files_constr(ops, n_ops, filename, min_bufsize, fhp)
	files_backend_op_t	ops[];
	int			n_ops;
	const char		*filename;
	int			min_bufsize;
	files_hash_t		*fhp;
{
	files_backend_ptr_t	be;

	if ((be = (files_backend_ptr_t) malloc(sizeof (*be))) == 0) {
		return (0);
	}
	be->ops		= ops;
	be->n_ops	= n_ops;
	be->filename	= filename;
	be->minbuf	= min_bufsize;
	be->f		= 0;
	be->buf		= 0;
	be->hashinfo	= fhp;

	if (fhp != NULL) {
		mutex_lock(&fhp->fh_lock);
		fhp->fh_refcnt++;
		mutex_unlock(&fhp->fh_lock);
	}

	return ((nss_backend_t *) be);
}
