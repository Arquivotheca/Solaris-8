/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)inspect.c	1.1	99/09/02 SMI"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<dirent.h>
#include	<libelf.h>
#include	<gelf.h>
#include	<errno.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<limits.h>
#include	"machdep.h"
#include	"sgs.h"
#include	"rtc.h"
#include	"_crle.h"
#include	"msg.h"

/*
 * Routines to add file and directory entries into the internal configuration
 * information.  This information is maintained in a number of hash tables which
 * after completion of input file processing will be processed and written to
 * the output configuration file.
 *
 * Each hash table is defined via a Hash_tbl structure.  These are organized:
 *
 *  c_strtbl	contains a hash entry for every file, directory, pathname and
 *		alternative path (dldump(3x) image) processed.
 *		c_strsize and c_objnum maintain the size and count of the
 *		strings added to this table and are used to size the output
 *		configuration file.
 *
 *  c_inotbls	contains a list of inode hash tables.  Each element of the list
 *		identifies a unique device.  Thus, for each file processed its
 *		st_dev and st_ino are used to assign its entry to the correct
 *		hash table.
 *
 *		Each directory processed is assigned a unique id (c_dirnum)
 *		which insures each file also becomes uniquely identified.
 *
 * All file and directory additions come through the inspect() entry point.
 */


static int
enteralt(Crle_desc * crle, Hash_obj * obj, const char * file, int flags)
{
	char		alter[PATH_MAX];
	size_t		altsz;

	/*
	 * Create an alternative pathname from the file and object destination
	 * directory.  If we're dumping an alternative don't allow it to
	 * override the original.
	 */
	(void) sprintf(alter, MSG_ORIG(MSG_FMT_PATH), crle->c_objdir, file);

	if (flags & CRLE_DUMP) {
		(void) realpath(alter, alter);
		if (strcmp(alter, obj->o_rpath) == 0) {
			(void) printf(MSG_INTL(MSG_ARG_ALT), crle->c_name,
			    obj->o_rpath);
			return (0);
		}
	}

	altsz = strlen(alter) + 1;
	if ((obj->o_alter = malloc(altsz)) == 0)
		return (0);
	(void) strcpy(obj->o_alter, alter);

	if (flags & CRLE_DUMP)
		obj->o_flags |= (RTC_OBJ_ALTER | RTC_OBJ_DUMP);
	else
		obj->o_flags |= RTC_OBJ_ALTER;

	crle->c_strsize += altsz;

	if (crle->c_flags & CRLE_VERBOSE)
		(void) printf(MSG_INTL(MSG_DIA_ALTERNATE), obj->o_id, alter);

	return (1);
}

static Hash_obj *
enterfile(Crle_desc * crle, const char * path, const char * file,
    Hash_obj * dobj, struct stat * status)
{
	Hash_tbl *	stbl = crle->c_strtbl;
	Hash_ent *	ent;
	Hash_obj *	obj;
	char		_path[PATH_MAX], * rpath = _path, * rfile;
	size_t		size;

	/*
	 * Add this new file to the inode hash table.
	 */
	if ((ent = get_hash(dobj->o_tbl, (Addr)status->st_ino,
	    (HASH_FND_ENT | HASH_ADD_ENT))) == 0)
		return (0);

	/*
	 * Establish the files real name - it is the real name that is used to
	 * record individual file objects, alias names will be assigned copies
	 * of the object.
	 */
	if (realpath(path, rpath) == 0)
		return (0);

	/*
	 * If an object doesn't yet exist, create one for the real file.
	 */
	if ((obj = ent->e_obj) == 0) {

		if ((obj = calloc(sizeof (Hash_obj), 1)) == 0)
			return (0);
		obj->o_info = (Lword)status->st_size;
		obj->o_id = dobj->o_id;
		obj->o_dir = dobj;
		obj->o_dir->o_cnt++;

		/*
		 * Assign this object to the orignal ino hash entry.
		 */
		ent->e_obj = obj;

		/*
		 * Save and record the real name.
		 */
		size = strlen(rpath) + 1;
		if ((obj->o_rpath = malloc(size)) == 0)
			return (0);
		(void) strcpy(obj->o_rpath, rpath);
		rpath = obj->o_rpath;

		/*
		 * Add this real path to the string hash table and reference
		 * the object data structure.
		 */
		if ((ent = get_hash(stbl, (Addr)rpath, HASH_ADD_ENT)) == 0)
			return (0);

		ent->e_obj = obj;

		crle->c_strsize += size;
		crle->c_hashstrnum++;
		crle->c_filenum++;

		if (crle->c_flags & CRLE_VERBOSE)
			(void) printf(MSG_INTL(MSG_DIA_RFILE), obj->o_id,
			    rpath);

		/*
		 * Add a file to the string hash table and reference the same
		 * object.  Note we reuse the basename portion of the real path
		 * string to reduce the string size in the final configuration
		 * file.
		 */
		if (rfile = strrchr(rpath, '/'))
			rfile++;
		else
			rfile = rpath;

		if ((ent = get_hash(stbl, (Addr)rfile,
		    (HASH_FND_ENT | HASH_ADD_ENT))) == 0)
			return (0);

		if (ent->e_obj == 0) {
			ent->e_obj = obj;

			crle->c_strsize += strlen(rfile) + 1;
			crle->c_hashstrnum++;

			if (crle->c_flags & CRLE_VERBOSE)
				(void) printf(MSG_INTL(MSG_DIA_AFILE),
				    obj->o_id, rfile);
		}
	}

	/*
	 * If the original path name is not equivalent to the real path name,
	 * then we have an alias (typically it's a symlink).  Add the path name
	 * to the string hash table and reference the object data structure.
	 */
	if ((path != rpath) && strcmp(path, rpath)) {
		char *	opath, * ofile;

		size = strlen(path) + 1;
		if ((opath = malloc(size)) == 0)
			return (0);
		(void) strcpy(opath, path);

		if ((ent = get_hash(stbl, (Addr)opath, HASH_ADD_ENT)) == 0)
			return (0);

		if (ent->e_obj == 0) {
			ent->e_obj = obj;

			crle->c_strsize += size;
			crle->c_hashstrnum++;

			if (crle->c_flags & CRLE_VERBOSE)
				(void) printf(MSG_INTL(MSG_DIA_AFILE),
				    obj->o_id, opath);
		}

		/*
		 * Determine if we also need a filename alias
		 */
		if (ofile = strrchr(opath, '/'))
			ofile++;
		else
			ofile = opath;

		if ((ent = get_hash(stbl, (Addr)ofile,
		    (HASH_FND_ENT | HASH_ADD_ENT))) == 0)
			return (0);

		if (ent->e_obj == 0) {
			ent->e_obj = obj;

			crle->c_strsize += strlen(ofile) + 1;
			crle->c_hashstrnum++;

			if (crle->c_flags & CRLE_VERBOSE)
				(void) printf(MSG_INTL(MSG_DIA_AFILE),
				    obj->o_id, ofile);
		}
	}
	return (obj);
}

static Hash_obj *
enterdir(Crle_desc * crle, const char * dir, struct stat * status)
{
	Hash_tbl *	stbl = crle->c_strtbl;
	Hash_ent *	ent;
	Hash_obj *	obj;
	Hash_tbl *	tbl;
	Listnode *	lnp = 0;
	Addr		ino;
	ulong_t		dev;
	Half		flags = 0;
	Lword		info;
	char		rdir[PATH_MAX], * ndir = rdir;
	size_t		size;

	/*
	 * If we have no status then we're creating a non-existent directory.
	 */
	if (status == 0) {
		ino = crle->c_noexistnum++;
		dev = 0;
		info = 0;
		flags = RTC_OBJ_NOEXIST;
	} else {
		ino = (Addr)status->st_ino;
		dev = status->st_dev;
		info = (Lword)status->st_mtime;

		/*
		 * Determine the directory dev number and establish a hash table
		 * for this devices inodes.
		 */
		for (LIST_TRAVERSE(&crle->c_inotbls, lnp, tbl)) {
			if (tbl->t_ident == dev)
				break;
		}
	}
	if (lnp == 0) {
		if ((tbl = make_hash(crle->c_inobkts, HASH_INT, dev)) == 0)
			return (0);
		if (list_append(&crle->c_inotbls, tbl) == 0)
			return (0);
	}

	/*
	 * Add this new directory to the inode hash table.
	 */
	if ((ent = get_hash(tbl, ino, (HASH_FND_ENT | HASH_ADD_ENT))) == 0)
		return (0);

	/*
	 * Establish the directories real name - it is the real name that is
	 * used to record individual directory objects, alias names will be
	 * assigned copies of the object.
	 */
	if (dev == 0)
		ndir = (char *)dir;
	else {
		if (realpath(dir, ndir) == 0)
			return (0);
	}

	/*
	 * If an object doesn't yet exist, create one for the real directory.
	 */
	if ((obj = ent->e_obj) == 0) {
		if ((obj = calloc(sizeof (Hash_obj), 1)) == 0)
			return (0);
		obj->o_id = crle->c_dirnum++;
		obj->o_tbl = tbl;
		obj->o_flags = (RTC_OBJ_DIRENT | flags);
		obj->o_info = info;

		/*
		 * Assign this object to the original ino hash entry.
		 */
		ent->e_obj = obj;

		/*
		 * Save and record the real name.
		 */
		size = strlen(ndir) + 1;
		if ((obj->o_rpath = malloc(size)) == 0)
			return (0);
		(void) strcpy(obj->o_rpath, ndir);
		ndir = obj->o_rpath;

		/*
		 * Add this real directory name to the string hash table and
		 * reference the object data structure.
		 */
		if ((ent = get_hash(stbl, (Addr)ndir, HASH_ADD_ENT)) == 0)
			return (0);

		ent->e_obj = obj;

		crle->c_strsize += size;
		crle->c_hashstrnum++;

		/*
		 * We add a dummy filename for each real directory so as to
		 * have a null terminated file table array for this directory.
		 */
		crle->c_filenum++;

		if (crle->c_flags & CRLE_VERBOSE) {
			if (obj->o_flags & RTC_OBJ_NOEXIST)
				(void) printf(MSG_INTL(MSG_DIA_NOEXIST),
				    obj->o_id, ndir);
			else
				(void) printf(MSG_INTL(MSG_DIA_RDIR),
				    obj->o_id, ndir);
		}
	}

	/*
	 * If the original directory name is not equivalent to the real
	 * directory name, then we have an alias (typically its a symlink).
	 * Add the directory name to the string hash table and reference the
	 * object data structure.
	 */
	if ((dir != ndir) && strcmp(dir, ndir)) {

		size = strlen(dir) + 1;
		if ((ndir = malloc(size)) == 0)
			return (0);
		(void) strcpy(ndir, dir);

		if ((ent = get_hash(stbl, (Addr)ndir, HASH_ADD_ENT)) == 0)
			return (0);

		ent->e_obj = obj;

		crle->c_strsize += size;
		crle->c_hashstrnum++;

		if (crle->c_flags & CRLE_VERBOSE)
			(void) printf(MSG_INTL(MSG_DIA_ADIR), obj->o_id, ndir);
	}

	return (obj);
}

static int
inspect_file(Crle_desc * crle, const char * path, const char * file,
    Hash_obj * dobj, int flags, struct stat * status, int error)
{
	Hash_ent *	ent;
	Hash_obj *	obj;
	int		fd;
	Elf *		elf;
	GElf_Ehdr	ehdr;
	Xword		dyflags = 0;

	/*
	 * Determine whether this file has already been processed.
	 */
	if ((ent = get_hash(crle->c_strtbl, (Addr)file, HASH_FND_ENT)) != 0) {
		obj = ent->e_obj;
		if (obj->o_flags & RTC_OBJ_NOEXIST)
			return (0);

		if ((flags & CRLE_ALTER) &&
		    ((obj->o_flags & (RTC_OBJ_ALTER | RTC_OBJ_NOALTER)) == 0)) {
			if (enteralt(crle, obj, file, flags) == 0)
				return (1);
		}
		return (0);
	}

	/*
	 * Determine if this file is a valid ELF file.
	 */
	if ((fd = open(path, O_RDONLY, 0)) == -1) {
		if (error) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
			    crle->c_name, file, strerror(err));
		}
		return (error);
	}

	/*
	 * Obtain an ELF descriptor and determine if we have a shared object.
	 */
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		if (error)
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_BEGIN),
			    crle->c_name, file, elf_errmsg(-1));
		(void) close(fd);
		return (error);
	}
	if ((elf_kind(elf) != ELF_K_ELF) ||
	    (gelf_getehdr(elf, &ehdr) == NULL) ||
	    (!((ehdr.e_type == ET_EXEC) || (ehdr.e_type == ET_DYN))) ||
	    (!((ehdr.e_ident[EI_CLASS] == crle->c_class) ||
	    (ehdr.e_machine == crle->c_machine)))) {
		if (error)
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_TYPE),
			    crle->c_name, file);
		(void) close(fd);
		(void) elf_end(elf);
		return (error);
	}

	(void) close(fd);

	/*
	 * If we're generating alternative objects find this objects DT_FLAGS
	 * to insure it isn't marked as non-dumpable (libdl.so.1 falls into
	 * this category).
	 */
	if (flags & CRLE_ALTER) {
		Elf_Scn *	scn = NULL;
		Elf_Data *	data;
		GElf_Shdr	shdr;
		GElf_Dyn	dyn;

		while (scn = elf_nextscn(elf, scn)) {
			int	num, _num;

			if (gelf_getshdr(scn, &shdr) == NULL)
				break;
			if (shdr.sh_type != SHT_DYNAMIC)
				continue;
			if ((data = elf_getdata(scn, NULL)) == NULL)
				break;

			num = shdr.sh_size / shdr.sh_entsize;
			for (_num = 0; _num < num; _num++) {
				(void) gelf_getdyn(data, _num, &dyn);
				if (dyn.d_tag != DT_FLAGS_1)
					continue;

				dyflags = dyn.d_un.d_val;
				break;
			}
			break;
		}
	}

	/*
	 * Executables aren't added to the configuration cache unless a specific
	 * application cache is being generated.  This latter case is identified
	 * when RTLD_REL_EXEC is in effect, and because the cache is specific to
	 * this application only one executable may be specified.
	 * Typically executables are used to gather dependencies.  If we come
	 * across an executable while searching a directory (error == 0) it is
	 * ignored.
	 */
	if (ehdr.e_type == ET_EXEC) {
		if (error == 0) {
			(void) elf_end(elf);
			return (0);
		}

		/*
		 * If we've not dumping the executable itself then simply
		 * determine its dependencies.
		 */
		if ((crle->c_dlflags & RTLD_REL_EXEC) == 0) {
			/*
			 * If we've not being asked for dependencies then the
			 * executable is rather useless.
			 */
			if ((flags & CRLE_GROUP) == 0) {
				(void) fprintf(stderr,
				    MSG_INTL(MSG_GEN_INVFILE), crle->c_name,
				    file);
				error = 1;
			} else
				error = depend(crle, path, &ehdr, flags);

			(void) elf_end(elf);
			return (error);
		}

		/*
		 * Record this object, thus creating an application specific
		 * configuration file.
		 */
		if (crle->c_app) {
			(void) fprintf(stderr, MSG_INTL(MSG_ARG_MODE),
			    crle->c_name);
			(void) elf_end(elf);
			return (1);
		}
		crle->c_app = path;
	}

	/*
	 * Enter the object in the configuration cache.
	 */
	if ((obj = enterfile(crle, path, file, dobj, status)) == 0) {
		(void) elf_end(elf);
		return (1);
	}

	if (flags & CRLE_ALTER) {
		if (dyflags & DF_1_NODUMP) {
			obj->o_flags |= RTC_OBJ_NOALTER;
		} else {
			if (((obj->o_flags & RTC_OBJ_ALTER) == 0) &&
			    (enteralt(crle, obj, file, flags) == 0)) {
				(void) elf_end(elf);
				return (1);
			}
		}
	}

	if (ehdr.e_type == ET_EXEC)
		obj->o_flags |= RTC_OBJ_EXEC;

	/*
	 * If we've been asked to process this object as a group determine its
	 * dependencies.
	 */
	if (flags & CRLE_GROUP) {
		if (depend(crle, path, &ehdr, flags) != 0) {
			(void) elf_end(elf);
			return (1);
		}
	}

	(void) elf_end(elf);
	return (0);
}

/*
 * Add a directory to configuration information.
 */
static int
inspect_dir(Crle_desc * crle, const char * name, int flags,
    struct stat * status)
{
	DIR *		dir;
	struct dirent *	dirent;
	Hash_obj *	obj;
	Hash_ent *	ent;
	int		error = 0;
	struct stat	_status;
	char		path[PATH_MAX], * dst;
	const char *	src;

	/*
	 * Determine whether we've already visited this directory to process
	 * all its entries.
	 */
	if ((ent = get_hash(crle->c_strtbl, (Addr)name, HASH_FND_ENT)) != 0) {
		obj = ent->e_obj;
		if (obj->o_flags & RTC_OBJ_ALLENTS)
			return (0);
	} else {
		/*
		 * Create a directory hash entry.
		 */
		if ((obj = enterdir(crle, name, status)) == 0)
			return (1);
		obj->o_flags |= RTC_OBJ_ALLENTS;
	}

	/*
	 * Establish the pathname buffer.
	 */
	for (dst = path, dst--, src = name; *src; src++)
		*++dst = *src;
	if (*dst++ != '/')
		*dst++ = '/';

	/*
	 * Access the directory in preparation for reading its entries.
	 */
	if ((dir = opendir(name)) == 0)
		return (1);

	/*
	 * Read each entry from the directory looking for ELF files.
	 */
	while ((dirent = readdir(dir)) != NULL) {
		const char *	file = dirent->d_name;
		char *		_dst;

		/*
		 * Ignore "." and ".." entries.
		 */
		if ((file[0] == '.') && ((file[1] == '\0') ||
		    ((file[1] == '.') && (file[2] == '\0'))))
			continue;

		/*
		 * Complete full pathname.
		 */
		for (_dst = dst, src = file; *src; _dst++, src++)
			*_dst = *src;
		*_dst = '\0';

		if (stat(path, &_status) == -1)
			continue;

		if ((_status.st_mode & S_IFMT) != S_IFREG)
			continue;

		if (inspect_file(crle, path, file, obj, flags, &_status, 0)) {
			error = 1;
			break;
		}
	}
	return (error);
}


/*
 * Inspect a file/dir name.  A stat(name) results in the following actions:
 *
 * The name doesn't exist:
 *	The name is assummed to be a non-existant directory and a directory
 *	cache entry is created to indicate this.
 *
 * The name is a directory:
 *	The directory is searched for appropriate files.
 *
 * The name is a file:
 *	The file is processed and added to the cache if appropriate.
 */
int
inspect(Crle_desc * crle, const char * name, int flags)
{
	Hash_obj *	obj;
	Hash_ent *	ent;
	const char *	file, * dir;
	struct stat	status;
	char		_name[PATH_MAX], _dir[PATH_MAX];

	/*
	 * If this is the first time through here establish a string table
	 * cache.
	 */
	if (crle->c_dirnum == 0) {
		if ((crle->c_strtbl = make_hash(crle->c_strbkts,
		    HASH_STR, 0)) == 0)
			return (0);
		crle->c_dirnum = 1;
	}

	/*
	 * Determine whether the name exists.
	 */
	if (stat(name, &status) != 0) {
		if (errno != ENOENT) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_STAT),
			    crle->c_name, name, strerror(err));
			return (1);
		} else {
			/*
			 * Establish an object descriptor to mark this as
			 * non-existent.
			 */
			if ((obj = enterdir(crle, name, 0)) == 0)
				return (1);

			return (0);
		}
	}

	/*
	 * Determine whether we're dealing with a directory or a file.
	 */
	if ((status.st_mode & S_IFMT) == S_IFDIR) {
		/*
		 * Process the directory name to collect its shared objects into
		 * the configuration file.
		 */
		return (inspect_dir(crle, name, flags, &status));
	}

	/*
	 * If this isn't a regular file we might as well bail now.  Note that
	 * even if it is, we might still reject the file if it's not ELF later
	 * in inspect_file().
	 */
	if ((status.st_mode & S_IFMT) != S_IFREG) {
		(void) fprintf(stderr, MSG_INTL(MSG_GEN_INVFILE), crle->c_name,
		    name);
		return (1);
	}

	/*
	 * Break the pathname into directory and filename components.
	 */
	if ((file = strrchr(name, '/')) == 0) {
		dir = MSG_ORIG(MSG_DIR_DOT);
		(void) strcpy(_name, MSG_ORIG(MSG_PTH_DOT));
		(void) strcpy(&_name[MSG_PTH_DOT_SIZE], name);
		name = (const char *)_name;
		file = (const char *)&_name[MSG_PTH_DOT_SIZE];
	} else {
		size_t	off = file - name;

		if (file == name)
			dir = MSG_ORIG(MSG_DIR_ROOT);
		else {
			(void) strncpy(_dir, name, off);
			_dir[off] = '\0';
			dir = (const char *)_dir;
		}
		file++;
	}

	/*
	 * Determine whether we've already visited this directory to process
	 * other dependencies.
	 */
	if ((ent = get_hash(crle->c_strtbl, (Addr)dir, HASH_FND_ENT)) != 0) {
		obj = ent->e_obj;
		if (obj->o_flags & RTC_OBJ_ALLENTS)
			return (0);
	} else {
		struct stat	_status;

		(void) stat(dir, &_status);
		if ((obj = enterdir(crle, dir, &_status)) == 0)
			return (1);
	}

	return (inspect_file(crle, name, file, obj, flags, &status, 1));
}
