/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)print.c	1.1	99/08/13 SMI"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mman.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<errno.h>
#include	"sgs.h"
#include	"rtc.h"
#include	"conv.h"
#include	"_crle.h"
#include	"msg.h"


/*
 * Dump a configuration files information.  This routine could be maintained in
 * clre(1) itself, but the hash table traversal is identical to that used in
 * dump.c to load and dump individual objects, and so it is maintained here in
 * case either/bother routines need to be changed.
 */
static int
scanconfig(Crle_desc * crle, Addr addr)
{
	Rtc_head *	head = (Rtc_head *)addr;
	Rtc_dir *	dirtbl;
	Rtc_file *	filetbl;
	Rtc_obj *	objtbl, * obj;
	Word *		hash, * chain;
	const char *	strtbl, * str;
	int		ndx, bkts;

	/* LINTED */
	objtbl = (Rtc_obj *)((char *)head->ch_obj + addr);
	strtbl = (const char *)((char *)head->ch_str + addr);

	/*
	 * Start displaying the configuration files information, first its name
	 * and whether this is applicable only for a specific application.
	 */
	(void) printf(MSG_INTL(MSG_DMP_HEAD), crle->c_confil, head->ch_dlflags ?
	    conv_dlflag_str(head->ch_dlflags) : MSG_ORIG(MSG_STR_EMPTY));

	if (head->ch_app) {
		obj = (Rtc_obj *)(head->ch_app + addr);

		(void) printf(MSG_INTL(MSG_DMP_APP),
		    (strtbl + obj->co_alter), (strtbl + obj->co_name));
	}

	/*
	 * Display any alternative library path and secure directory entries.
	 */
	if (head->ch_edlibpath)
		(void) printf(MSG_INTL(MSG_DMP_DLIBPTH), MSG_ORIG(MSG_STR_ELF),
		    (const char *)(head->ch_edlibpath + addr));
	if (head->ch_adlibpath)
		(void) printf(MSG_INTL(MSG_DMP_DLIBPTH), MSG_ORIG(MSG_STR_AOUT),
		    (const char *)(head->ch_adlibpath + addr));
	if (head->ch_eslibpath)
		(void) printf(MSG_INTL(MSG_DMP_SLIBPTH), MSG_ORIG(MSG_STR_ELF),
		    (const char *)(head->ch_eslibpath + addr));
	if (head->ch_aslibpath)
		(void) printf(MSG_INTL(MSG_DMP_SLIBPTH), MSG_ORIG(MSG_STR_AOUT),
		    (const char *)(head->ch_aslibpath + addr));

	/*
	 * Display any memory reservations required for any alternative
	 * objects.
	 */
	if (head->ch_resbgn)
		(void) printf(MSG_INTL(MSG_DMP_RESV), head->ch_resbgn,
		    head->ch_resend, (head->ch_resend - head->ch_resbgn));

	if (head->ch_hash == 0)
		return (0);

	/*
	 * Traverse the directory and filename arrays.
	 */
	for (dirtbl = (Rtc_dir *)(head->ch_dir + addr);
	    dirtbl->cd_obj; dirtbl++) {
		struct stat	status;

		obj = (Rtc_obj *)(dirtbl->cd_obj + addr);
		str = strtbl + obj->co_name;

		if (obj->co_flags & RTC_OBJ_NOEXIST) {
			(void) printf(MSG_INTL(MSG_DMP_DIR_2), str);
			continue;
		}

		(void) printf(MSG_INTL(MSG_DMP_DIR_1), str);

		if (crle->c_flags & CRLE_VERBOSE) {
			if (stat(str, &status) != 0) {
				int err = errno;
				(void) printf(MSG_INTL(MSG_DMP_STAT), str,
				    strerror(err));
			} else if (status.st_mtime != obj->co_info) {
				(void) printf(MSG_INTL(MSG_DMP_DCMP), str);
			}
		}

		for (filetbl = (Rtc_file *)(dirtbl->cd_file + addr);
		    filetbl->cf_obj; filetbl++) {

			obj = (Rtc_obj *)(filetbl->cf_obj + addr);
			str = strtbl + obj->co_name;

			if (obj->co_alter)
				(void) printf(MSG_INTL(MSG_DMP_FILE_2), str,
				    (strtbl + obj->co_alter));
			else
				(void) printf(MSG_INTL(MSG_DMP_FILE_1), str);

			if (crle->c_flags & CRLE_VERBOSE) {
				if (stat(str, &status) != 0) {
					int err = errno;
					(void) printf(MSG_INTL(MSG_DMP_STAT),
					    str, strerror(err));
				} else if (status.st_size != obj->co_info) {
					(void) printf(MSG_INTL(MSG_DMP_FCMP),
					    str);
				}
			}
		}
	}
	(void) printf(MSG_ORIG(MSG_STR_NL));

	if ((crle->c_flags & CRLE_VERBOSE) == 0)
		return (0);

	/*
	 * If we've in verbose mode scan the hash list.
	 */
	/* LINTED */
	hash = (Word *)((char *)head->ch_hash + addr);
	bkts = hash[0];
	chain = &hash[2 + bkts];
	hash += 2;

	(void) printf(MSG_INTL(MSG_DMP_HASH));

	/*
	 * Scan the hash buckets looking for valid entries.
	 */
	for (ndx = 0; ndx < bkts; ndx++, hash++) {
		Rtc_obj *	obj;
		const char *	str;
		Word		_ndx;

		if (*hash == 0)
			continue;

		obj = objtbl + *hash;
		str = strtbl + obj->co_name;

		(void) printf(MSG_INTL(MSG_DMP_HASHENT_1), obj->co_id, ndx,
			str, conv_config_obj(obj->co_flags));

		/*
		 * Determine whether there are other objects chained to this
		 * bucket.
		 */
		for (_ndx = chain[*hash]; _ndx; _ndx = chain[_ndx]) {
			obj = objtbl + _ndx;
			str = strtbl + obj->co_name;

			(void) printf(MSG_INTL(MSG_DMP_HASHENT_2), obj->co_id,
			    str, conv_config_obj(obj->co_flags));
		}
	}
	(void) printf(MSG_ORIG(MSG_STR_NL));

	return (0);
}


int
printconfig(Crle_desc * crle)
{
	int		error;
	Addr		addr;
	int		fd;
	struct stat	status;
	const char *	caller = crle->c_name;
	const char *	file = crle->c_confil;

	/*
	 * Open the configuration file, determine its size and map it in.
	 */
	if ((fd = open(file, O_RDONLY, 0)) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN), caller, file,
		    strerror(err));
		return (1);
	}
	(void) fstat(fd, &status);
	if (status.st_size < sizeof (Rtc_head)) {
		(void) close(fd);
		(void) fprintf(stderr, MSG_INTL(MSG_COR_TRUNC), caller, file);
		return (1);
	}
	if ((addr = (Addr)mmap(0, status.st_size, PROT_READ, MAP_SHARED,
	    fd, 0)) == (Addr)MAP_FAILED) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_MMAP), caller, file,
		    strerror(err));
		(void) close(fd);
		return (1);
	}
	(void) close(fd);

	/*
	 * Print the contents of the configuration file.
	 */
	error = scanconfig(crle, addr);

	(void) munmap((void *)addr, status.st_size);
	return (error);
}
