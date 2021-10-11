/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)ramfile.c	1.23	97/11/25 SMI"

#include <sys/bootconf.h>
#include <sys/filemap.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>

/*
 *  RAMfile globals
 */
static rfil_t *RAMfileslist;			/* The entire file system */
static rffd_t *RAMfdslist;			/* Active file descriptors */
static int    RAMfs_filedes;			/* Unique identifier counter */

/*
 *  External functions and structures
 */
extern struct bootops *bop;
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int strncasecmp(char *, char *, int);

/*
 * Debug printf toggle
 */
int	RAMfile_debug = 0;

/*
 * For situations where we must return an error indication,
 * the File_doserr contains an appropriate DOS error value.
 */
ushort	File_doserr;

/*
 * For situations where we must return both an error indication
 * and a number of bytes, the number of bytes are stored in the
 * RAMfile_bytecount global.
 */
int	RAMfile_bytecount;

rblk_t
*RAMblk_alloc(void)
{
	rblk_t *rb;

	if (RAMfile_debug)
		printf("[]alloc. ");

	if ((rb = (rblk_t *)bkmem_alloc(sizeof (rblk_t))) == (rblk_t *)NULL)
		goto error;
	if ((rb->datap = (char *)
	    bkmem_alloc(RAMfile_BLKSIZE)) == (char *)NULL) {
		bkmem_free((caddr_t)rb, sizeof (rblk_t));
		rb = (rblk_t *)NULL;
		goto error;
	}

	/* Zero out block contents */
	bzero(rb->datap, RAMfile_BLKSIZE);
	rb->next = (rblk_t *)NULL;
	rb->prev = (rblk_t *)NULL;
	goto done;

error:
	printf("Insufficient memory for RAMblk allocation!\n");
done:
	return (rb);
}

int
RAMblklst_free(rblk_t *rl)
{
	rblk_t	*cb;
	int	rv = RAMblk_OK;

	if (RAMfile_debug)
		printf("[]lst_free ");

	if (!rl)
		return (rv);	/* that was easy */

	/*
	 *  Separate from the rest of the list if this is only a
	 *  part of the list.
	 */
	if (rl->prev) {
		rl->prev->next = (rblk_t *)NULL;
		rl->prev = (rblk_t *)NULL;
	}

	for (cb = rl; cb; cb = cb->next) {
		if (cb->datap)
			bkmem_free(cb->datap, RAMfile_BLKSIZE);
	}

	/* Find second to last node in list */
	for (cb = rl; cb && cb->next; cb = cb->next);

	/* Free the last node if there is one */
	if (cb->next)
		bkmem_free((caddr_t)cb->next, sizeof (rblk_t));

	/* Work backwards to front of list */
	while (cb->prev) {
		cb = cb->prev;
		bkmem_free((caddr_t)cb->next, sizeof (rblk_t));
	}
	/* Free remaining node */
	bkmem_free((caddr_t)cb, sizeof (rblk_t));
	return (rv);
}

void
RAMfile_addtolist(rfil_t *nf)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_addtolist ");

	for (sl = RAMfileslist; sl && sl->next; sl = sl->next);

	if (sl)
		sl->next = nf;
	else
		RAMfileslist = nf;
}

void
RAMfile_rmfromlist(rfil_t *df)
{
	rfil_t *cf, *pf;

	if (RAMfile_debug)
		printf("@file_rmfromlist ");

	pf = (rfil_t *)NULL;
	for (cf = RAMfileslist; cf; pf = cf, cf = cf->next) {
		if (cf == df) {
			if (pf)
				pf->next = cf->next;
			else
				RAMfileslist = cf->next;
			break;
		}
	}
}

rfil_t *
RAMfile_alloc(char *fn, ulong attr)
{
	rfil_t *nf;

	if (RAMfile_debug)
		printf("@file_alloc ");

	if ((nf = (rfil_t *)bkmem_alloc(sizeof (rfil_t))) == (rfil_t *)NULL)
		goto done;
	if ((nf->contents = RAMblk_alloc()) == (rblk_t *)NULL) {
		bkmem_free((caddr_t)nf, sizeof (rfil_t));
		nf = (rfil_t *)NULL;
		goto done;
	}
	if ((nf->name = (char *)bkmem_alloc(strlen(fn)+1)) == (char *)NULL) {
		(void) RAMblklst_free(nf->contents);
		nf->contents = (rblk_t *)NULL;
		bkmem_free((caddr_t)nf, sizeof (rfil_t));
		nf = (rfil_t *)NULL;
		goto done;
	}
	(void) strcpy(nf->name, fn);
	nf->next = (rfil_t *)NULL;
	nf->attrib = attr;
	nf->size = 0;
	nf->flags = 0;

	RAMfile_addtolist(nf);

done:
	return (nf);
}

int
RAMfile_free(rfil_t *df)
{
	int rv;

	if (RAMfile_debug)
		printf("@file_free ");

	RAMfile_rmfromlist(df);

	if (RAMblklst_free(df->contents) == RAMblk_ERROR)
		goto error;
	bkmem_free(df->name, strlen(df->name)+1);
	bkmem_free((caddr_t)df, sizeof (rfil_t));

	rv = RAMfile_OK;
	goto done;
error:
	printf("RAMFILE resource free failure!?\n");
	rv = RAMfile_ERROR;

done:
	return (rv);
}

rffd_t *
RAMfile_findfd(int fd)
{
	rffd_t *lp;

	if (fd <= 0 || fd > RAMfs_filedes) {
		File_doserr = DOSERR_INVALIDHANDLE;
		return ((rffd_t *)NULL);
	}

	for (lp = RAMfdslist; lp; lp = lp->next) {
		if (lp->uid == fd)
			break;
	}

	if (!lp) {
		File_doserr = DOSERR_INVALIDHANDLE;
	}

	return (lp);
}

int
RAMfile_allocfd(rfil_t *nf)
{
	rffd_t *rfd;

	if (RAMfile_debug)
		printf("@file_allocfd ");

	if ((rfd = (rffd_t *)bkmem_alloc(sizeof (rffd_t))) == (rffd_t *)NULL) {
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		return (-1);
	} else {
		rfd->uid = ++RAMfs_filedes;
		rfd->isdir = 0;
		rfd->file = nf;
		rfd->fptr = nf->contents->datap;
		rfd->cblkp = nf->contents;
		rfd->cblkn = 0;
		rfd->foff = 0;

		if (RAMfdslist) {
			rfd->next = RAMfdslist;
		} else {
			rfd->next = (rffd_t *)NULL;
		}
		RAMfdslist = rfd;
	}

	if (RAMfile_debug)
		printf("=<%d>", RAMfs_filedes);

	return (RAMfs_filedes);
}

int
RAMfile_freefd(rffd_t *handle)
{
	rffd_t *lp, *pp;

	if (RAMfile_debug)
		printf("@file_freefd:%x:", handle);

	if (!handle) {
		if (RAMfile_debug)
			printf("NOTRAM\n");
		File_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	if (handle == RAMfdslist) {
		RAMfdslist = handle->next;
	} else {
		pp = RAMfdslist;
		for (lp = RAMfdslist->next; lp; ) {
			if (handle == lp) {
				pp->next = handle->next;
				break;
			} else {
				pp = lp;
				lp = lp->next;
			}
		}
		bkmem_free((caddr_t)handle, sizeof (rffd_t));
	}

	return (RAMfile_OK);
}

/*ARGSUSED1*/
int
RAMfile_open(char *fn, ulong mode)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_open:%s:", fn);

	/* Eliminate any leading U: or R: volume name */
	if ((strncasecmp(fn, "R:", 2) == 0) || (strncasecmp(fn, "U:", 2) == 0))
		fn += 2;

	/*
	 * All RAMfiles are chroot'ed and the root path isn't stored as
	 * part of the filename.
	 */
	fn = RAMfile_striproot(fn);

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@match, %s:", sl->name);
			break;
		}
	}

	if (!sl) {
		if (RAMfile_debug)
			printf("FNF.\n");
		File_doserr = DOSERR_FILENOTFOUND;
		return (RAMfile_ERROR);
	}
	return (RAMfile_allocfd(sl));
}

int
RAMfile_close(int fd)
{
	rffd_t *handle;

	if (RAMfile_debug)
		printf("@file_close:%d:", fd);

	/* XXX: note that this failure occurs frequently */
	if ((handle = RAMfile_findfd(fd)) == (rffd_t *)NULL) {
		File_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	/* Set up to perform a delayed write, but only if not a special file */
	if (handle->isdir == 0 &&
	    (strcmp(handle->file->name, DOSBOOTOPC_FN) != 0) &&
	    (strcmp(handle->file->name, DOSBOOTOPR_FN) != 0) &&
	    ((handle->file->flags & RAMfp_nosync) == 0) &&
	    (handle->file->flags & RAMfp_modified)) {
		RAMfiletoprop(handle);
	}

	return (RAMfile_freefd(handle));
}

/*ARGSUSED*/
void
RAMfile_closeall(int flag)
{
	rffd_t *rfd;

	if (RAMfile_debug)
		printf("@file_closeall ");

	for (rfd = RAMfdslist; rfd; rfd = rfd->next) {
		RAMfile_close(rfd->uid);
	}
}

int
RAMfile_trunc_atoff(int fd)
{
	rffd_t *handle;
	rfil_t *rf;

	if (RAMfile_debug)
		printf("@file_trunc_atoff <%x>", fd);

	if (((handle = RAMfile_findfd(fd)) == (rffd_t *)NULL) ||
	    handle->isdir == 1) {
		File_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	rf = handle->file;

	if (handle->foff == 0)
		RAMfile_trunc(rf, rf->attrib);
	else {
		rf->size = handle->foff;
		rf->flags |= RAMfp_modified;
	}

	return (RAMfile_OK);
}

void
RAMfile_trunc(rfil_t *fp, ulong attr)
{
	if (RAMfile_debug)
		printf("@file_trunc [%s]", fp->name);

	fp->attrib = attr;
	fp->size = 0;
	/* Free all but the first contents block */
	if (RAMblklst_free(fp->contents->next) == RAMblk_ERROR)
		printf("WARNING: RAMblklst_free failed during truncate.\n");
	fp->contents->next = (rblk_t *)NULL;
	/* Zero out contents */
	bzero(fp->contents->datap, RAMfile_BLKSIZE);
	fp->flags |= RAMfp_modified;
}

int
RAMfile_create(char *fn, ulong attr)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_create:%s,%x:", fn, attr);

	/* Eliminate any leading U: or R: volume name */
	if ((strncasecmp(fn, "R:", 2) == 0) || (strncasecmp(fn, "U:", 2) == 0))
		fn += 2;

	/*
	 * All RAMfiles are chroot'ed and the root path isn't stored as
	 * part of the filename.
	 */
	fn = RAMfile_striproot(fn);

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@crematch, %s:", sl->name);
			break;
		}
	}

	if (sl) {
		RAMfile_trunc(sl, attr);
	} else {
		if ((sl = RAMfile_alloc(fn, attr)) == (rfil_t *)NULL) {
			printf("ERROR: No memory for RAM file\n");
			File_doserr = DOSERR_INSUFFICIENT_MEMORY;
			return (RAMfile_ERROR);
		}
	}
	return (RAMfile_allocfd(sl));
}

int
RAMfile_destroy(char *fn)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_destroy<%s>", fn);

	/* Eliminate any leading U: or R: volume name */
	if ((strncasecmp(fn, "R:", 2) == 0) || (strncasecmp(fn, "U:", 2) == 0))
		fn += 2;

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@desmatch, %s:", sl->name);
			break;
		}
	}

	if (sl) {
		return (RAMfile_free(sl));
	} else {
		File_doserr = DOSERR_FILENOTFOUND;
		return (RAMfile_ERROR);
	}
}

off_t
RAMfile_lseek(int fd, off_t offset, int whence)
{
	ulong bn, reqblk, reqoff;
	rffd_t *handle;
	rblk_t *fb, *pfb;
	rfil_t *fp;
	off_t newoff;
	int isrdonly = 0;

	if (RAMfile_debug)
		printf("@file_lseek <%x->%d>", fd, offset);

	if (((handle = RAMfile_findfd(fd)) == (rffd_t *)NULL) ||
	    handle->isdir == 1) {
		if (RAMfile_debug)
			printf("@file_lseek <bad fd>");
		File_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	fp = handle->file;

	isrdonly = fp->attrib & DOSATTR_RDONLY;

	switch (whence) {
	case SEEK_SET:
		newoff = offset;
		break;
	case SEEK_CUR:
		newoff = handle->foff + offset;
		break;
	case SEEK_END:
		newoff = fp->size + offset;
		break;
	default:
		goto seekerr;
	}

	/* Sanity checking the new offset */
	if ((isrdonly && newoff > fp->size) || newoff < 0)
		goto seekerr;

	/* Compute new block and the offset-within-block of the new offset */
	reqblk = newoff/RAMfile_BLKSIZE;
	reqoff = newoff%RAMfile_BLKSIZE;

	pfb = (rblk_t *)NULL;
	fb = fp->contents;
	bn = reqblk;
	while (reqblk--) {
		if (fb) {
			pfb = fb;
			fb = fb->next;
		} else {
			if (!(fb = RAMblk_alloc()))
				goto memerr;
			pfb->next = fb;
			fb->prev = pfb;
		}
	}

	handle->fptr = fb->datap + reqoff;
	handle->cblkp = fb;
	handle->cblkn = bn;
	handle->foff = newoff;
done:
	if (RAMfile_debug)
		printf("@file_lseek <went to %x>", newoff);
	return (newoff);
seekerr:
	File_doserr = DOSERR_SEEKERROR;
	if (RAMfile_debug)
		printf("@file_lseek <bad seek>");
	return (RAMfile_ERROR);
memerr:
	if (RAMfile_debug)
		printf("@file_lseek <bad seek, nomem>");
	File_doserr = DOSERR_INSUFFICIENT_MEMORY;
	return (RAMfile_ERROR);
}

int
RAMfile_fstat(int fd, struct stat *stbuf)
{
	/*
	 * Not at all a complete implementation, just enough to get us
	 * by.  THE ONLY FIELD ANYBODY CALLING THIS CARES ABOUT AT PRESS
	 * TIME IS THE SIZE, AND THAT IS ALL WE FILL IN.
	 */
	rffd_t *handle;
	rfil_t *rf;

	if (RAMfile_debug)
		printf("@file_fstat <%x>", fd);

	handle = RAMfile_findfd(fd);
	if (!stbuf || !handle)
		return (RAMfile_ERROR);

	/*
	 * Update from previous press release is that occasionally we
	 * might care about the fact that the descriptor points at a
	 * directory.
	 */
	if (handle->isdir) {
		stbuf->st_mode = S_IFDIR;
		stbuf->st_size = 0;
		return (RAMfile_OK);
	} else {
		stbuf->st_mode = S_IFREG;
	}

	rf = handle->file;
	stbuf->st_size = rf->size;
	return (RAMfile_OK);
}

ssize_t
RAMfile_read(int fd, char *buf, size_t buflen)
{
	rffd_t *handle;
	rfil_t *rf;
	int oc = 0;

	if (RAMfile_debug)
		printf("@file_read:%d:", fd);

	if (((handle = RAMfile_findfd(fd)) == (rffd_t *)NULL) ||
	    handle->isdir == 1) {
		File_doserr = DOSERR_INVALIDHANDLE;
		RAMfile_bytecount = oc;
		return (RAMfile_ERROR);
	} else if (buflen == 0) {
		return (RAMfile_bytecount = oc);
	}

	rf = handle->file;

	while ((handle->foff < rf->size) && (oc < buflen)) {
		if ((handle->foff/RAMfile_BLKSIZE) != handle->cblkn) {
			handle->cblkp = handle->cblkp->next;
			handle->fptr = handle->cblkp->datap;
			handle->cblkn++;
		}
		buf[oc++] = *(handle->fptr);
		handle->fptr++; handle->foff++;
	}
	return (RAMfile_bytecount = oc);
}

void
RAMfile_clear_modbit(int fd)
{
	rffd_t *handle;

	if (((handle = RAMfile_findfd(fd)) != (rffd_t *)NULL) &&
	    handle->isdir == 0) {
		handle->file->flags &= ~RAMfp_modified;
	}
}

void
RAMfile_set_cachebit(int fd)
{
	rffd_t *handle;

	if (((handle = RAMfile_findfd(fd)) != (rffd_t *)NULL) &&
	    handle->isdir == 0) {
		handle->file->flags |= RAMfp_nosync;
	}
}

int
RAMfile_write(int fd, char *buf, int buflen)
{
	rffd_t *handle;
	rfil_t *rf;
	ulong  begoff;
	int ic = 0;

	if (RAMfile_debug)
		printf("@file_write:%d:", fd);

	if (((handle = RAMfile_findfd(fd)) == (rffd_t *)NULL) ||
	    handle->isdir == 1) {
		File_doserr = DOSERR_INVALIDHANDLE;
		RAMfile_bytecount = ic;
		return (RAMfile_ERROR);
	} else if (buflen == 0) {
		return (RAMfile_bytecount = ic);
	}

	rf = handle->file;

	begoff = handle->foff;

	while (ic < buflen) {
		if ((handle->foff/RAMfile_BLKSIZE) != handle->cblkn) {
			if (!handle->cblkp->next) {
				handle->cblkp->next = RAMblk_alloc();
				if (!handle->cblkp->next) {
					File_doserr =
					    DOSERR_INSUFFICIENT_MEMORY;
					if ((RAMfile_bytecount = ic) > 0)
						rf->flags |= RAMfp_modified;
					return (RAMfile_ERROR);
				}
				handle->cblkp->next->prev = handle->cblkp;
			}
			handle->cblkp = handle->cblkp->next;
			handle->fptr = handle->cblkp->datap;
			handle->cblkn++;
		}
		*(handle->fptr) = buf[ic++];
		handle->fptr++; handle->foff++;
	}

	rf->flags |= RAMfp_modified;
	rf->size = MAX((begoff + ic), rf->size);

	if (RAMfile_debug)
		printf("@file_wrote %d bytes:", ic);

	return (RAMfile_bytecount = ic);
}

int
RAMfile_rename(int fd, char *nn)
{
	rffd_t *rf;
	char *an;
	int nl;

	rf = RAMfile_findfd(fd);
	if (!rf || rf->isdir == 1) {
		File_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	} else if (!nn)
		return (RAMfile_ERROR);

	nl = strlen(nn) + 1;
	if ((an = (char *)bkmem_alloc(nl)) == (char *)NULL)
		return (RAMfile_ERROR);
	(void) strcpy(an, nn);

	bkmem_free(rf->file->name, strlen(rf->file->name)+1);
	rf->file->name = an;
	return (RAMfile_OK);
}

/*
 * RAMfile_striproot
 *	RAMfile names leave off the 'boottree' part of the name to save
 *	a little space.  When dirent operations are happening we frequently
 *	have full path names to look up.  This routine provides a convenient
 *	interface for stripping off the 'boottree' part of a filename.
 *	(Presumably, because a lookup of it is desired using RAMfile_open, etc.)
 */
char *
RAMfile_striproot(char *name)
{
	char *prop;
	int plen, clen;

	if ((plen = bgetproplen(bop, "boottree", 0)) > 0) {
		if ((prop = bkmem_alloc(plen)) == NULL) {
			return (name);
		}

		/*
		 * Grab the property for comparison with the name passed
		 * in.  Leave out any ending NULL character in the comparison.
		 */
		if (bgetprop(bop, "boottree", prop, plen, 0) == plen) {
			clen = prop[plen-1] ? plen : plen - 1;
			if (strncmp(name, prop, clen) == 0) {
				bkmem_free(prop, plen);
				return (name+clen);
			}
		}

		bkmem_free(prop, plen);
		return (name);
	} else {
		return (name);
	}
}

int
RAMfile_allocdirfd(void)
{
	rffd_t *rfd;

	if (RAMfile_debug)
		printf("@file_allocdirfd ");

	if ((rfd = (rffd_t *)bkmem_alloc(sizeof (rffd_t))) == (rffd_t *)NULL) {
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		return (-1);
	} else {
		rfd->uid = ++RAMfs_filedes;
		rfd->isdir = 1;
		rfd->file = (rfil_t *)NULL;
		rfd->fptr = (char *)NULL;
		rfd->cblkp = (rblk_t *)NULL;
		rfd->cblkn = 0;
		rfd->foff = 0;

		if (RAMfdslist) {
			rfd->next = RAMfdslist;
		} else {
			rfd->next = (rffd_t *)NULL;
		}
		RAMfdslist = rfd;
	}

	if (RAMfile_debug)
		printf("=<%d>", RAMfs_filedes);

	return (RAMfs_filedes);
}

int
RAMfile_diropen(char *dirnam)
{
	/*
	 *  Determine if the required directory "exists"
	 *  in the RAMfiles we have.
	 */
	rfil_t *sl;
	char *rdn;

	/* Eliminate any leading U: or R: volume name */
	if ((strncasecmp(dirnam, "R:", 2) == 0) ||
	    (strncasecmp(dirnam, "U:", 2) == 0))
		dirnam += 2;

	/*
	 * All RAMfiles are chroot'ed and the root path isn't stored as
	 * part of the filename.
	 */
	rdn = RAMfile_striproot(dirnam);

	/*
	 * Eliminate leading slashes as well.
	 */
	while (*rdn == '/')
		rdn++;

	/*
	 * We must match the entire remainder of the path with a file in
	 * the RAMfile list.
	 */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strncmp(rdn, sl->name, strlen(rdn)) == 0) {
			char *pne;

			pne = sl->name + strlen(rdn);

			/*
			 * We match up to the end of the requested directory
			 * path.  Double check that our RAMfile path name is
			 * an EXACT match.  I.E., avoid the case where we
			 * think the RAMfile solaris/drivers/fish.bef is a
			 * match for a requested path solaris/driv.
			 */
			if (*pne != '/')
				continue;

			/*
			 * We have a match on the directory part.
			 * Build a special directory fd and return success.
			 */
			return (RAMfile_allocdirfd());
		}
	}

	/*
	 *  Handle virtual (map-created) directories as RAM directories.
	 */
	if (cpfs_isamapdir(dirnam))
		return (RAMfile_allocdirfd());

	/*
	 * The requested name is not the name of a directory in the RAMfs.
	 */
	return (-1);
}

/*
 * RAMfile_patch_dirents
 *	Based on the list of current RAMfiles, add any dirents necessary to
 *	the dirent info stored in the dentbuf_list linked list we've been
 *	handed.
 */
#define	NAMEBUFSIZE 56

void
RAMfile_patch_dirents(char *path, dentbuf_list *dentslist)
{
	/*
	 *  The path we've been passed is the current path we're looking
	 *  up dirents for.  We need to test to see if anything in that path
	 *  exists as a RAMfile.
	 *
	 *  The first step is determine if the required directory "exists"
	 *  in the RAMfiles we have.
	 */
	rfil_t *sl;
	char *rdn;

	/* Eliminate any leading U: or R: volume name */
	if ((strncasecmp(path, "R:", 2) == 0) ||
	    (strncasecmp(path, "U:", 2) == 0))
		path += 2;

	/*
	 * All RAMfiles are chroot'ed and the root path isn't stored as
	 * part of the filename.
	 */
	rdn = RAMfile_striproot(path);

	/*
	 * Eliminate leading slashes as well.
	 */
	while (*rdn == '/')
		rdn++;

	/*
	 * We must match the entire remainder of the path with a file in
	 * the RAMfile list.
	 */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (!*rdn || strncmp(rdn, sl->name, strlen(rdn)) == 0) {
			static char pnebuf[NAMEBUFSIZE];
			char *pne;
			int  pnelen = 0;

			pne = sl->name + strlen(rdn);

			/*
			 * We match up to the end of the requested directory
			 * path.  Double check that our RAMfile path name is
			 * an EXACT match.  I.E., avoid the case where we
			 * think the RAMfile solaris/drivers/fish.bef is a
			 * match for a requested path solaris/driv.
			 */
			if (*rdn && *pne != '/')
				continue;

			/*
			 * We have a match on the directory part.
			 * The next chunk of the RAMfile name, either up to
			 * the next / or the end of the name, is potentially
			 * a new dirent.
			 */
			while (*pne == '/')
				pne++;
			while (*pne != '/' && *pne && pnelen < NAMEBUFSIZE-1)
				pnebuf[pnelen++] = *pne++;
			pnebuf[pnelen] = '\0';

			add_dentent(dentslist, pnebuf, pnelen);
		}
	}
}

/*
 *  add_dentent
 *	The entname we are passing in may or may not already be in the
 *	big list of dent entries we are passing in.  If it is, then
 *	do nothing, if it isn't, add it.
 */
void
add_dentent(dentbuf_list *dentslist, char *entname, int entlen)
{
	struct dirent *dep;
	dentbuf_list *db, *pdb;
	int dec, found;

	pdb = db = dentslist;
	found = 0;
	while (db) {
		dep = (struct dirent *)db->dentbuf;
		for (dec = 0; dec < db->numdents; dec++) {
			/* See if it's already in the entries */
			if (strcmp(dep->d_name, entname) == 0) {
				found = 1;
				break;
			}
			dep = (struct dirent *)((char *)dep + dep->d_reclen);
		}

		if (!found) {
			pdb = db;
			db = db->next;
		} else {
			break;
		}
	}

	if (!found) {
		/*
		 * No matches found. We should add this to the dirents.
		 */
		ushort reclen;

		db = pdb;
		reclen = roundup(sizeof (struct dirent) + entlen,
			sizeof (long));

		if ((char *)dep + reclen >=
		    (char *)db->dentbuf + RAMDENTBUFSZ) {
			/* No room for this entry */
			db->next = (dentbuf_list *)
				bkmem_alloc(sizeof (dentbuf_list));
			if (db->next == (dentbuf_list *)NULL) {
				prom_panic("No memory for dirent buffer");
			} else {
				db = db->next;
				db->numdents = 0;
				db->curdent = 0;
				db->next = (dentbuf_list *)NULL;
				dep = (struct dirent *)db->dentbuf;
			}
		}

		dep->d_reclen = reclen;
		(void) strcpy(dep->d_name, entname);
		db->numdents++;
	}
}
