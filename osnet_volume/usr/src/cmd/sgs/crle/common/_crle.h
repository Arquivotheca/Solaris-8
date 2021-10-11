/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__CRLE_H
#define	__CRLE_H

#pragma ident	"@(#)_crle.h	1.1	99/08/13 SMI"

#include <sys/types.h>
#include <gelf.h>
#include "sgs.h"
#include "machdep.h"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Hash table support routines.
 */
typedef	struct hash_obj	Hash_obj;
typedef struct hash_ent Hash_ent;
typedef	struct hash_tbl	Hash_tbl;

typedef enum {
	HASH_STR,
	HASH_INT
} Hash_type;

struct hash_obj {
	Half		o_id;
	Half		o_flags;
	Word		o_cnt;
	Hash_tbl *	o_tbl;
	char *		o_alter;
	char *		o_rpath;
	Hash_obj *	o_dir;
	Lword		o_info;
};

struct hash_ent {
	Hash_ent *	e_next;
	Word		e_hash;
	Addr		e_key;
	Hash_obj *	e_obj;
};

struct hash_tbl {
	ulong_t		t_ident;
	int 		t_size;
	Hash_type	t_type;
	Hash_ent **	t_entry;
};

#define	HASH_FND_ENT	0x01		/* search for existing hash entry */
#define	HASH_ADD_ENT	0x02		/* add hash entry */

/*
 * Global data for final configuration files construction.
 */
typedef	struct crle_desc {
	char *		c_name;		/* calling program */
	char *		c_tempname;	/* temporary file, file descriptor */
	int		c_tempfd;	/*	mmapped address and size */
	Addr		c_tempaddr;
	size_t		c_tempsize;
	char *		c_confil;	/* configuration file */
	char *		c_objdir;	/* object directory (dldump(3x)) */
	char *		c_audit;	/* audit library name */
	unsigned int	c_flags;	/* state flags for crle processing */
	unsigned short	c_machine;	/* object machine type and class to */
	unsigned short	c_class;	/* 	operate on */
	int		c_dlflags;	/* dldump(3x) flags */
	int		c_strbkts;	/* internal hash table initialization */
	int		c_inobkts;	/*	parameters */
	unsigned int	c_noexistnum;	/* no. of non-existent entries */
	unsigned int	c_dirnum;	/* no. of directories processed */
	unsigned int	c_filenum;	/* no. of files processed */
	unsigned int	c_hashstrnum;	/* no. of hashed strings to create */
	Hash_tbl *	c_strtbl;	/* string table and size */
	size_t		c_strsize;
	List		c_inotbls;	/* list of inode tables */
	const char *	c_app;		/* alternative application */
	char *		c_edlibpath;	/* ELF default library path */
	char *		c_adlibpath;	/* AOUT default library path */
	char *		c_eslibpath;	/* ELF secure library path */
	char *		c_aslibpath;	/* AOUT secure library path */
} Crle_desc;

#define	CRLE_CREAT	0x0001		/* config file creation required */
#define	CRLE_ALTER	0x0002		/* alternative entry required */
#define	CRLE_DUMP	0x0004		/* alternative create by dldump(3x) */
#define	CRLE_GROUP	0x0008		/* determine objects dependencies */
#define	CRLE_VERBOSE	0x0010		/* verbose mode */
#define	CRLE_AOUT	0x0020		/* AOUT flag in effect */
#define	CRLE_EXISTS	0x0040		/* config file already exists */
#define	CRLE_DIFFDEV	0x0080		/* config file and temporary exist on */
					/*	different filesystems */

/*
 * Local functions.
 */
extern int		addlib(Crle_desc *, char **, const char *);
extern int		depend(Crle_desc *, const char *, GElf_Ehdr *, int);
extern int		dlflags(Crle_desc *, const char *);
extern int		dump(Crle_desc *);
extern int		genconfig(Crle_desc *);
extern Hash_ent *	get_hash(Hash_tbl *, Addr, int);
extern int		inspect(Crle_desc *, const char *, int);
extern Listnode *	list_append(List *, const void *);
extern Hash_tbl *	make_hash(int, Hash_type, ulong_t);
extern int		printconfig(Crle_desc *);
extern int		updateconfig(Crle_desc *);

#ifdef	__cplusplus
}
#endif

#endif	/* __CRLE_H */
