/*
 *
 *			cachefspack.c
 *
 */

/*
 * Copyright (c) 1996-1997, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cachefspack.c 1.13     97/12/06 SMI"

#include <locale.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <fslib.h>
#include <dirent.h>
#include <search.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dlog.h>
#include <sys/fs/cachefs_ioctl.h>
#include <sys/utsname.h>
#include <rpc/rpc.h>
#include "../common/subr.h"
#include "../common/cachefsd.h"

enum packop {P_PACK, P_UNPACK, P_INFO, P_SIZES};

struct info {
	enum packop	i_packop;		/* pack, unpack, info */
	time_t		i_guiinfo;		/* !0 if outputing gui stats */
	time_t		i_guiexp;		/* time to output next stat */
	int		i_verbose;		/* 1 if verbose output */
	int		i_dolib;		/* 1 means okay to do libs */
	u_longlong_t	i_size;			/* total pack size */
	long long	i_filecnt;		/* total number of files */
	int		i_lines;		/* total lines in pack file */
	int		i_curline;		/* current line in pack file */
	int		i_hashfull;		/* hash table is full */
	int		i_estimate;		/* estimated size only */
};
typedef struct info info_t;

struct split {
	char *data;
};
typedef struct split split_t;

/* error codes */
#define	ERR_NOSPACE	1
#define	ERR_OTHER	2
#define	ERR_NOTCFS	3

#define	RNDV (8 * 1024)

/* forward references */
int do_args(info_t *ip, char *fflag, int argc, char **argv, int index);
void usage(char *msgp);
void pr_err(char *fmt, ...);
void help_message();
int packinglistfile(char *filep, info_t *ip, int level);
int process_pathname(char *pathnamep, info_t *ip, int recurse);
int process_file(DIR *dirp, char *dirpathp, char *filep, info_t *ip,
    struct stat64 *sinfop);
int process_dir(DIR *dirp, char *dirpathp, info_t *ip);
split_t *split_path(char *pathp, int *cntp);
char *xxmalloc(size_t size);
int need_slash(const char *p);
int process_lib(char *dirpathp, char *filep, info_t *ip);
void update_sizes(info_t *ip, struct stat64 *sinfop, int addsub);
void output_status(info_t *ip);
int unpackall(info_t *ip, char *cachenamep);
int skip_file(info_t *ip, const char *cp);

/*
 *
 *			main
 *
 * Description:
 *	Main routine for the cachefspack program.
 * Arguments:
 *	argc	number of command line arguments
 *	argv	list of command line arguments
 * Returns:
 *	Returns 0 for success, 1 an error was encountered.
 * Preconditions:
 */

main(int argc, char **argv)
{
	int xx;
	int c;
	char *filep;
	int typecnt = 0;
	int hflag = 0;
	char *Uflag = NULL;
	char *fflag = NULL;
	info_t infodata;
	int error;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* set default options */
	infodata.i_packop = P_PACK;
	infodata.i_guiinfo = 0;
	infodata.i_verbose = 0;
	infodata.i_dolib = 1;
	infodata.i_size = 0;
	infodata.i_filecnt = 0;
	infodata.i_lines = 0;
	infodata.i_curline = 0;
	infodata.i_estimate = 0;


	/* process command line options */
	while ((c = getopt(argc, argv, "ehpuiU:s:vf:")) != EOF) {
		switch (c) {
		case 'e':	/* print estimated size */
			infodata.i_estimate = 1;
			break;

		case 'h':	/* print help info */
			hflag = 1;
			break;

		case 'p':	/* pack files */
			typecnt++;
			infodata.i_packop = P_PACK;
			break;

		case 'u':	/* unpack files */
			typecnt++;
			infodata.i_packop = P_UNPACK;
			break;

		case 'i':	/* pack info */
			typecnt++;
			infodata.i_packop = P_INFO;
			break;

		case 'U':	/* unpack all files */
			Uflag = optarg;
			break;

		case 's':	/* print statistics for gui */
			infodata.i_guiinfo = atol(optarg);
			if (infodata.i_guiinfo == 0)
				infodata.i_guiinfo = 5;
			infodata.i_guiexp = time(NULL) + infodata.i_guiinfo;
			break;

		case 'v':	/* verbose output */
			infodata.i_verbose = 1;
			break;

		case 'f':	/* file with packing information */
			fflag = optarg;
			break;

		default:
			usage("invalid option");
			return (1);
		}
	}

	/* print help message (and exit) if requested */
	if (hflag) {
		help_message();
		return (0);
	}

	/* only one of -p -u -i allowed */
	if (typecnt > 1) {
		usage(gettext("Select only one of -p, -u, or -i."));
		return (1);
	}

	/* create hash table for tracking libraries */
	if (hcreate(10000) == 0) {
		/* unlikely this ever happens or I would work around it */
		pr_err(gettext("Cannot allocate heap space."));
		return (ERR_OTHER);
	}
	infodata.i_hashfull = 0;

	/* precompute packing list size if necessary */
	if (infodata.i_estimate ||
	    (infodata.i_guiinfo && (infodata.i_packop == P_PACK))) {
		infodata.i_packop = P_SIZES;
		do_args(&infodata, fflag, argc, argv, optind);
		if (infodata.i_estimate) {
			printf(gettext("cachefspack: estimated size: %lld\n"),
			    infodata.i_size);
			fflush(stdout);
			return (0);
		}
		infodata.i_packop = P_PACK;
		output_status(&infodata);
	}

	/* if should unpack everything, do it now */
	if (Uflag) {
		xx = unpackall(&infodata, Uflag);
		if (xx)
			return (xx);
	}

	/* see if there are any files to process */
	if (!fflag && (argc == optind)) {
		if (Uflag == NULL) {
			usage(gettext("No files to process."));
			return (1);
		}
		return (0);
	}

	/* process the args */
	error = do_args(&infodata, fflag, argc, argv, optind);
	if ((infodata.i_packop == P_PACK) && infodata.i_guiinfo)
		output_status(&infodata);

	/* return result */
	return (error);
}

/*
 * Actually process the set of files.
 */
int
do_args(info_t *ip, char *fflag, int argc, char **argv, int index)
{
	int error;
	char *filep;

	/* process packing list file */
	if (fflag) {
		error = packinglistfile(fflag, ip, 0);
		if (error == ERR_NOSPACE) {
			output_status(ip);
			goto out;
		}
	}

	/* process files on command line */
	for (; index < argc; index++) {
		/* get the next file */
		filep = argv[index];

		/* process the file */
		error = process_pathname(filep, ip, 1);
		if (error == ERR_NOSPACE)
			goto out;
	}

out:
	return (error);
}

/*
 *
 *			usage
 *
 * Description:
 *	Prints a short usage message.
 * Arguments:
 *	msgp	message to include with the usage message
 * Returns:
 * Preconditions:
 */

void
usage(char *msgp)
{
	if (msgp) {
		pr_err(gettext("%s"), msgp);
	}

	fprintf(stderr,
	    gettext("Usage: cachefspack [options] [file ...]\n"));
}

/*
 *
 *			pr_err
 *
 * Description:
 *	Prints an error message to stderr.
 * Arguments:
 *	fmt	printf style format
 *	...	arguments for fmt
 * Returns:
 * Preconditions:
 *	precond(fmt)
 */

void
pr_err(char *fmt, ...)
{
	va_list ap;

	va_start(ap);
	fflush(stdout);
	(void) fprintf(stderr, gettext("cachefspack: "));
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	fflush(stderr);
	va_end(ap);
}

/*
 *			help_message
 *
 * Description:
 *	Prints out a detailed help message.
 * Arguments:
 * Returns:
 * Preconditions:
 */

static char *helptxt =
	"    -h        this help message\n"
	"    -p        pack (default)\n"
	"    -u        unpack\n"
	"    -i        packing information\n"
	"    -f file   packing file\n"
	"    -U        unpack all files\n"
	"  Only one of -p, -u or -i can be specified.\n";

void
help_message()
{
	usage(NULL);
	(void) fprintf(stderr, gettext(helptxt));
}


/*
 * Processes a packing list file.
 */
int
packinglistfile(char *filep, info_t *ip, int level)
{
	FILE *fin;
	char buf[MAXPATHLEN * 2];
	char name[MAXPATHLEN * 2];
	char *namep;
	char *attrp;
	char *strp;
	int xx;
	int linecnt;
	char *cp;
	struct stat64 statinfo;
	char *dp = NULL;
	int dplen;
	int error;

	if (ip->i_verbose)
		printf("processing packing list file '%s'\n", filep);

	/* open the file */
	fin = fopen64(filep, "r");
	if (fin == NULL) {
		pr_err(gettext("Cannot open packing list file %s: %s"),
		    filep, strerror(errno));
		return (ERR_OTHER);
	}

	/* make sure a regular file */
	xx = fstat64(fileno(fin), &statinfo);
	if (xx == -1) {
		pr_err(gettext("Cannot stat packing list file %s: %s"),
		    filep, strerror(errno));
		fclose(fin);
		return (ERR_OTHER);
	}
	if (!S_ISREG(statinfo.st_mode)) {
		pr_err(gettext("Packing list file %s is not a regular file."),
		    filep);
		fclose(fin);
		return (ERR_OTHER);
	}

	/* check the first line of the file for our identifier */
	if (fgets(buf, sizeof (buf), fin) == NULL) {
		pr_err(gettext("Packing list file %s cannot be read."), filep);
		fclose(fin);
		return (ERR_OTHER);
	}
	if (strcmp(buf, "#Cachefs Packing List\n") != 0) {
		pr_err(gettext("Packing list file %s is missing header line."),
		    filep);
		fclose(fin);
		return (ERR_OTHER);
	}

	/* pack the packing list file */
	error = process_pathname(filep, ip, 0);
	if (error && (error != ERR_NOTCFS)) {
		fclose(fin);
		return (error);
	}
	error = 0;

	/* determine the path prefix for relative file paths in the pack file */
	cp = strrchr(filep, '/');
	if (cp) {
		dplen = cp - filep;
		dp = xxmalloc(dplen + 10);
		sprintf(dp, "%.*s", dplen, filep);
	}

	if (ip->i_verbose && dp)
		printf("path prefix for relative files '%s'\n", dp);

	/* process each line of the file */
	for (linecnt = 2; ; linecnt++) {
		/* output status */
		if ((level == 0) && (ip->i_packop == P_PACK) && ip->i_guiinfo)
			output_status(ip);

		/* get the next line */
		strp = fgets(buf, sizeof (buf), fin);
		if (strp == NULL)
			break;

		if ((level == 0) && (ip->i_packop == P_PACK) && ip->i_guiinfo)
			ip->i_curline = linecnt;

		/* chop off the new line */
		strp[strlen(strp) - 1] = '\0';

		/* get the file name from the input line */
		namep = strtok(buf, " \t");
		if (namep == NULL) {
			pr_err(gettext("Bad input line %s:%d, no file name"),
			    filep, linecnt);
			error = ERR_OTHER;
			break;
		}
		attrp = "x";

		/* skip over the file size */
		strp = strtok(NULL, " \t");
		if (strp) {
			/* get the file type */
			attrp = strtok(NULL, " \t");
			if (attrp == NULL)
				attrp = "x";
		}

		/* if a relative path, prepend path to packing file */
		if ((*namep != '/') && dp) {
			if ((strlen(namep) + dplen + 1) > (size_t)MAXPATHLEN) {
				pr_err(gettext("File name too long '%s/%s'"),
				    dp, namep);
				error = ERR_OTHER;
				break;
			}
			sprintf(name, "%s/%s", dp, namep);
			namep = name;
		}

		if (ip->i_verbose)
			printf("%s:%d file '%s', type '%s'\n",
			    filep, linecnt, namep, attrp);

		/* if another packing list file */
		if (*attrp == 'p') {
			error = packinglistfile(namep, ip, level+1);
			if (error == ERR_NOSPACE)
				break;
			error = 0;
		}

		/* else process the file */
		else {
			error = process_pathname(namep, ip, 1);
			if (error == ERR_NOSPACE)
				break;
			error = 0;
		}
	}

	/* save the number of lines if the first pack file */
	if ((level == 0) && (ip->i_packop == P_SIZES))
		ip->i_lines = linecnt - 1;

	if (dp)
		free(dp);

	fclose(fin);
	return (error);
}

/*
 * Processes a full pathname.
 */
int
process_pathname(char *pathnamep, info_t *ip, int recurse)
{
	DIR *dirp;
	int xx;
	char dirbuf[MAXPATHLEN];
	char filebuf[MAXPATHLEN];
	char linkbuf[MAXPATHLEN];
	char rbuf[MAXPATHLEN];
	char tbuf[MAXPATHLEN];
	struct stat64 sinfo;
	split_t *splitp, *sp;
	int cnt;
	int index;
	char *dp;
	int error = 0;

	/* make a copy of the path so split_path can modify it */
	if ((int)strlen(pathnamep) >= MAXPATHLEN) {
		pr_err(gettext("Path too long '%s'"), pathnamep);
		return (1);
	}
	strcpy(tbuf, pathnamep);

	/* split the path into tokens */
	splitp = sp = split_path(tbuf, &cnt);
	if (sp == NULL)
		return (0);

	dirbuf[0] = '\0';
	filebuf[0] = '\0';
	for (index = 1; index <= cnt; index++, sp++) {
		/* construct the path to the next component */
		if (need_slash(dirbuf))
			sprintf(filebuf, "%s/%s", dirbuf, sp->data);
		else
			sprintf(filebuf, "%s%s", dirbuf, sp->data);

		/* find out what type of file */
		xx = lstat64(filebuf, &sinfo);
		if (xx == -1) {
			pr_err(gettext("File %s: %s"),
			    filebuf, strerror(errno));
			error = ERR_OTHER;
			break;
		}

		/* if a directory */
		if (S_ISDIR(sinfo.st_mode)) {
			/*
			 * Note: we process directories differently than
			 * regular files.  We always open the directory
			 * and process "." instead of the other way which
			 * is to open the parent directory and process the
			 * child directory.  This is so traversing mount
			 * points works.
			 */

			/* open the directory */
			dirp = opendir(filebuf);
			if (dirp == NULL) {
				pr_err(gettext("Cannot open directory %s: %s"),
				    filebuf, strerror(errno));
				error = ERR_OTHER;
				break;
			}

			/* if the last component, pack recursively */
			if ((index == cnt) && recurse) {
				error = process_dir(dirp, filebuf, ip);
			}

			/* else just pack the dir contents */
			else {
				if (skip_file(ip, filebuf) == 0) {
					error = process_file(dirp, filebuf,
					    ".", ip, NULL);
					if (error == ERR_NOTCFS)
						error = 0;
				}
			}
			closedir(dirp);
			if (error)
				break;
		}

		/* else if not a directory */
		else {
			/* determine the parent directory */
			if (dirbuf[0] == '\0')
				dp = ".";
			else
				dp = dirbuf;

			/* open the parent directory */
			dirp = opendir(dp);
			if (dirp == NULL) {
				pr_err(gettext("Cannot open directory %s: %s"),
				    dp, strerror(errno));
				error = ERR_OTHER;
				break;
			}

			/* process the file */
			error = process_file(dirp, dp, sp->data, ip, &sinfo);
			if (error == ERR_NOTCFS) {
				if ((index == cnt) &&
				    (ip->i_packop == P_INFO)) {
					pr_err(gettext("File '%s' is not in"
					    " a CacheFS file system"), filebuf);
				}
				error = 0;
			}
			closedir(dirp);
			if (error)
				break;
		}

		/* if this component is a symbolic link */
		if (S_ISLNK(sinfo.st_mode)) {
			/* read the link */
			xx = readlink(filebuf, linkbuf, MAXPATHLEN - 1);
			if (xx == -1) {
				pr_err(gettext("Cannot read link %s: %s"),
				    filebuf, strerror(errno));
				error = ERR_OTHER;
				break;
			}
			linkbuf[xx] = '\0';

			/* if a relative path, prepend current directory */
			if (linkbuf[0] != '/') {
				xx = strlen(dirbuf) + strlen(linkbuf) + 2;
				if (xx > MAXPATHLEN) {
					pr_err(gettext("Path too long %s/%s"),
					    dirbuf, linkbuf);
					error = ERR_OTHER;
					break;
				}
				sprintf(rbuf, "%s/%s", dirbuf, linkbuf);
			} else {
				strcpy(rbuf, linkbuf);
			}

			/* now process the link */
			process_pathname(rbuf, ip, 0);
		}

		/* set up the path to the next component */
		if (need_slash(dirbuf))
			strcat(dirbuf, "/");
		strcat(dirbuf, sp->data);
	}

	free(splitp);
	return (error);
}

/*
 * Returns 1 if string does not consist of "" or "/"
 */
int
need_slash(const char *p)
{
	if ((*p == '\0') ||
	    ((*p == '/') && (*(p+1) == '\0')))
		return (0);
	return (1);
}

/*
 * Splits the specified path into its components.
 * pathp is modified.
 * free up the returned value when done.
 * *cntp is set to the number of components.
 */
split_t *
split_path(char *pathp, int *cntp)
{
	char data[MAXPATHLEN];
	int cnt;
	split_t *sp1, *sp2;
	int xx;

	/* make a copy of the path so can determine token count */
	if ((int)strlen(pathp) >= MAXPATHLEN) {
		pr_err(gettext("Path too long '%s'"), pathp);
		return (NULL);
	}
	strcpy(data, pathp);

	/* pass1, find the number of tokens */
	cnt = 0;
	if (strtok(data, "/")) {
		cnt++;
		while (strtok(NULL, "/"))
			cnt++;
	}
	if (*pathp == '/')
		cnt++;

	/* quit if no tokens */
	if (cnt == 0)
		return (NULL);

	/* get space to store pointers to the tokens */
	sp1 = (split_t *)xxmalloc(cnt * sizeof (split_t));
	sp2 = sp1;

	/* pass2, get pointers to tokens */
	xx = 0;
	if (*pathp == '/') {
		sp2->data = "/";
		sp2++;
		xx++;
	}
	if (xx < cnt) {
		sp2->data = strtok(pathp, "/");
		xx++;
		sp2++;
		for (; xx < cnt; xx++, sp2++) {
			sp2->data = strtok(NULL, "/");
			assert(sp2->data);
		}
	}

	*cntp = cnt;
	return (sp1);
}

/*
 * Like malloc excepts dies if no more memory.
 */
char *
xxmalloc(size_t size)
{
	char *datap;
	datap = malloc(size);
	if (datap == NULL) {
		pr_err(gettext("out of memory"));
		exit(2);
	}
	return (datap);
}

/*
 * Unpacks all files.
 */
int
unpackall(info_t *ip, char *cachenamep)
{
	CLIENT *clnt;
	enum clnt_stat retval;
	int ret;
	int xx;
	int result;
	char *hostp;
	struct utsname info;
	struct cachefsd_caches_return caches;
	struct cachefsd_mount_returns mounts;
	struct cachefsd_mount_stat mstat;
	struct cachefsd_mount_stat_args mstatarg;
	struct cachefsd_caches_id *cp;
	struct cachefsd_mount *mp;
	int index;
	int index2;
	int fd;
	int done;

	/* get the host name */
	xx = uname(&info);
	if (xx == -1) {
		pr_err(gettext("cannot get host name, errno %d"), errno);
		return (1);
	}
	hostp = info.nodename;

	/* creat the connection to the daemon */
	clnt = clnt_create(hostp, CACHEFSDPROG, CACHEFSDVERS, "tcp");
	if (clnt == NULL) {
		pr_err(gettext("cachefsd is not running"));
		return (1);
	}

	/* get the list of caches */
	retval = cachefsd_caches_1(NULL, &caches, clnt);
	if (retval != RPC_SUCCESS) {
		clnt_perror(clnt, gettext("cachefsd is not responding"));
		clnt_destroy(clnt);
		return (1);
	}

	/* if there are no caches */
	if (caches.ccr_ids.ccr_ids_len == 0) {
		pr_err(gettext("No caches exist to unpack."));
		clnt_destroy(clnt);
		return (1);
	}

	/* loop through the list of caches */
	for (index = 0; index < caches.ccr_ids.ccr_ids_len; index++) {
		cp = &caches.ccr_ids.ccr_ids_val[index];

		/* next if this cache does not match cache to unpack */
		if ((strcmp(cachenamep, "all") != 0) &&
		    (strcmp(cachenamep, cp->cci_name) != 0))
			continue;

		if (ip->i_verbose) {
			printf("cache '%s', id '%d'\n",
			    cp->cci_name, cp->cci_cacheid);
		}

		/* get the list of file systems in the cache */
		retval = cachefsd_mounts_1(&cp->cci_cacheid, &mounts, clnt);
		if (retval != RPC_SUCCESS) {
			clnt_perror(clnt,
			    gettext("cachefsd is not responding"));
			clnt_destroy(clnt);
			return (1);
		}

		/* if there are no file systems in the cache */
		if (mounts.cmr_names.cmr_names_len == 0) {
			pr_err(gettext("No file systems exists in cache %s"),
			    cp->cci_name);
			continue;
		}

		/* loop through the list of file systems */
		done = 0;
		for (index2 = 0; index2 < mounts.cmr_names.cmr_names_len;
		    index2++) {
			mp = &mounts.cmr_names.cmr_names_val[index2];

			/* get info about this mount point */
			mstatarg.cma_cacheid = cp->cci_cacheid;
			mstatarg.cma_fsid = mp->cm_fsid;
			retval = cachefsd_mount_stat_1(&mstatarg, &mstat, clnt);
			if (retval != RPC_SUCCESS) {
				clnt_perror(clnt,
				    gettext("cachefsd is not responding"));
				clnt_destroy(clnt);
				return (1);
			}

			/* open the mount point */
			fd = open64(mstat.cms_mountpt, O_RDONLY);
			if (fd == -1)
				continue;

			/* get info about this mount point again */
			retval = cachefsd_mount_stat_1(&mstatarg, &mstat, clnt);
			if (retval != RPC_SUCCESS) {
				clnt_perror(clnt,
				    gettext("cachefsd is not responding"));
				clnt_destroy(clnt);
				close(fd);
				return (1);
			}

			/* skip if not mounted */
			if (mstat.cms_mounted != 1) {
				close(fd);
				continue;
			}

			/* unpack the cache through this file system */
			xx = ioctl(fd, CACHEFSIO_UNPACKALL, 0);
			if (xx) {
				if ((errno == ENOTTY) || (errno == ENOSYS)) {
					close(fd);
					continue;
				}
				pr_err(gettext("Unexpected error unpacking"
				    " cache %s: %s"),
				    cp->cci_name, strerror(errno));
				close(fd);
				break;
			}
			done = 1;

			if (ip->i_verbose) {
				printf("cache %s unpacked from fs %s\n",
				    cp->cci_name, mp->cm_name);
			}

			close(fd);
			break;
		}

		if (!done) {
			pr_err(gettext("Could not unpack cache %s, no mounted"
			    " filesystems in the cache."), cp->cci_name);
		}
	}

	ret = 0;

	clnt_destroy(clnt);

	return (0);
}

/*
 * Processes a single file or directory.
 */
int
process_file(DIR *dirp, char *dirpathp, char *filep, info_t *ip,
    struct stat64 *sinfop)
{
	int xx = 0;
	int error = 0;
	cachefsio_pack_t pack;
	char *cp;

	if (ip->i_verbose) {
		if (need_slash(dirpathp))
			printf("processing file '%s/%s'\n", dirpathp, filep);
		else
			printf("processing file '%s%s'\n", dirpathp, filep);
	}

	/* set up pack structure for the ioctls */
	if (strlen(filep) >= sizeof (pack.p_name)) {
		pr_err(gettext("File name too long '%s'"), filep);
		return (ERR_OTHER);
	}
	strcpy(pack.p_name, filep);
	pack.p_status = 0;

	/* process the file */
	switch (ip->i_packop) {
	case P_PACK:
		xx = ioctl(dirp->dd_fd, CACHEFSIO_PACK, &pack);
		if (xx) {
			if ((errno == ENOTTY) || (errno == ENOSYS))
				error = ERR_NOTCFS;
			else if (errno == ENOSPC)
				error = ERR_NOSPACE;
			else
				error = ERR_OTHER;
			if (error && (error != ERR_NOTCFS))
				pr_err(gettext("Cannot pack '%s' '%s': %s"),
				    dirpathp, filep, strerror(errno));
			break;
		}
		update_sizes(ip, sinfop, -1);

		/* if a regular executable file */
		if (sinfop && S_ISREG(sinfop->st_mode) &&
		    (sinfop->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			error = process_lib(dirpathp, filep, ip);
		}
		break;

	case P_UNPACK:
		xx = ioctl(dirp->dd_fd, CACHEFSIO_UNPACK, &pack);
		if (xx) {
			if ((errno == ENOTTY) || (errno == ENOSYS))
				error = ERR_NOTCFS;
			else {
				error = ERR_OTHER;
				pr_err(gettext("Cannot unpack '%s' '%s': %s"),
				    dirpathp, filep, strerror(errno));
			}
		}
		break;

	case P_INFO:
		xx = ioctl(dirp->dd_fd, CACHEFSIO_PACKINFO, &pack);
		if (xx) {
			if ((errno == ENOTTY) || (errno == ENOSYS))
				error = ERR_NOTCFS;
			else {
				error = ERR_OTHER;
				pr_err(gettext("Cannot get pack info"
				    " on '%s' '%s': %s"),
				    dirpathp, filep, strerror(errno));
			}
			break;
		}

		/* output status about the packed state */
		if (need_slash(dirpathp))
			cp = gettext("cachefspack: file %s/%s");
		else
			cp = gettext("cachefspack: file %s%s");
		printf(cp, dirpathp, filep);
		printf(gettext(" marked packed %s, packed %s\n"),
		    (pack.p_status & CACHEFS_PACKED_FILE) ? "YES" : "NO",
		    (pack.p_status & CACHEFS_PACKED_DATA) ? "YES" : "NO");
		if (ip->i_verbose) {
			printf(gettext("    nocache %s\n"),
			    (pack.p_status & CACHEFS_PACKED_NOCACHE) ?
			    "YES" : "NO");
		}

		/* if a regular executable file */
		if (sinfop && S_ISREG(sinfop->st_mode) &&
		    (sinfop->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			error = process_lib(dirpathp, filep, ip);
		}
		break;

	case P_SIZES:
		xx = ioctl(dirp->dd_fd, CACHEFSIO_PACKINFO, &pack);
		if (xx) {
			if ((errno == ENOTTY) || (errno == ENOSYS))
				error = ERR_NOTCFS;
			else {
				error = ERR_OTHER;
			}
			break;
		}

		update_sizes(ip, sinfop, 1);
		if (ip->i_estimate)
			break;

		/* if a regular executable file */
		if (sinfop && S_ISREG(sinfop->st_mode) &&
		    (sinfop->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			error = process_lib(dirpathp, filep, ip);
		}
		break;

	default:
		assert(0);
	}

	return (error);
}

/*
 * Processes a directory recursively.
 */
int
process_dir(DIR *dirp, char *dirpathp, info_t *ip)
{
	int xx;
	struct stat64 sinfo;
	struct dirent64 *entp;
	char path[MAXPATHLEN];
	DIR *cdirp;
	int error = 0;

	if (ip->i_verbose)
		printf("processing dir '%s'\n", dirpathp);

	/* pack the directory contents */
	error = process_file(dirp, dirpathp, ".", ip, NULL);
	if (error) {
		if ((error == ERR_NOTCFS) && (ip->i_packop == P_INFO)) {
			pr_err(gettext("Directory '%s' is not in"
			    " a CacheFS file system"), dirpathp);
		}
		return (error);
	}
	error = 0;

	/* for each entry in the directory */
	for (entp = readdir64(dirp); entp != NULL; entp = readdir64(dirp)) {
		/* ignore . and .. */
		if ((strcmp(entp->d_name, ".") == 0) ||
		    (strcmp(entp->d_name, "..") == 0))
			continue;

		/* construct a path to the file */
		xx = strlen(dirpathp) + strlen(entp->d_name) + 2;
		if (xx > MAXPATHLEN) {
			pr_err(gettext("Path too long %s/%s"),
			    dirpathp, entp->d_name);
			error = ERR_OTHER;
			break;
		}
		sprintf(path, "%s/%s", dirpathp, entp->d_name);

		/* stat the file */
		xx = lstat64(path, &sinfo);
		if (xx == -1) {
			if (errno == ENOENT)
				continue;
			pr_err(gettext("Cannot stat file %s: %s"),
			    path, strerror(errno));
			error = ERR_OTHER;
			break;
		}

		/* if a directory */
		if (S_ISDIR(sinfo.st_mode)) {
			/* process the subdirectory */
			cdirp = opendir(path);
			if (cdirp == NULL) {
				pr_err(gettext("Cannot open directory %s: %s"),
				    path, strerror(errno));
				continue;
			}
			error = process_dir(cdirp, path, ip);
			closedir(cdirp);
			if ((error == ERR_NOTCFS) && (ip->i_packop == P_INFO)) {
				pr_err(gettext("Directory '%s' is not in"
				    " a CacheFS file system"), path);
			}
		} else {
			/* process the file */
			error = process_file(dirp, dirpathp, entp->d_name, ip,
			    &sinfo);
		}
		if (error)
			break;
	}


	return (error);
}

/*
 * Runs ldd on the specified file and packs loadable librarys.
 */
int
process_lib(char *dirpathp, char *filep, info_t *ip)
{
	char path[MAXPATHLEN];
	char buf[MAXPATHLEN * 2];
	pid_t pid;
	int xx;
	char *ldd = "/usr/bin/ldd";
	int fildes[2];
	ssize_t len;
	FILE *fin = NULL;
	int error = 0;
	int status;
	char *cp;

	/* no need to do libraries recursively, ldd does this for us */
	if (ip->i_dolib == 0)
		return (0);

	/* construct the path to the file */
	xx = strlen(dirpathp) + strlen(filep) + 2;
	if (xx > MAXPATHLEN) {
		pr_err(gettext("Path too long %s/%s"), dirpathp, filep);
		return (ERR_OTHER);
	}
	if (need_slash(dirpathp))
		sprintf(path, "%s/%s", dirpathp, filep);
	else
		sprintf(path, "%s%s", dirpathp, filep);

	if (ip->i_verbose)
		printf("processing ldd file '%s'\n", path);

	ip->i_dolib = 0;

	/* create a pipe to read output of ldd */
	if (pipe(fildes) == -1) {
		pr_err(gettext("could not create pipe: %s"),
		    strerror(errno));
		return (1);
	}

	/* fork */
	if ((pid = fork()) == -1) {
		pr_err(gettext("could not fork %s"),
		    strerror(errno));
		error = ERR_OTHER;
		goto out;
	}

	/* if the child */
	if (pid == 0) {
		/* close down the side of the pipe this process does not use */
		close(fildes[0]);

		/* redirect stdout to the pipe */
		close(1);
		if (dup(fildes[1]) == -1) {
			pr_err(gettext("Dup failed: %s"), strerror(errno));
			exit(1);
		}
		close(fildes[1]);

		/* shut down stderr */
		close(2);

		/* exec ldd */
		execl(ldd, ldd, path, NULL);
		pr_err(gettext("Execl of %s failed: %s"), ldd, strerror(errno));
		exit(1);
	}
	/* else must be the parent */

	/* close down the side of the pipe this process does not use */
	close(fildes[1]);

	/* turn the file descriptor into a stream */
	fin = fdopen(fildes[0], "r");
	if (fin == NULL) {
		pr_err(gettext("could not open pipe: %s"), strerror(errno));
		error = ERR_OTHER;
		goto out;
	}

	/* process each line of input */
	for (;;) {
		/* get a line of input */
		if (fgets(buf, sizeof (buf) - 1, fin) == NULL)
			break;

		/* chop off the new line */
		buf[strlen(buf) - 1] = '\0';

		/* isolate the library name */
		cp = strtok(buf, "=>");
		if (cp == NULL)
			continue;
		cp = strtok(NULL, "=>");
		if (cp == NULL)
			continue;
		while ((*cp == ' ') || (*cp == '\t'))
			cp++;
		if (*cp == '\0')
			continue;

		/* skip if not found */
		if (strcmp(cp, "(not found)") == 0)
			continue;
		if (strcmp(cp, "(version not found)") == 0)
			continue;

		/* see if we can skip this library because we have seen it */
		if (skip_file(ip, cp))
			continue;

		if (ip->i_verbose)
			printf("Library '%s'\n", cp);

		/* process the library */
		error = process_pathname(cp, ip, 0);
		if (error && (error == ERR_NOSPACE))
			break;
		error = 0;
	}
out:
	if (fin)
		fclose(fin);
	close(fildes[0]);

	/* wait for the child to exit */
	if (wait(&status) == -1) {
		pr_err(gettext("wait failed %s"), strerror(errno));
		error = ERR_OTHER;
	} else if (!WIFEXITED(status)) {
		pr_err(gettext("%s did not exit"), ldd);
		error = ERR_OTHER;
	}

	ip->i_dolib = 1;
	return (error);
}

/*
 * Updates sizes and outputs periodic status messages.
 */
void
update_sizes(info_t *ip, struct stat64 *sinfop, int addsub)
{
	u_longlong_t sz;
	time_t tod;

	/* determine the size the file will occupy in the cache */
	if (sinfop) {
		if (S_ISREG(sinfop->st_mode)) {
			sz = (sinfop->st_size + RNDV - 1) / RNDV;
			sz = sz * RNDV / 1024;
		} else
			sz = 0;
	} else {
		sz = RNDV / 1024;
	}

	/* update the sizes we are tracking */
	if (addsub == 1) {
		ip->i_size += sz;
		ip->i_filecnt++;
	} else if (addsub == -1) {
		ip->i_size -= sz;
		ip->i_filecnt--;
	}

	/* output status message */
	if ((ip->i_packop == P_PACK) && ip->i_guiinfo) {
		tod = time(NULL);
		if (tod > ip->i_guiexp) {
			ip->i_guiexp = tod + ip->i_guiinfo;
			output_status(ip);
		}
	}
}

/*
 * Outputs a status message about how much progress has been made.
 */
void
output_status(info_t *ip)
{
	printf("cachefspack: status: line %d of %d,"
	    " files left %lld, Kbytes left %lld\n",
	    ip->i_curline, ip->i_lines, ip->i_filecnt, ip->i_size);
	fflush(stdout);
}

/*
 * Returns 1 if the file can be skipped, 0 if not.
 */
int
skip_file(info_t *ip, const char *cp)
{
	ENTRY hitem, *hitemp;

	/* look for the library in the hash table */
	hitem.key = strdup(cp);
	if (hitem.key == NULL)
		return (0);
	hitem.data = NULL;
	hitemp = hsearch(hitem, FIND);

	/* if found in the hash table */
	if (hitemp) {
		/* skip file if already saw it on this pass */
		if (ip->i_packop == (enum packop)hitemp->data) {
			free(hitem.key);
			return (1);
		}

		/* mark that we have seen this file for this pass */
		hitemp->data = (char *)ip->i_packop;
	}

	/* else if not found in the hash table and there is room */
	else if (ip->i_hashfull == 0) {
		/* mark that we have seen this file for this pass */
		hitem.data = (char *)ip->i_packop;

		/* try to insert the file into the hash table */
		if (hsearch(hitem, ENTER) == NULL) {
			if (ip->i_verbose)
				pr_err(gettext("hash table full"));
			ip->i_hashfull = 1;
		}
	}

	return (0);
}
