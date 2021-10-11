/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_IO_IMPL_H
#define	_MDB_IO_IMPL_H

#pragma ident	"@(#)mdb_io_impl.h	1.1	99/08/11 SMI"

#include <mdb/mdb_io.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

typedef struct mdb_io_ops {
	ssize_t (*io_read)(mdb_io_t *, void *, size_t);
	ssize_t (*io_write)(mdb_io_t *, const void *, size_t);
	off64_t (*io_seek)(mdb_io_t *, off64_t, int);
	int (*io_ctl)(mdb_io_t *, int, void *);
	void (*io_close)(mdb_io_t *);
	const char *(*io_name)(mdb_io_t *);
	void (*io_link)(mdb_io_t *, mdb_iob_t *);
	void (*io_unlink)(mdb_io_t *, mdb_iob_t *);
	ssize_t (*io_attrstr)(mdb_io_t *, int, uint_t, char *);
} mdb_io_ops_t;

#define	IOP_READ(io, buf, len) ((io)->io_ops->io_read((io), (buf), (len)))
#define	IOP_WRITE(io, buf, len) ((io)->io_ops->io_write((io), (buf), (len)))
#define	IOP_SEEK(io, off, whence) ((io)->io_ops->io_seek((io), (off), (whence)))
#define	IOP_CTL(io, req, arg) ((io)->io_ops->io_ctl((io), (req), (arg)))
#define	IOP_CLOSE(io) ((io)->io_ops->io_close((io)))
#define	IOP_NAME(io) ((io)->io_ops->io_name((io)))
#define	IOP_LINK(io, iob) ((io)->io_ops->io_link((io), (iob)))
#define	IOP_UNLINK(io, iob) ((io)->io_ops->io_unlink((io), (iob)))
#define	IOP_ATTRSTR(io, r, a, b) ((io)->io_ops->io_attrstr((io), (r), (a), (b)))

#define	IOPF_READ(io)	\
	((ssize_t (*)(mdb_io_t *, void *, size_t))(io)->io_ops->io_read)

#define	IOPF_WRITE(io)	\
	((ssize_t (*)(mdb_io_t *, void *, size_t))(io)->io_ops->io_write)

#define	ATT_STANDOUT	0x01		/* Standout mode */
#define	ATT_UNDERLINE	0x02		/* Underline mode */
#define	ATT_REVERSE	0x04		/* Reverse video mode */
#define	ATT_BOLD	0x08		/* Bold text mode */
#define	ATT_DIM		0x10		/* Dim text mode */
#define	ATT_ALTCHARSET	0x20		/* Alternate character set mode */

#define	ATT_ALL		0x3f		/* Mask of all valid attributes */

#define	ATT_OFF		0		/* Turn attributes off */
#define	ATT_ON		1		/* Turn attributes on */

struct mdb_io {
	const mdb_io_ops_t *io_ops;	/* I/O type-specific operations */
	void *io_data;			/* I/O type-specific data pointer */
	mdb_io_t *io_next;		/* Link to next i/o object on stack */
	size_t io_refcnt;		/* Reference count */
};

struct mdb_iob {
	char *iob_buf;			/* Input/output buffer */
	size_t iob_bufsiz;		/* Size of iob_buf in bytes */
	char *iob_bufp;			/* Current buffer location */
	size_t iob_nbytes;		/* Number of bytes in io_buf */
	size_t iob_nlines;		/* Lines output on current page */
	size_t iob_lineno;		/* Storage for saved yylineno */
	size_t iob_rows;		/* Terminal height */
	size_t iob_cols;		/* Terminal width */
	size_t iob_tabstop;		/* Tab stop width */
	size_t iob_margin;		/* Margin width */
	uint_t iob_flags;		/* Flags (see <mdb/mdb_io.h>) */
	mdb_io_t *iob_iop;		/* I/o implementation pointer */
	mdb_io_t *iob_pgp;		/* Pager i/o implementation pointer */
	mdb_iob_t *iob_next;		/* Stack next pointer */
};

/*
 * Stub functions for i/o backend implementors: these stubs either act as
 * pass-through no-ops or return ENOTSUP as appropriate.
 */
extern ssize_t no_io_read(mdb_io_t *, void *, size_t);
extern ssize_t no_io_write(mdb_io_t *, const void *, size_t);
extern off64_t no_io_seek(mdb_io_t *, off64_t, int);
extern int no_io_ctl(mdb_io_t *, int, void *);
extern void no_io_close(mdb_io_t *);
extern const char *no_io_name(mdb_io_t *);
extern void no_io_link(mdb_io_t *, mdb_iob_t *);
extern void no_io_unlink(mdb_io_t *, mdb_iob_t *);
extern ssize_t no_io_attrstr(mdb_io_t *, int, uint_t, char *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_IO_IMPL_H */
