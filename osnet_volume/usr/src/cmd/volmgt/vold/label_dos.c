/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)label_dos.c	1.32	96/11/22 SMI"

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	<string.h>
#if	defined(_FIRMWARE_NEEDS_FDISK)
#include	<sys/dktp/fdisk.h>
#endif
#include	<sys/vtoc.h>
#include	<sys/dkio.h>
#include	<sys/dklabel.h>
#include	<sys/fs/pc_label.h>
#include	<sys/fs/pc_fs.h>
#include	<sys/fs/pc_dir.h>

#include	"vold.h"


#if	defined(DEBUG_LABEL) && !defined(DEBUG_DOS_LABEL)
#define	DEBUG_DOS_LABEL
extern char	*laread_res_to_str(enum laread_res);
#endif


/*
 * this is the dos label driver
 *
 * it attempts to interpret the dos lable on a disk
 *
 *
 * if no label can be found then we'll look for an FDISK table, and,
 * if found, find the first DOS partition available
 */


/* fwd declarations */
static bool_t 		dos_compare(label *, label *);
static enum laread_res	dos_read(int, label *, struct devs *dp);
static void		dos_setup(vol_t *);
static char		*dos_key(label *);
static void		dos_xdr(label *, enum xdr_op, void **);

/*
 * NOTE: most DOS defs come from <sys/fs/pc_*.h>, to avoid having to
 *  recompile them
 */
#define	DOS_NAMELEN_REG		PCFNAMESIZE
#define	DOS_NAMELEN_EXT		PCFEXTSIZE
#define	DOS_NAMELEN		(DOS_NAMELEN_REG + DOS_NAMELEN_EXT)

/*
 * this is the datastructure that we keep around for each dos
 * label to allow us to identify it.
 */
struct dos_label {
	u_short	dos_version;	/* version of this structure for db */
	u_long	dos_lcrc;			/* crc of label */
	u_long	dos_magic;			/* pseudo rand # from label */
	uchar_t	dos_volname[DOS_NAMELEN+1];	/* name from label */
};

#define	DOS_VERSION	1
#define	DOS_SIZE	sizeof (struct dos_label)

/* Begin definitions from DOS book... */

/*
 * Number of bytes that contain the "volume header" on a dos disk
 * at sector 0.  Note: it's less on earlier version of DOS, but never more.
 */
#define	DOS_LABLEN	0x3e

/*
 * Since we can't read anything but a multiple of sector size out of
 * the raw device, this is the amount that we read.
 *
 * NOTE: sector size is 512 on most formats, but is 1024 on some
 */
#define	DOS_READLEN	(PC_SECSIZE * 4)

/*
 * Offset in the volume header of the pseudo random id.  This is only
 * valid for dos version 4.0 and later.  It took them that long to
 * figure it out.  So, if we look at this and find it to be zero,
 * we just take the time(2) and poke it back out there.
 */
#define	DOS_ID_OFF	0x27

/*
 * Offset in the volume header of the ascii name of the volume. This is
 * only valid for dos 4.0 and later.  We should also read the first
 * FAT and slurp a name out of there, if we can.
 */
#define	DOS_NAME_OFF	0x2b


/*
 * location/length of the OEM name and version
 */
#define	DOS_OEM_JUNK	0x3
#define	DOS_OEM_LENGTH	8

/*
 * OEM name/version of those lovely NEC 2.0 floppies
 */
#define	DOS_OEM_NEC2	"NEC 2.00"

/* End definitions from DOS book */

/* size of buffer used to print longlong numbers */
#define	FLOPPY_NUMBUFLEN	512

static struct label_loc	dos_labelloc = { 0, DOS_LABLEN };

static struct labsw	doslabsw = {
	dos_key, 	/* l_key */
	dos_compare, 	/* l_compare */
	dos_read, 	/* l_read */
	NULL, 		/* l_write */
	dos_setup, 	/* l_setup */
	dos_xdr, 	/* l_xdr */
	DOS_SIZE, 	/* l_size */
	DOS_SIZE, 	/* l_xdrsize */
	PCFS_LTYPE,	/* l_ident */
	1,		/* l_nll */
	&dos_labelloc,	/* l_ll */
};


/*
 * Initialization function, called by the dso loader.
 */
bool_t
label_init(void)
{
	label_new(&doslabsw);
	return (TRUE);
}


/*
 * legal dos filename/dir char
 *
 * I know it's ugly, but it's copied from pc_validchar() in the kernel (with
 * the exception that I won't allow spaces!)
 *
 * the reason isdigit(), isupper(), ... aren't used is that they are
 * char-set dependent, but DOS isn't
 */
static bool_t
dos_filename_char(char c)
{
	register char	*cp;
	static char valtab[] = {
		"$#&@!%()-{}<>`_\\^~|'"
	};


	/*
	 * Should be "$#&@!%()-{}`_^~' " ??
	 * From experiment in DOSWindows, "*+=|\[];:\",<>.?/" are illegal.
	 * See IBM DOS4.0 Tech Ref. B-57.
	 */

	if ((c >= 'A') && (c <= 'Z')) {
		return (TRUE);
	}
	if ((c >= '0') && (c <= '9')) {
		return (TRUE);
	}
	for (cp = valtab; *cp != NULLC; cp++) {
		if (c == *cp++) {
			return (TRUE);
		}
	}
	return (FALSE);
}


/*
 * check for a legal DOS label char
 *
 * if the char is okay as is, return it
 * if the char is xlateable, then xlate it (i.e. a space becomes an underscore)
 */
static int
dos_label_char(int c)
{
	/* check for alpha-numerics (digits or letters) */
	if (isalnum(c)) {
#ifdef	DEBUG_DOS_LABEL
		debug(1, "dos_label_char: returning alpha-num: '%c'\n", c);
#endif
		return (c);
	}

	/* check for spaces or tabs */
	if (isspace(c)) {
#ifdef	DEBUG_DOS_LABEL
		debug(1, "dos_label_char: returning '_' (from '%c')\n", c);
#endif
		return ('_');
	}

	/* check for other chars */
	switch (c) {
	case '.':
	case '_':
	case '+':
	case '-':
#ifdef	DEBUG_DOS_LABEL
		debug(1, "dos_label_char: returning spcl char '%c'\n", c);
#endif
		return (c);
	}

#ifdef	DEBUG_DOS_LABEL
	debug(1, "dos_label_char: returning NULLC\n");
#endif
	return (NULLC);
}


/*
 * read the label off the disk and build an sl label structure
 * to represent it
 */
static enum laread_res
dos_read(int fd, label *la, struct devs *dp)
{
	static enum laread_res	dos_read_offset(int, label *, struct devs *,
				    off_t);
#if	defined(_FIRMWARE_NEEDS_FDISK)
	static bool_t		find_dos_part_offset(int, off_t *);
	off_t			offset;
#endif
	enum laread_res		retval = L_UNRECOG;



	debug(1, "dos_read: entering, fd = %d\n", fd);

	/* try no offset */
	retval = dos_read_offset(fd, la, dp, 0L);
#if	defined(_FIRMWARE_NEEDS_FDISK)
	if (retval == L_UNRECOG) {
		if (find_dos_part_offset(fd, &offset)) {
			/* we found fdisk info */
			retval = dos_read_offset(fd, la, dp, offset);
		}
	}
#endif

#ifdef	DEBUG_DOS_LABEL
	debug(1, "dos_read: returning %s (%d)\n",
	    laread_res_to_str(retval), (int)retval);
#endif
	return (retval);
}


#if		defined(_FIRMWARE_NEEDS_FDISK)
/*
 * find DOS partitions offset *iff* there's no Solaris partition *at all*
 */
static bool_t
find_dos_part_offset(int fd, off_t *offsetp)
{
	bool_t		res = FALSE;
	char		mbr[DOS_READLEN];	/* master boot record buf */
	int		err;
	struct mboot	*mbp;
	int		i;
	struct ipart	*ipp;
	int		dos_index = -1;



#ifdef	DEBUG_DOS_LABEL
	debug(5, "find_dos_part_offset: fd = %d\n", fd);
#endif

	/* seek to start of disk (assume it works) */
	(void) lseek(fd, 0L, SEEK_SET);

	/* try to read */
	if ((err = read(fd, mbr, DOS_READLEN)) != DOS_READLEN) {
		if (err < 0) {
			debug(1,
			"find_dos_part_offset: FDISK read failed; %m\n");
		} else {
			debug(1,
		"find_dos_part_offset: short FDISK read (%d); %m\n",
			    err);
		}
		goto dun;
	}

	/* get pointer to master boot struct and validate */
	mbp = (struct mboot *)mbr;
	if (mbp->signature != MBB_MAGIC) {
		debug(3,
		    "find_dos_part_offset: FDISK magic %x AFU (%x expected)\n",
		    mbp->signature, MBB_MAGIC);
		goto dun;
	}

	/* scan fdisk entries, looking for first BIG-DOS/DOSOS16 entry */
	for (i = 0; i < FD_NUMPART; i++) {
		ipp = (struct ipart *)&(mbp->parts[i * sizeof (struct ipart)]);
		if ((ipp->systid == DOSOS16) ||
		    (ipp->systid == DOSHUGE)) {
			dos_index = i;
		}
		if (ipp->systid == SUNIXOS) {
			/* oh oh -- not suposed to be solaris here! */
			goto dun;
		}
	}

	/* see if we found a match */
	if (dos_index >= 0) {
		res = TRUE;
		*offsetp = ipp->relsect * PC_SECSIZE;
	}

dun:
#ifdef	DEBUG_DOS_LABEL
	if (res) {
		debug(5, "find_dos_part_offset: SUCCESS, offset=%ld\n",
		    *offsetp);
	} else {
		debug(5, "find_dos_part_offset: FAILURE\n");
	}
#endif
	return (res);
}
#endif	/* _FIRMWARE_NEEDS_FDISK */


static enum laread_res
dos_read_offset(int fd, label *la, struct devs *dp, off_t offset)
{
	static char		*dos_rootdir_volname(int, char *);
	extern u_long		unique_key(char *, char *);
	u_char			dos_buf[DOS_READLEN];
	char			*type = dp->dp_dsw->d_mtype;
	struct dos_label	*dlp = 0;
	enum laread_res		retval;
	int			i;
	uchar_t			c;
	int			err;
	char			*cp;
	unsigned int		fat_off;



#ifdef	DEBUG_DOS_LABEL
	debug(1, "dos_read_offset: fd = %d, offset = %ld\n", fd, offset);
#endif

	if (lseek(fd, offset, SEEK_SET) != offset) {
		debug(1, "dos_read_offset: can't seek to %ld; %m\n", offset);
		retval = L_ERROR;
		goto out;
	}

	/* try to read the first sector */
	if ((err = read(fd, dos_buf, DOS_READLEN)) != DOS_READLEN) {
		if (err == -1) {
			/* couldn't read anything */
			debug(1, "dos_read_offset: read of \"%s\"; %m\n",
			    dp->dp_path);
		} else {
			/* couldn't read a whole sector! */
			debug(1, "dos_read_offset: short read of label\n");
		}
		retval = L_UNFORMATTED;
		goto out;			/* give up */
	}

	/* try to interpret the data from the first sector */
	if ((*dos_buf != (u_char)DOS_ID1) &&
	    (*dos_buf != (u_char)DOS_ID2a)) {
		debug(3, "dos_read_offset: jump instruction missing/wrong\n");
		retval = L_UNRECOG;
		goto out;			/* give up */
	}

	/* calculate where FAT starts */
	fat_off = ltohs(dos_buf[PCB_BPSEC]) * ltohs(dos_buf[PCB_RESSEC]);

	/* if offset is too large we probably have garbage */
	if (fat_off >= sizeof (dos_buf)) {
		debug(3, "dos_read_offset: FAT offset out of range\n");
		retval = L_UNRECOG;
		goto out;
	}

	if ((dos_buf[PCB_MEDIA] != dos_buf[fat_off]) ||
	    ((uchar_t)0xff != dos_buf[fat_off + 1]) ||
	    ((uchar_t)0xff != dos_buf[fat_off + 2])) {
		debug(3, "dos_read_offset: FAT not cool!\n");
		retval = L_UNRECOG;
		goto out;
	}

#ifdef	DEBUG_DOS_LABEL
	debug(5, "dos_read_offset: found a DOS label!\n");
#endif

	/* create a label structure */
	la->l_label = (void *)calloc(1, sizeof (struct dos_label));
	dlp = (struct dos_label *)la->l_label;

	/*
	 * Get the volume name out of the volume label.
	 *
	 * Earlier versions of DOS have the label name (if any) at
	 * DOS_NAME_OFF.  If no label is there (as in newer DOSes), then
	 * look in the root directory.
	 */
	if (dos_filename_char(dos_buf[DOS_NAME_OFF])) {
#ifdef	DEBUG_DOS_LABEL
		debug(1,
		"dos_read_offset: copying dos_volname from byte %d of blk 0\n",
		    DOS_NAME_OFF);
#endif
		/* must be a legal dos name -- xfer it */
		for (i = 0; i < DOS_NAMELEN; i++) {
			if ((c = dos_buf[DOS_NAME_OFF + i]) == NULLC) {
				break;
			}
			if ((c = dos_label_char(c)) == NULLC) {
				break;
			}
			dlp->dos_volname[i] = c;
#ifdef	DEBUG_DOS_LABEL
			debug(1, "dos_read_offset: added volname char '%c'\n",
			    dlp->dos_volname[i]);
#endif
		}

		/* null terminate */
		dlp->dos_volname[i] = NULLC;

		/* trip off any "spaces" at the end */
		while (--i > 0) {
			if (dlp->dos_volname[i] != '_') {
				break;
			}
			dlp->dos_volname[i] = NULLC;
		}

	} else {

		/* look in the root dir */
		if ((cp = dos_rootdir_volname(fd, (char *)dos_buf)) != NULL) {
			(void) strcpy((char *)(dlp->dos_volname), cp);
		} else {
			dlp->dos_volname[0] = NULLC;
		}
	}

	/*
	 * ceck for NEC DOS 2.0 (this should probably just be any DOS
	 * version earlier than 4)
	 *
	 * if found then we have a non-unique piece of ... media
	 *
	 * XXX: this should be more general, but this is the only type of
	 * media I've seen so far that doesn't have the "id" field
	 */
	if (strncmp((char *)(dos_buf + DOS_OEM_JUNK), DOS_OEM_NEC2,
	    DOS_OEM_LENGTH) == 0) {
		retval = L_NOTUNIQUE;
		goto out;
	}

	/*
	 * read the pseudo random thing.  unfortunatly, it's not
	 * aligned on a word boundary otherwise I'd just cast...
	 */
	(void) memcpy(&dlp->dos_magic, &dos_buf[DOS_ID_OFF], sizeof (u_long));

	/*
	 * if it's zero (probably a pre-dos4.0 disk), we'll just
	 * slap a number in there
	 */
	if (dlp->dos_magic == 0) {
		if (dp->dp_writeprot || never_writeback) {
			retval = L_NOTUNIQUE;
			goto out;
		}
		dlp->dos_magic = unique_key(type, PCFS_LTYPE);
		(void) memcpy(&dos_buf[DOS_ID_OFF], &dlp->dos_magic,
		    sizeof (u_long));
		if (lseek(fd, offset, SEEK_SET) != offset) {
			warning(gettext(
			    "dos read: couldn't seek for write back; %m\n"));
			retval = L_NOTUNIQUE;
			goto out;
		}
		if (write(fd, dos_buf, DOS_READLEN) < 0) {
			warning(gettext(
			"dos_read_offset: couldn't write back label; %m\n"));
			retval = L_NOTUNIQUE;
			goto out;
		}
		debug(6, "dos_read_offset: wroteback %#x as unique thing\n",
		    dlp->dos_magic);
	}

	/*
	 * ok, we're happy now -- we'll just continue along here
	 */
	dlp->dos_lcrc = calc_crc(dos_buf, DOS_LABLEN);

	retval = L_FOUND;
out:
	if ((retval != L_FOUND) && (retval != L_NOTUNIQUE)) {
		if (dlp != NULL) {
			free(dlp);
		}
		la->l_label = 0;
	}

#ifdef	DEBUG_DOS_LABEL
	if (retval == L_FOUND) {
		debug(1,
		    "dos_read_offset: returning %s (%d) -- label = \"%s\"\n",
		    laread_res_to_str(retval), (int)retval, dlp->dos_volname);
	} else {
		debug(1, "dos_read_offset: returning %s (%d)\n",
		    laread_res_to_str(retval), (int)retval);
	}
#endif

	return (retval);
}


static char *
dos_rootdir_volname(int fd, char *boot_buf)
{
	static char	ent_buf[DOS_NAMELEN + 2] = "";	/* room for '.' */
	char		*rp = NULL;		/* result pointer */
	ushort_t	root_sec;
	ushort_t	sec_size;
	uchar_t		*root_dir = NULL;
	struct pcdir	*root_entry;
	ushort_t	root_ind;
	ushort_t	root_dir_entries;
	size_t		root_dir_size;
	char		*cp;
	uint_t		i;
	uchar_t		c;



	/* find where the root dir should be */
	root_sec = ltohs(boot_buf[PCB_RESSEC]) +
	    ((ushort_t)boot_buf[PCB_NFAT] * ltohs(boot_buf[PCB_SPF]));
	sec_size = ltohs(boot_buf[PCB_BPSEC]);
	root_dir_entries = ltohs(boot_buf[PCB_NROOTENT]);
	root_dir_size = (size_t)(root_dir_entries * sizeof (struct pcdir));

	/*
	 * read in the root directory
	 */
	if ((root_dir = (uchar_t *)malloc(root_dir_size)) == NULL) {
#ifdef	DEBUG
		debug(1, "dos_rootdir_volname: can't alloc memory; %m\n");
#endif
		goto dun;
	}
	if (lseek(fd, (root_sec * sec_size), SEEK_SET) < 0) {
#ifdef	DEBUG
		debug(1, "dos_rootdir_volname: can't seek; %m\n");
#endif
		goto dun;
	}
	if (read(fd, root_dir, root_dir_size) != root_dir_size) {
#ifdef	DEBUG
		debug(1, "dos_rootdir_volname: can't read root dir; %m\n");
#endif
		goto dun;
	}

	for (root_ind = 0; root_ind < root_dir_entries; root_ind++) {

		/*LINTED: alignment ok*/
		root_entry = (struct pcdir *)&root_dir[root_ind *
		    sizeof (struct pcdir)];

		if (root_entry->pcd_filename[0] == PCD_UNUSED) {
			break;			/* end of entries */
		}
		if (root_entry->pcd_filename[0] == PCD_ERASED) {
			continue;		/* erased */
		}
		if ((root_entry->pcd_attr & PCDL_LFN_BITS) == PCDL_LFN_BITS) {
			continue;		/* long filename */
		}

		if (root_entry->pcd_attr & PCA_LABEL) {

			/* found it! - now extract it */

			rp = ent_buf;		/* result pointer */
			cp = ent_buf;

			/* treat name+extension as one entity */

			/* get the name part */
			if (dos_filename_char(root_entry->pcd_filename[0])) {
				for (i = 0; i < DOS_NAMELEN_REG; i++) {
					c = root_entry->pcd_filename[i];
					if (!dos_label_char(c)) {
						break;
					}
					*cp++ = (char)c;
				}
			}

			/* if name was full then look at extension field */
			if (i == DOS_NAMELEN_REG) {
				for (i = 0; i < DOS_NAMELEN_EXT; i++) {
					c = root_entry->pcd_ext[i];
					if (!dos_label_char(c)) {
						break;
					}
					*cp++ = (char)c;
				}
			}

			/* null terminate, hoser */
			*cp = NULLC;

			break;
		}

	}

dun:
	if (root_dir != NULL) {
		free(root_dir);
	}
	return (rp);
}


static char *
dos_key(label *la)
{
	char			buf[FLOPPY_NUMBUFLEN];
	struct dos_label	*dlp = (struct dos_label *)la->l_label;


	(void) sprintf(buf, "0x%lx", dlp->dos_magic);
	return (strdup(buf));
}


static bool_t
dos_compare(label *la1, label *la2)
{
	struct dos_label	*dlp1;
	struct dos_label	*dlp2;


	dlp1 = (struct dos_label *)la1->l_label;
	dlp2 = (struct dos_label *)la2->l_label;

	/* easy first wack to see if they're different */
	if (dlp1->dos_lcrc != dlp2->dos_lcrc) {
		return (FALSE);
	}
	if (dlp1->dos_magic != dlp2->dos_magic) {
		return (FALSE);
	}
	return (TRUE);
}


static void
dos_setup(vol_t *v)
{
	struct dos_label	*dlp = (struct dos_label *)v->v_label.l_label;
	int			do_unnamed = 0;
	char			unnamed_buf[MAXNAMELEN+1];


	/* name selection... ya gotta love it! */
	if (dlp->dos_volname[0] != NULLC) {
#ifdef	DEBUG_DOS_LABEL
		debug(5,
		"dos_setup: calling makename(\"%s\",  %d) (from volname)\n",
		    dlp->dos_volname, DOS_NAMELEN);
#endif
		v->v_obj.o_name = makename((char *)(dlp->dos_volname),
		    DOS_NAMELEN);
		if (v->v_obj.o_name[0] == NULLC) {
			/* makename() couldn't make a name */
			do_unnamed++;
			free(v->v_obj.o_name);
		}
#ifdef	DEBUG_DOS_LABEL
		else {
			debug(5, "dos_setup: makename() returned \"%s\"\n",
			    v->v_obj.o_name);
		}
#endif
	} else {
		do_unnamed++;
	}

	if (do_unnamed) {
		/*
		 * we could be either floppy or pcmem (or whatever), so
		 * we must use "media type" here
		 */
		(void) sprintf(unnamed_buf, "%s%s", UNNAMED_PREFIX,
		    v->v_mtype);
		v->v_obj.o_name = strdup(unnamed_buf);
	}

#ifdef	DEBUG
	debug(5, "dos_setup: DOS media given name \"%s\"\n",
	    v->v_obj.o_name);
#endif

	/* just one partition ever */
	v->v_ndev = 1;

	/* XXX: say that it is available to the whole domain */
	v->v_flags |= V_NETWIDE;
}


static void
dos_xdr(label *l, enum xdr_op op, void **data)
{
	XDR			xdrs;
	struct dos_label	*dlp;
	struct dos_label	sdl;
	char			*s = NULL;


	/* if size is zero then fill it in */
	if (doslabsw.l_xdrsize == 0) {
		/* add in size of "dos_version" */
		doslabsw.l_xdrsize += xdr_sizeof(xdr_u_short,
		    (void *)&sdl.dos_version);
		/* add in size of "dos_lcrc" */
		doslabsw.l_xdrsize += xdr_sizeof(xdr_u_long,
		    (void *)&sdl.dos_lcrc);
		/* add in size of "dos_magic" */
		doslabsw.l_xdrsize += xdr_sizeof(xdr_u_long,
		    (void *)&sdl.dos_magic);
		/*
		 * add in size of "dos_volname" --
		 * yes, well, this is a bit of a hack here, but
		 * I don't know of any other way to do it.
		 * I know that xdr_string encodes the string
		 * as an int + the bytes, so I just allocate that
		 * much space (silveri said it would work).
		 */
		doslabsw.l_xdrsize += DOS_NAMELEN + sizeof (int);
	}

	if (op == XDR_ENCODE) {

		dlp = (struct dos_label *)l->l_label;
		*data = malloc(doslabsw.l_xdrsize);
		xdrmem_create(&xdrs, *data, doslabsw.l_xdrsize, op);
		dlp->dos_version = DOS_VERSION;
		(void) xdr_u_short(&xdrs, &dlp->dos_version);
		(void) xdr_u_long(&xdrs, &dlp->dos_lcrc);
		(void) xdr_u_long(&xdrs, &dlp->dos_magic);
		s = (char *)(dlp->dos_volname);
		(void) xdr_string(&xdrs, &s, DOS_NAMELEN);
		xdr_destroy(&xdrs);

	} else if (op == XDR_DECODE) {

		xdrmem_create(&xdrs, *data, doslabsw.l_xdrsize, op);
		if (l->l_label == NULL) {
			l->l_label =
			    (void *)calloc(1, sizeof (struct dos_label));
		}
		dlp = (struct dos_label *)l->l_label;
		(void) xdr_u_short(&xdrs, &dlp->dos_version);
		/*
		 * here's where we'll deal with other versions of this
		 * structure...
		 */
		ASSERT(dlp->dos_version == DOS_VERSION);
		(void) xdr_u_long(&xdrs, &dlp->dos_lcrc);
		(void) xdr_u_long(&xdrs, &dlp->dos_magic);
		(void) xdr_string(&xdrs, &s, DOS_NAMELEN);
		/*
		 * xdr_string seems not to allocate any memory for
		 * a null string, and therefore s is null on return...
		 */
		if (s != NULL) {
			(void) strncpy((char *)(dlp->dos_volname), s,
			    DOS_NAMELEN);
			xdr_free(xdr_string, (void *)&s);
		}
		xdr_destroy(&xdrs);
	}
}
