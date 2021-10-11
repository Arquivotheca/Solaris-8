/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)config_elf.c	1.15	99/11/03 SMI"

#include	"_synonyms.h"

#include	<sys/mman.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<stdio.h>
#include	<string.h>
#include	"rtc.h"
#include	"debug.h"
#include	"_rtld.h"
#include	"msg.h"

static Config	_config = { 0 };
Config *	config = &_config;


/*
 * Validate a configuration file.
 */
static int
elf_config_validate(Addr addr, Rtc_head * head, Rt_map * lmp)
{
	const char *	str, * strtbl = config->c_strtbl;
	Rtc_obj *	obj;
	Rtc_dir *	dirtbl;
	Rtc_file *	filetbl;
	struct stat	status;
	int		err;

	/*
	 * If this configuration file is for a specific application make sure
	 * we've been invoked by the application.  Note that we only check the
	 * basename component of the application as the original application
	 * and its cached quivalent are never going to have the same pathnames.
	 * Also, we use PATHNAME() and not NAME() - this catches things like vi
	 * that exec shells using execv(/usr/bin/ksh, sh ...).
	 */
	if (head->ch_app) {
		char *	_str, * _cname, * cname, * aname = PATHNAME(lmp);

		obj = (Rtc_obj *)(head->ch_app + addr);
		cname = _cname = (char *)(strtbl + obj->co_alter);

		if ((_str = strrchr(aname, '/')) != NULL)
			aname = ++_str;
		if ((_str = strrchr(cname, '/')) != NULL)
			cname = ++_str;

		if (strcmp(aname, cname)) {
			eprintf(ERR_WARNING, MSG_INTL(MSG_CONF_APP),
			    config->c_name, _cname);
			return (1);
		}
	}

	/*
	 * If alternative objects are specified traverse the directories
	 * specified in the configuration file, if any directory is newer than
	 * the time it was recorded in the cache then continue to inspect its
	 * files.  Any file determined newer than its configuration recording
	 * questions the the use of any alternative objects.  The intent here
	 * is to make sure no-one abuses a configuration as a means of static
	 * linking.
	 */
	for (dirtbl = (Rtc_dir *)(head->ch_dir + addr);
	    dirtbl->cd_obj; dirtbl++) {
		obj = (Rtc_obj *)(dirtbl->cd_obj + addr);

		if (obj->co_flags & RTC_OBJ_NOEXIST)
			continue;

		str = strtbl + obj->co_name;

		if (stat(str, &status) != 0) {
			err = errno;
			eprintf(ERR_WARNING, MSG_INTL(MSG_CONF_STAT),
			    config->c_name, str, strerror(err));
			continue;
		}

		if (status.st_mtime == obj->co_info)
			continue;

		/*
		 * The system directory is newer than the configuration files
		 * entry, start checking and dumped files.
		 */
		for (filetbl = (Rtc_file *)(dirtbl->cd_file + addr);
		    filetbl->cf_obj; filetbl++) {
			obj = (Rtc_obj *)(filetbl->cf_obj + addr);
			str = strtbl + obj->co_name;

			if ((obj->co_flags & RTC_OBJ_DUMP) == 0)
				continue;

			if (stat(str, &status) != 0) {
				err = errno;
				eprintf(ERR_WARNING, MSG_INTL(MSG_CONF_STAT),
				    config->c_name, str, strerror(err));
				continue;
			}

			if (status.st_size != obj->co_info) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_CONF_FCMP),
				    config->c_name, str);
			}
		}
	}
	return (0);
}

int
elf_config(Rt_map * lmp)
{
	Rtc_head *	head;
	int		fd, features = 0;
	struct stat	status;
	Addr		addr;
	Pnode *		pnp;
	const char *	str = config->c_name;

	/*
	 * If an alternative configuration file has been specified use it
	 * (expanding any tokens), otherwise try opening up the default.
	 */
	if ((str == 0) && ((rtld_flags & RT_FL_CONFAPP) == 0))
#ifdef _ELF64
		str = MSG_ORIG(MSG_PTH_CONFIG_64);
#else
		str = MSG_ORIG(MSG_PTH_CONFIG);
#endif
	else if (rtld_flags & RT_FL_SECURE)
		return (0);
	else {
		size_t	size;
		char *	name;

		/*
		 * If we're dealing with an alternative application fabricate
		 * the need for a $ORIGIN/ld.config.app-name configuration file.
		 */
		if (rtld_flags & RT_FL_CONFAPP) {
			char	_name[PATH_MAX];

			if ((str = strrchr(PATHNAME(lmp), '/')) != NULL)
				str++;
			else
				str = PATHNAME(lmp);

			(void) sprintf(_name, MSG_ORIG(MSG_ORG_CONFIG), str);
			str = _name;
		}

		size = strlen(str);
		name = (char *)str;

		(void) expand(&name, &size, 0, lmp);
		str = (const char *)name;
	}
	config->c_name = str;

	/*
	 * If we can't open the configuration file return silently.
	 */
	if ((fd = open(str, O_RDONLY, 0)) == -1)
		return (DBG_CONF_PRCFAIL);

	/*
	 * Determine the configuration file size and map the file.
	 */
	(void) fstat(fd, &status);
	if (status.st_size < sizeof (Rtc_head)) {
		(void) close(fd);
		return (DBG_CONF_CORRUPT);
	}
	if ((addr = (Addr)mmap(0, status.st_size, PROT_READ, MAP_SHARED,
	    fd, 0)) == (Addr)MAP_FAILED) {
		(void) close(fd);
		return (DBG_CONF_PRCFAIL);
	}

	config->c_bgn = addr;
	config->c_end = addr + status.st_size;
	(void) close(fd);

	head = (Rtc_head *)addr;

	/*
	 * Make sure we can handle this version of the configuration file.
	 */
	if (head->ch_version > RTC_VER_CURRENT)
		return (DBG_CONF_VERSION);

	/*
	 * When crle(1) creates a temporary configuration file the
	 * RTC_HDR_IGNORE flag is set.  Thus the mapping of the configuration
	 * file is taken into account but not its content.
	 */
	if (head->ch_cnflags & RTC_HDR_IGNORE)
		return (DBG_CONF_IGNORE);

	/*
	 * Apply any new default library pathname.
	 */
	if (head->ch_edlibpath) {
		str = (const char *)(head->ch_edlibpath + addr);
		if ((pnp = make_pnode_list(str,
		    (LA_SER_DEFAULT | LA_SER_CONFIG), 0, 0, lmp)) != 0)
			elf_fct.fct_dflt_dirs = pnp;
		features |= DBG_CONF_EDLIBPATH;
	}
	if (head->ch_eslibpath) {
		str = (const char *)(head->ch_eslibpath + addr);
		if ((pnp = make_pnode_list(str,
		    (LA_SER_SECURE | LA_SER_CONFIG), 0, 0, lmp)) != 0)
			elf_fct.fct_secure_dirs = pnp;
		features |= DBG_CONF_ESLIBPATH;
	}
#if	defined(__sparc) && !defined(_LP64)
	if (head->ch_adlibpath) {
		str = (const char *)(head->ch_adlibpath + addr);
		if ((pnp = make_pnode_list(str,
		    (LA_SER_DEFAULT | LA_SER_CONFIG), 0, 0, lmp)) != 0)
			aout_fct.fct_dflt_dirs = pnp;
		features |= DBG_CONF_ADLIBPATH;
	}
	if (head->ch_aslibpath) {
		str = (const char *)(head->ch_aslibpath + addr);
		if ((pnp = make_pnode_list(str,
		    (LA_SER_SECURE | LA_SER_CONFIG), 0, 0, lmp)) != 0)
			aout_fct.fct_secure_dirs = pnp;
		features |= DBG_CONF_ASLIBPATH;
	}
#endif

	/*
	 * Determine whether directory configuration is available.
	 */
	if ((!(rtld_flags & RT_FL_NODIRCFG)) && head->ch_hash) {
		config->c_hashtbl = (Word *)(head->ch_hash + addr);
		config->c_hashchain = &config->c_hashtbl[2 +
		    config->c_hashtbl[0]];
		config->c_objtbl = (Rtc_obj *)(head->ch_obj + addr);
		config->c_strtbl = (const char *)(head->ch_str + addr);

		rtld_flags |= RT_FL_DIRCFG;
		features |= DBG_CONF_DIRCFG;
	}

	/*
	 * Determine whether alternative objects are specified or an object
	 * reservation area is required.  If the reservation can't be completed
	 * (either because the configuration information is out-of-date, or the
	 * the reservation can't be allocated), then alternative objects are
	 * ignored.
	 */
	if ((!(rtld_flags & (RT_FL_NODIRCFG | RT_FL_NOOBJALT))) &&
	    (head->ch_cnflags & RTC_HDR_ALTER)) {
		rtld_flags |= RT_FL_OBJALT;
		features |= DBG_CONF_OBJALT;

		if (head->ch_resbgn) {

			if (elf_config_validate(addr, head, lmp) != 0)
				return (-1);

			if (((config->c_bgn <= head->ch_resbgn) &&
			    (config->c_bgn >= head->ch_resend)) ||
			    (nu_map((caddr_t)head->ch_resbgn,
			    (head->ch_resend - head->ch_resbgn), PROT_NONE,
			    MAP_FIXED | MAP_PRIVATE) == MAP_FAILED))
				return (-1);

			rtld_flags |= RT_FL_MEMRESV;
			features |= DBG_CONF_MEMRESV;
		}
	}

	return (features);
}

/*
 * Determine whether the given file exists in the configuration file.
 */
Rtc_obj *
elf_config_ent(const char * name, Word hash, int id, const char ** alternate)
{
	Word		bkt, ndx;
	const char *	str;
	Rtc_obj *	obj;

	bkt = hash % config->c_hashtbl[0];
	ndx = config->c_hashtbl[2 + bkt];

	while (ndx) {
		obj = config->c_objtbl + ndx;
		str = config->c_strtbl + obj->co_name;

		if ((obj->co_hash != hash) || (strcmp(name, str) != 0) ||
		    (id && (id != obj->co_id))) {
			ndx = config->c_hashchain[ndx];
			continue;
		}

		if ((obj->co_flags & RTC_OBJ_ALTER) && alternate)
			*alternate = config->c_strtbl + obj->co_alter;

		return (obj);
	}
	return (0);
}
