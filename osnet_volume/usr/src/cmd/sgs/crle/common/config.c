/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)config.c	1.2	99/11/03 SMI"

#include	<sys/mman.h>
#include	<sys/types.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<errno.h>
#include	<stdio.h>
#include	<string.h>
#include	"rtc.h"
#include	"_crle.h"
#include	"msg.h"

#pragma	ident	"@(#)config.c	1.2	99/11/03 SMI"

#define	MAXNBKTS 10007

static const int hashsize[] = {
	3,	7,	13,	31,	53,	67,	83,	97,
	101,	151,	211,	251,	307,	353,	401,	457,	503,
	557,	601,	653,	701,	751,	809,	859,	907,	953,
	1009,	1103,	1201,	1301,	1409,	1511,	1601,	1709,	1801,
	1901,	2003,	2111,	2203,	2309,	2411,	2503,	2609,	2707,
	2801,	2903,	3001,	3109,	3203,	3301,	3407,	3511,	3607,
	3701,	3803,	3907,	4001,	5003,   6101,   7001,   8101,   9001,
	MAXNBKTS
};

/*
 * Generate a configuration file from the internal configuration information.
 * (very link-editor like).
 */
genconfig(Crle_desc * crle)
{
	int		ndx, bkt;
	size_t		size, hashoff = 0, stroff = 0, objoff = 0;
	size_t		diroff = 0, fileoff = 0;
	Addr		addr;
	Rtc_head *	head;
	Word *		hashtbl, * hashbkt, * hashchn, hashbkts = 0;
	char *		strtbl, * _strtbl;
	Rtc_obj *	objtbl;
	Rtc_dir *	dirtbl;
	Rtc_file *	filetbl;
	Hash_tbl *	stbl = crle->c_strtbl;
	Hash_ent *	ent;

	/*
	 * Establish the size of the configuration file.
	 */
	size = S_ROUND(sizeof (Rtc_head), sizeof (Word));

	if (crle->c_hashstrnum) {
		hashoff = size;

		/*
		 * Increment the hash string number to account for an initial
		 * null entry.  Indexes start at 1 to simplify hash lookup.
		 */
		crle->c_hashstrnum++;

		/*
		 * Determine the hash table size.  Establish the number of
		 * buckets from the number of strings, the number of chains is
		 * equivalent to the number of objects, and two entries for the
		 * nbucket and nchain entries.
		 */
		for (ndx = 0; ndx < (sizeof (hashsize) / sizeof (int)); ndx++) {
			if (crle->c_hashstrnum > hashsize[ndx])
				continue;
			hashbkts = hashsize[ndx];
			break;
		}
		if (hashbkts == 0)
			hashbkts = MAXNBKTS;
		size += ((2 + hashbkts + crle->c_hashstrnum) * sizeof (Word));
		size = S_ROUND(size, sizeof (Lword));
		objoff = size;

		/*
		 * Add the object table size (account for an 8-byte alignment
		 * requirement for each object).
		 */
		size += (crle->c_hashstrnum *
		    S_ROUND(sizeof (Rtc_obj), sizeof (Lword)));

		/*
		 * Add the file descripter arrays.
		 */
		fileoff = size;
		size += S_ROUND((crle->c_filenum * sizeof (Rtc_file)),
		    sizeof (Word));

		/*
		 * Add the directory descriptor array.
		 */
		diroff = size;
		size += S_ROUND((crle->c_dirnum * sizeof (Rtc_dir)),
		    sizeof (Word));
	}

	/*
	 * Add the string table size (this may contain library and/or secure
	 * path strings, in addition to any directory/file strings).
	 */
	if (crle->c_strsize) {
		stroff = size;
		size += S_ROUND(crle->c_strsize, sizeof (Word));
	}

	/*
	 * Truncate our temporary file now that we know its size and map it.
	 */
	if (ftruncate(crle->c_tempfd, size) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_TRUNC),
		    crle->c_name, crle->c_tempname, strerror(err));
		(void) close(crle->c_tempfd);
		return (1);
	}
	if ((addr = (Addr)mmap(0, size, (PROT_READ | PROT_WRITE), MAP_SHARED,
	    crle->c_tempfd, 0)) == (Addr)-1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_MMAP),
		    crle->c_name, crle->c_tempname, strerror(err));
		(void) close(crle->c_tempfd);
		return (1);
	}

	/*
	 * Save the mapped files info for possible dldump(3x) updates.
	 */
	crle->c_tempaddr = addr;
	crle->c_tempsize = size;

	/*
	 * Establish the real address of each of the structures within the file.
	 */
	head = (Rtc_head *)addr;

	head->ch_hash = hashoff;
	/* LINTED */
	hashtbl = (Word *)((char *)head->ch_hash + addr);

	head->ch_obj = objoff;
	/* LINTED */
	objtbl = (Rtc_obj *)((char *)head->ch_obj + addr);
	objtbl = (Rtc_obj *)S_ROUND((int)(objtbl + 1), sizeof (Lword));

	head->ch_file = fileoff;
	/* LINTED */
	filetbl = (Rtc_file *)((char *)head->ch_file + addr);

	head->ch_dir = diroff;
	/* LINTED */
	dirtbl = (Rtc_dir *)((char *)head->ch_dir + addr);

	head->ch_str = stroff;
	strtbl = _strtbl = (char *)((char *)head->ch_str + addr);

	/*
	 * Fill in additional basic header information.
	 */
	head->ch_version = RTC_VER_CURRENT;

	if (crle->c_flags & CRLE_ALTER)
		head->ch_cnflags |= RTC_HDR_ALTER;
	if (crle->c_flags & CRLE_DUMP) {
		head->ch_cnflags |= RTC_HDR_IGNORE;
		head->ch_dlflags = crle->c_dlflags;
	}

	if (crle->c_hashstrnum) {
		hashtbl[0] = hashbkts;
		hashtbl[1] = crle->c_hashstrnum;
		hashbkt = &hashtbl[2];
		hashchn = &hashtbl[2 + hashbkts];

		/*
		 * Insure all hash chain and directory/filename table entries
		 * are cleared.
		 */
		(void) memset(hashchn, 0, (crle->c_hashstrnum * sizeof (Word)));
		(void) memset(dirtbl, 0, (strtbl - (char *)dirtbl));

		/*
		 * Loop through the current string table list inspecting only
		 * directories.
		 */
		for (ndx = 1, bkt = 0; bkt < stbl->t_size; bkt++) {
			for (ent = stbl->t_entry[bkt]; ent; ent = ent->e_next) {
				Word		hashval;
				Hash_obj *	obj = ent->e_obj;
				char *		dir = (char *)ent->e_key;

				/*
				 * Skip any empty and non-directory entries.
				 */
				if ((obj == 0) || obj->o_dir)
					continue;
				/*
				 * Assign basic object attributes.
				 */
				objtbl->co_hash = ent->e_hash;
				objtbl->co_id = obj->o_id;
				objtbl->co_flags = obj->o_flags;
				objtbl->co_info = obj->o_info;

				/*
				 * Assign the directory name (from its key),
				 * and copy its name to the string table.
				 */
				objtbl->co_name = (Addr)(_strtbl - strtbl);
				(void) strcpy(_strtbl, dir);
				_strtbl += strlen(dir) + 1;

				/*
				 * If this is the real directory name establish
				 * a directory entry and reserve space for its
				 * associated filename entries.
				 */
				if (dir == obj->o_rpath) {
					Rtc_dir *	_dirtbl;
					int		_id = obj->o_id;

					_dirtbl = &dirtbl[_id - 1];
					_dirtbl->cd_file =
					    (Word)((char *)filetbl - addr);
					_dirtbl->cd_obj =
					    (Word)((char *)objtbl - addr);

					/* LINTED */
					filetbl = (Rtc_file *)((char *)filetbl +
					    ((obj->o_cnt + 1) *
					    sizeof (Rtc_file)));

					objtbl->co_flags |= RTC_OBJ_REALPTH;
				}

				hashval = ent->e_hash % hashbkts;
				hashchn[ndx] = hashbkt[hashval];
				hashbkt[hashval] = ndx++;

				/*
				 * Increment Rt_obj pointer (make sure pointer
				 * falls on an 8-byte boundary).
				 */
				objtbl = (Rtc_obj *)S_ROUND((int)(objtbl + 1),
				    sizeof (Lword));
			}
		}

		/*
		 * Now collect all real pathnames.
		 */
		for (bkt = 0; bkt < stbl->t_size; bkt++) {
			for (ent = stbl->t_entry[bkt]; ent; ent = ent->e_next) {
				Word		hashval;
				Hash_obj *	obj = ent->e_obj;
				char *		file = (char *)ent->e_key;
				char *		_str;
				Rtc_dir *	_dirtbl;
				Rtc_file *	_filetbl;
				int		_id;

				/*
				 * Skip empty and directory entries, and any
				 * simple filname entries.
				 */
				if ((obj == 0) ||
				    (obj->o_dir == 0) || (file != obj->o_rpath))
					continue;

				/*
				 * Assign basic object attributes.
				 */
				objtbl->co_hash = ent->e_hash;
				objtbl->co_id = obj->o_id;
				objtbl->co_flags = obj->o_flags;
				objtbl->co_info = obj->o_info;

				/*
				 * Assign the file name (from its key),
				 * and copy its name to the string table.
				 */
				objtbl->co_name = (Addr)(_strtbl - strtbl);
				(void) strcpy(_strtbl, file);
				_strtbl += strlen(file) + 1;

				_dirtbl = &dirtbl[obj->o_id - 1];
				/* LINTED */
				_filetbl = (Rtc_file *)
				    ((char *)_dirtbl->cd_file + addr);

				_id = --obj->o_dir->o_cnt;
				_filetbl[_id].cf_obj =
				    (Word)((char *)objtbl - addr);

				/*
				 * If object has an alternative, record it in
				 * the string table and assign the alter pointer
				 * so that any alias filename processing picks
				 * it up.
				 */
				if (objtbl->co_flags & RTC_OBJ_ALTER) {
					_str = obj->o_alter;
					objtbl->co_alter =
					    (Addr)(_strtbl - strtbl);
					(void) strcpy(_strtbl, _str);
					obj->o_alter = _strtbl;
					_strtbl += strlen(_str) + 1;
				} else
					objtbl->co_alter = 0;

				/*
				 * If object identifies the specific application
				 * for which this cache is relevant, record it
				 * in the header.
				 */
				if (objtbl->co_flags & RTC_OBJ_EXEC)
					head->ch_app = _filetbl[_id].cf_obj;

				objtbl->co_flags |= RTC_OBJ_REALPTH;

				hashval = ent->e_hash % hashbkts;
				hashchn[ndx] = hashbkt[hashval];
				hashbkt[hashval] = ndx++;

				/*
				 * Increment Rt_obj pointer (make sure pointer
				 * falls on an 8-byte boundary).
				 */
				objtbl = (Rtc_obj *)S_ROUND((int)(objtbl + 1),
				    sizeof (Lword));
			}
		}

		/*
		 * Finally pick off any alias filenames.
		 */
		for (bkt = 0; bkt < stbl->t_size; bkt++) {
			for (ent = stbl->t_entry[bkt]; ent; ent = ent->e_next) {
				Word		hashval;
				Hash_obj *	obj = ent->e_obj;
				char *		file = (char *)ent->e_key;

				/*
				 * Skip everything except simple filenames.
				 */
				if ((obj == 0) || (obj->o_dir == 0) ||
				    (file == obj->o_rpath))
					continue;

				/*
				 * Assign basic object attributes.
				 */
				objtbl->co_hash = ent->e_hash;
				objtbl->co_id = obj->o_id;
				objtbl->co_flags = obj->o_flags;
				objtbl->co_info = obj->o_info;

				/*
				 * Assign the file name (from its key),
				 * and copy its name to the string table.
				 */
				objtbl->co_name = (Addr)(_strtbl - strtbl);
				(void) strcpy(_strtbl, file);
				_strtbl += strlen(file) + 1;

				/*
				 * Assign any alternative (from previously saved
				 * alternative).
				 */
				if (objtbl->co_flags & RTC_OBJ_ALTER) {
					objtbl->co_alter =
					    (Addr)(obj->o_alter - strtbl);
				}

				hashval = ent->e_hash % hashbkts;
				hashchn[ndx] = hashbkt[hashval];
				hashbkt[hashval] = ndx++;

				/*
				 * Increment Rt_obj pointer (make sure pointer
				 * falls on an 8-byte boundary).
				 */
				objtbl = (Rtc_obj *)S_ROUND((int)(objtbl + 1),
				    sizeof (Lword));
			}
		}
	}

	/*
	 * Add any library, or secure path definitions.
	 */
	if (crle->c_edlibpath) {
		head->ch_edlibpath = head->ch_str + (_strtbl - strtbl);

		(void) strcpy(_strtbl, crle->c_edlibpath);
		_strtbl += strlen((char *)crle->c_edlibpath) + 1;
	} else
		head->ch_edlibpath = 0;

	if (crle->c_adlibpath) {
		head->ch_adlibpath = head->ch_str + (_strtbl - strtbl);

		(void) strcpy(_strtbl, crle->c_adlibpath);
		_strtbl += strlen((char *)crle->c_adlibpath) + 1;
	} else
		head->ch_adlibpath = 0;

	if (crle->c_eslibpath) {
		head->ch_eslibpath = head->ch_str + (_strtbl - strtbl);

		(void) strcpy(_strtbl, crle->c_eslibpath);
		_strtbl += strlen((char *)crle->c_eslibpath) + 1;
	} else
		head->ch_eslibpath = 0;

	if (crle->c_aslibpath) {
		head->ch_aslibpath = head->ch_str + (_strtbl - strtbl);

		(void) strcpy(_strtbl, crle->c_aslibpath);
		_strtbl += strlen((char *)crle->c_aslibpath) + 1;
	} else
		head->ch_aslibpath = 0;

	/*
	 * Flush everything out.
	 */
	(void) close(crle->c_tempfd);
	if (msync((void *)addr, size, MS_ASYNC) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_TRUNC),
		    crle->c_name, crle->c_tempname, strerror(err));
		return (1);
	}

	return (0);
}


/*
 * Update a configuration file.  If dldump()'ed images have been created then
 * the memory reservation of those images is added to the configuration file.
 * The temporary file is then moved into its final resting place.
 */
updateconfig(Crle_desc * crle)
{
	Rtc_head *	head = (Rtc_head *)crle->c_tempaddr;

	if (crle->c_flags & CRLE_DUMP) {
		head->ch_cnflags &= ~RTC_HDR_IGNORE;

		if (msync((void *)crle->c_tempaddr, crle->c_tempsize,
		    MS_ASYNC) == -1) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_TRUNC),
			    crle->c_name, crle->c_tempname, strerror(err));
			return (1);
		}
	}

	/*
	 * If an original configuration file exists, remove it.
	 */
	if (crle->c_flags & CRLE_EXISTS)
		(void) unlink(crle->c_confil);

	/*
	 * Move the config file to its final resting place.  If the two files
	 * exist on the same filesystem a rename is sufficient.
	 */
	if (crle->c_flags & CRLE_DIFFDEV) {
		int	fd;

		if ((fd = open(crle->c_confil, (O_RDWR | O_CREAT | O_TRUNC),
		    0666)) == -1) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
			    crle->c_name, crle->c_confil, strerror(err));
			return (1);
		}
		if (write(fd, (void *)crle->c_tempaddr, crle->c_tempsize) !=
		    crle->c_tempsize) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_WRITE),
			    crle->c_name, crle->c_confil, strerror(err));
			return (1);
		}
		(void) close(fd);
		(void) unlink(crle->c_tempname);
	} else
		(void) rename(crle->c_tempname, crle->c_confil);

	return (0);
}
