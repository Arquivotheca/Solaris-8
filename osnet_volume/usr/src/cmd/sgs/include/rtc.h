/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_RTC_H
#define	_RTC_H

#pragma ident	"@(#)rtc.h	1.2	99/11/03 SMI"

/*
 * Global include file for the runtime configuration support.
 */
#include <time.h>
#include "machdep.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Configuration header.
 */
typedef struct {
	Word	ch_version;		/* version of config file */
	Word	ch_cnflags;		/* configuration flags */
	Word	ch_dlflags;		/* dldump() flags used */
	Word	ch_app;			/* application that this config file */
					/*	is specific to */
	Word	ch_hash;		/* hash table offset */
	Word	ch_obj;			/* object table offset */
	Word	ch_str;			/* string table offset */
	Word	ch_file;		/* file entries */
	Word	ch_dir;			/* directory entries */
	Word	ch_edlibpath;		/* ELF default library path offset */
	Word	ch_adlibpath;		/* AOUT default library path offset */
	Word	ch_eslibpath;		/* ELF secure library path offset */
	Word	ch_aslibpath;		/* AOUT secure library path offset */
	Lword	ch_resbgn;		/* Memory reservation required to map */
	Lword	ch_resend;		/*	alternative objects defined */
					/*	by the configuration info */
} Rtc_head;

#define	RTC_HDR_IGNORE	0x0001		/* ignore config information */
#define	RTC_HDR_ALTER	0x0002		/* alternative objects are defined - */
					/*	it may exist without a memory */
					/*	reservation (see -a) */
/*
 * Object descriptor.
 */
typedef struct {
	Lword	co_info;		/* validation information */
	Word	co_name;		/* object name (directory or file) */
	Word	co_hash;		/* name hash value */
	Half	co_id;			/* directory identifier */
	Half	co_flags;		/* various flags */
	Word	co_alter;		/* alternative object file */
} Rtc_obj;

#define	RTC_OBJ_DIRENT	0x0001		/* object defines a directory */
#define	RTC_OBJ_ALLENTS	0x0002		/* directory was scanned for all */
					/*	containing objects */
#define	RTC_OBJ_NOEXIST	0x0004		/* object does not exist */
#define	RTC_OBJ_EXEC	0x0008		/* object identifies executable */
#define	RTC_OBJ_ALTER	0x0010		/* object has an alternate */
#define	RTC_OBJ_DUMP	0x0020		/* alternate created by dldump(3x) */
#define	RTC_OBJ_REALPTH	0x0040		/* object identifies real path */
#define	RTC_OBJ_NOALTER	0x0080		/* object can't have an alternate */

/*
 * Directory and file descriptors.  The configuration cache (cd_dir) points to
 * an array of directory descriptors, this in turn point to their associated
 * arrays of file descriptors.  Both of these provide sequential access for
 * configuration file validation (directory, and possible file stat()'s).
 */
typedef struct {
	Word	cd_obj;			/* index to Rtc_obj */
	Word	cd_file;		/* index to Rtc_file[] */
} Rtc_dir;

typedef	struct {
	Word	cf_obj;			/* index to Rtc_obj */
} Rtc_file;


#define	RTC_VER_NONE	0
#define	RTC_VER_CURRENT 1
#define	RTC_VER_NUM	2

#ifdef	__cplusplus
}
#endif

#endif	/* _RTC_H */
