/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_I386_SYS_RAMFILE_H
#define	_I386_SYS_RAMFILE_H

#pragma ident	"@(#)ramfile.h	1.9	97/11/25 SMI"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	RAMblk_ERROR	-2
#define	RAMblk_OK	0

#define	RAMfile_EOF	-2
#define	RAMfile_ERROR	-1
#define	RAMfile_OK	0

#define	RAMfile_BLKSIZE	1024

#define	RAMrewind(rfd) \
	(void) RAMfile_lseek((rfd), 0, SEEK_SET)

typedef struct rblk {
	struct rblk *next;
	struct rblk *prev;
	char	*datap;
} rblk_t;

#define	RAMfp_modified	0x1	/* Indicate if RAMfile actually modified */
#define	RAMfp_nosync	0x2	/*
				 * Indicate a RAMfile should never be saved
				 * with a delayed write
				 */

typedef struct rfil {
	struct rfil *next;
	rblk_t	*contents;
	char	*name;
	ulong	attrib;
	ulong	size;
	ulong	flags;
} rfil_t;

typedef struct orf {
	int	uid;	/* Unique identifier */
	int	isdir;	/*
			 * Flags the descriptor as describing a directory
			 * within the RAMfs.  Only stat() calls are valid
			 * on such descriptors.
			 */
	rfil_t	*file;
	ulong	foff;	/* Absolute offset of file pointer */
	char	*fptr;	/* Pointer to offset's storage in actual RAMblk */
	rblk_t	*cblkp;	/* Pointer to RAMblk struct in which offset resides */
	/*
	 * cblkn -- Indicates which RAMfile_BLKSIZE sized
	 * 	chunk the current offset resides in.
	 * 	Chunks numbered from zero.
	 */
	ulong	cblkn;
	struct orf *next;  /* pointer to the next descriptor */
} rffd_t;

/*
 *  RAMfiles don't live in a real file system.  So when we want
 *  to provide dirents that include RAMfiles we have to first get all
 *  the dirents (which are stored in these dentbuf_list structures and
 *  then patch over the top.
 */
#define	RAMDENTBUFSZ 1024

typedef struct dbll {
	struct dbll *next;
	int numdents;
	int curdent;
	char dentbuf[RAMDENTBUFSZ];
} dentbuf_list;

/*
 *  RAMblk prototypes
 */
rblk_t	*RAMblk_alloc(void);
int	RAMblklst_free(rblk_t *);

/*
 *  RAMfile prototypes
 */
extern	void	RAMfile_addtolist(rfil_t *);
extern	void	RAMfile_rmfromlist(rfil_t *);
extern	rfil_t	*RAMfile_alloc(char *, ulong);
extern	int	RAMfile_free(rfil_t *);
extern	int	RAMfile_allocfd(rfil_t *);
extern	int	RAMfile_freefd(rffd_t *);

extern	int	RAMfile_open(char *, ulong);
extern	int	RAMfile_close(int);
extern	void	RAMfile_closeall(int);
extern	int	RAMfile_diropen(char *);
extern	char	*RAMfile_striproot(char *);
extern	int	RAMfile_trunc_atoff(int);
extern	void	RAMfile_trunc(rfil_t *, ulong);
extern	int	RAMfile_create(char *, ulong);
extern	int	RAMfile_destroy(char *);
extern	off_t	RAMfile_lseek(int, off_t, int);
extern	int	RAMfile_fstat(int, struct stat *);
extern	int	RAMfile_write(int, char *, int);
extern	ssize_t	RAMfile_read(int, char *, size_t);
extern  int	RAMfile_rename(int, char *);
extern	void	RAMfile_clear_modbit(int);
extern	void	RAMfile_set_cachebit(int);
extern	int	RAMcvtfile(char *, ulong);
extern	void	RAMfiletoprop(rffd_t *);
extern	void	RAMfile_patch_dirents(char *path, dentbuf_list *dentslist);
extern	void	add_dentent(dentbuf_list *, char *, int);

#ifdef	__cplusplus
}
#endif

#endif /* _I386_SYS_RAMFILE_H */
