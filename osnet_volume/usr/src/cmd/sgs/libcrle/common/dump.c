/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dump.c	1.1	99/09/08 SMI"

/* LINTLIBRARY */

#include	<sys/mman.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<procfs.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<limits.h>
#include	<errno.h>
#include	"rtld.h"
#include	"rtc.h"
#include	"_crle.h"
#include	"msg.h"

/*
 * Routines for dumping alternate objects under CRLE_AUD_DLDUMP mode.
 */
static Addr	membgn = 0;
static Addr	memend = 0;

/*
 * For each file in the configuration file that requires an alternate (dldump())
 * version, add the object to the processes main link-map.  The process head
 * may be an application, shared object, or lddstub.  In any case this object
 * may be augmented with other objects defined within the configuration file.
 *
 * Each file is loaded with RTLD_NOFIXUP so that no dependency analysis,
 * relocation or user code (.init's) is executed.  By skipping analysis we save
 * time and allow for a family of objects to be dumped that may not have all
 * relocations satisfied.  From each objects link-map maintain a mapping range
 * which will be written back to the caller.
 */
static int
/* ARGSUSED1 */
load(const char * opath, const char * npath)
{
	Dl_handle *	dlp;
	Rt_map *	lmp;
	Addr		_membgn, _memend;

	if ((dlp = (Dl_handle *)dlmopen(LM_ID_BASE, opath,
	    (RTLD_NOW | RTLD_GLOBAL | RTLD_CONFGEN))) == NULL) {
		(void) fprintf(stderr, MSG_INTL(MSG_DL_OPEN),
		    MSG_ORIG(MSG_FIL_LIBCRLE), dlerror());
		return (1);
	}
	lmp = (Rt_map *)(dlp->dl_depends.head->data);
	FLAGS1(lmp) |= FL1_RT_CONFSET;

	/*
	 * Establish the mapping range of the objects dumped so far.
	 */
	_membgn = ADDR(lmp);
	_memend = (ADDR(lmp) + MSIZE(lmp));

	if (membgn == 0) {
		membgn = _membgn;
		memend = _memend;
	} else {
		if (membgn > _membgn)
			membgn = _membgn;
		if (memend < _memend)
			memend = _memend;
	}
	return (0);
}

/*
 * dldump(3x) an object that is already part of the main link-map list.
 */
static int
dump(const char * opath, const char * npath)
{
	(void) unlink(npath);

	if (dldump(opath, npath, dlflag) != 0) {
		(void) fprintf(stderr, MSG_INTL(MSG_DL_DUMP),
		    MSG_ORIG(MSG_FIL_LIBCRLE), dlerror());
		return (1);
	}
	return (0);
}

/*
 * Traverse a configuration file directory/file list.  Each file within the
 * list is maintained as both a full pathname and a simple filename - we're
 * only interested in one.
 *
 * This rutine is called twice, once to insure the appropriate objects are
 * mapped in (fptr == load()) and then once again to dldump(3x) the mapped
 * objects (fptr == dump()).
 */
static int
scanconfig(Addr addr, int (*fptr)())
{
	Rtc_head *	head = (Rtc_head *)addr;
	Rtc_obj *	obj;
	Rtc_dir *	dirtbl;
	Rtc_file *	filetbl;
	const char *	str, * strtbl;

	/* LINTED */
	strtbl = (const char *)((char *)head->ch_str + addr);

	/*
	 * Scan the directory and filename arrays looking for alternatives.
	 */
	for (dirtbl = (Rtc_dir *)(head->ch_dir + addr);
	    dirtbl->cd_obj; dirtbl++) {

		obj = (Rtc_obj *)(dirtbl->cd_obj + addr);
		str = strtbl + obj->co_name;

		if (obj->co_flags & RTC_OBJ_NOEXIST)
			continue;

		for (filetbl = (Rtc_file *)(dirtbl->cd_file + addr);
		    filetbl->cf_obj; filetbl++) {

			obj = (Rtc_obj *)(filetbl->cf_obj + addr);
			str = strtbl + obj->co_name;

			if ((obj->co_flags &
			    (RTC_OBJ_DUMP | RTC_OBJ_EXEC)) == RTC_OBJ_DUMP) {
				if ((*fptr)(str, strtbl + obj->co_alter) != 0)
					return (1);
			}
		}
	}

	/*
	 * Are we dumping a specific application.
	 */
	if (head->ch_app) {
		if (fptr == load) {
			Dl_handle *	dlp;

			/*
			 * Make sure RTLD_NOW variable is set in the head link-
			 * map and indicate the application can be bound to.
			 */
			if ((dlp = dlmopen(LM_ID_BASE, 0,
			    (RTLD_NOLOAD | RTLD_NOW | RTLD_CONFGEN))) == 0)
				return (1);
			FLAGS1(HDLHEAD(dlp)) |= FL1_RT_CONFSET;

		} else {
			/*
			 * If we're dumping and this configuration is for a
			 * specific application dump it also.
			 */
			/* LINTED */
			obj = (Rtc_obj *)((char *)head->ch_app + addr);
			str = strtbl + obj->co_alter;

			if (dump((const char *)0, str) != 0)
				return (1);
		}
	}
	return (0);
}

/*
 * Before loading any dependencies determine the present memory mappings being
 * used and fill any holes between these mappings.  This insures that all
 * dldump()'ed dependencies will live in a single consecutive address range.
 */
int
filladdr(void)
{
	prmap_t *	maps, * _maps;
	struct stat	status;
	int		fd, err, num, _num;
	size_t		size;
	uintptr_t	laddr = 0;

	/*
	 * Open /proc/self/rmap to obtain the processes reserved mappings.
	 */
	if ((fd = open(MSG_ORIG(MSG_PTH_PROCRMAP), O_RDONLY)) == -1) {
		err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
		    MSG_ORIG(MSG_FIL_LIBCRLE), MSG_ORIG(MSG_PTH_PROCRMAP),
		    strerror(err));
		return (1);
	}
	(void) fstat(fd, &status);

	/*
	 * Determine number of mappings - add one, as it's possible the
	 * following malloc() will produce another mapping.
	 */
	num = status.st_size / sizeof (prmap_t);
	size = (num + 1) * sizeof (prmap_t);

	if ((maps = malloc(size)) == 0) {
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_ALLOC),
		    MSG_ORIG(MSG_FIL_LIBCRLE), strerror(ENOMEM));
		(void) close(pfd);
		return (1);
	}

	if (read(fd, (void *)maps, size) < 0) {
		err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_READ),
		    MSG_ORIG(MSG_FIL_LIBCRLE), MSG_ORIG(MSG_PTH_PROCRMAP),
		    strerror(err));
		(void) close(fd);
		free(maps);
		return (1);
	}
	(void) close(fd);

	/*
	 * Use /dev/null for filling holes.
	 */
	if ((fd = open(MSG_ORIG(MSG_PTH_DEVNULL), O_RDONLY)) == -1) {
		err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
		    MSG_ORIG(MSG_FIL_LIBCRLE), MSG_ORIG(MSG_PTH_DEVNULL),
		    strerror(err));
		free(maps);
		return (1);
	}

	/*
	 * Scan each mapping - note it is assummed that the mappings are
	 * presented in order.  We fill holes between mappings.  On intel
	 * the last mapping is usually the data segment of ld.so.1, after
	 * this comes a red zone into which non-fixed mapping won't get
	 * place.  Thus we can simply bail from the loop after seeing the
	 * last mapping.
	 */
	for (_num = 0, _maps = maps; _num <= num; _num++, _maps++) {

		/*
		 * The initial mappings represent the a.out, typically a text
		 * and data segment - skip these as they typically reside in
		 * low memory.  If we've already captured a mapping (laddr)
		 * then we could be processing an executable shared object.
		 */
		if ((laddr == 0) && ((_maps->pr_mapname[0] == 'a') &&
		    (_maps->pr_mapname[1] == '.') &&
		    (_maps->pr_mapname[2] == 'o') &&
		    (_maps->pr_mapname[3] == 'u') &&
		    (_maps->pr_mapname[4] == 't') &&
		    (_maps->pr_mapname[5] == '\0')))
			continue;

		/*
		 * Any heap resides at low memory and so skip (typical use of
		 * libcrle.so is processing an object without it being executed
		 * so there's not going to be any heap - test is here simply for
		 * completness).
		 */
		if (_maps->pr_mflags & MA_BREAK)
			continue;

		/*
		 * On intel the stack is maintained below the a.out (laddr isn't
		 * set) so skip it.  On sparc this is the last mapping and will
		 * preceed the loop termination.
		 */
		if ((laddr == 0) && (_maps->pr_mflags & MA_STACK))
			continue;

		/*
		 * For each consecutive mapping determine the hole between each
		 * and fill it from /dev/null.
		 */
		if (laddr == 0) {
			laddr = _maps->pr_vaddr + _maps->pr_size;
			continue;
		}

		if ((size = _maps->pr_vaddr - laddr) != 0) {
			if (mmap((void *)laddr, size, PROT_NONE,
			    MAP_FIXED | MAP_PRIVATE, fd, 0) == MAP_FAILED) {
				err = errno;
				(void) fprintf(stderr, MSG_INTL(MSG_SYS_MMAP),
				    MSG_ORIG(MSG_FIL_LIBCRLE),
				    MSG_ORIG(MSG_PTH_DEVNULL), strerror(err));
				free(maps);
				return (1);
			}
		}
		laddr = _maps->pr_vaddr + _maps->pr_size;
	}

	/*
	 * Close /dev/null.  Don't free the maps we've just processed as we're
	 * using mapmalloc() we want to insure the mappings stay in place
	 * before dldump'ing.
	 */
	(void) close(fd);
	return (0);
}

/*
 * Dump alternative objects as part of building a configuration file.  A temp
 * configuration is already built and made available to the process, and is
 * located via dlinfo().  Having load()'ed each object, and dump()'ed its image,
 * the final memory reservation infoamtion is returned to the caller.
 */
int
dumpconfig(void)
{
	char		buffer[PATH_MAX];
	Addr		config;
	Dl_info		info;

	/*
	 * Determine the configuration file and where it is mapped.
	 */
	if (dlinfo((void *)NULL, RTLD_DI_CONFIGADDR, &info) == -1) {
		(void) fprintf(stderr, MSG_INTL(MSG_DL_INFO),
		    MSG_ORIG(MSG_FIL_LIBCRLE), dlerror());
		return (1);
	}
	config = (Addr)info.dli_fbase;

	/*
	 * Scan the configuration file for alternative entries.
	 */
	if (scanconfig(config, load) != 0)
		return (1);

	/*
	 * Having mapped all objects, relocate them.  It would be nice if we
	 * could drop this step altogether, and have dldump() carry out just
	 * those relocations required, but when binding to an application we
	 * need to handle copy relocations - these can affect bindings (in the
	 * case of things like libld.so which have direct bindings) and require
	 * that the dat being copied is itself relocated.
	 */
	if (dlmopen(LM_ID_BASE, 0, (RTLD_NOW | RTLD_CONFGEN)) == 0)
		return (1);

	/*
	 * Rescan the configuration dumping out each alternative file.
	 */
	if (scanconfig(config, dump) != 0)
		return (1);

	/*
	 * Having established the memory range of the dumped images and
	 * sucessfully dumped them out, report back to the caller.
	 */
	(void) sprintf(buffer, MSG_ORIG(MSG_AUD_RESBGN), EC_ADDR(membgn));
	(void) write(pfd, buffer, strlen(buffer));

	(void) sprintf(buffer, MSG_ORIG(MSG_AUD_RESEND), EC_ADDR(memend));
	(void) write(pfd, buffer, strlen(buffer));

	return (0);
}
