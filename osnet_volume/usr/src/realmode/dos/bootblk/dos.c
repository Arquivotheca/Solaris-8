/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dos.c	1.11	99/02/11 SMI\n"

/*
 * This file contains high level file system routines like read,
 * write, stat, open
 */
#include <bioserv.h>          /* BIOS interface support routines */
#include "bootblk.h"
#include "disk.h"

/*
 *[]------------------------------------------------------------[]
 * | Globals used in this file					|
 *[]------------------------------------------------------------[]
 */
extern fat_cntlr_t	Fatc;
static _file_desc_t	File_table[1] = {0};
_dir_entry_p		Dsec = 0;
int			Last_dir_blk = -1;

/*
 *[]------------------------------------------------------------[]
 * | cd_dos -- change the directory which lookups start from	|
 *[]------------------------------------------------------------[]
 */
int
fs_cd(char *name)
{
        int rtn;
        _dir_entry_t d;

        if (EQ(name, "\\") || EQ(name, "/")) {
                /* ---- short cut ---- */
                setcurdir(CLUSTER_ROOTDIR);
                rtn = 0;
        }
        else if (lookuppn_dos(name, &d, 0))
                rtn -1;
        else {
		setcurdir(d.d_cluster);
                rtn = 0;
        }

	Dprintf(DBG_FLOW, ("fs_cd (dos) for %s %s\n", (char _FAR_ *)name,
		(char _FAR_ *)(rtn ? "failed" : "succeeded")));
        return rtn;
}

/*
 *[]------------------------------------------------------------[]
 * | stat_dos -- returns a directory pointer for the given	|
 * | file name. 						|
 *[]------------------------------------------------------------[]
 */
int
stat_dos(char *name, _dir_entry_p dp)
{
        return lookuppn_dos(name, dp, 0);
}

/*
 *[]------------------------------------------------------------[]
 * | fs_open -- open a filename and return a pointer which	|
 * | is then used by fs_read     				|
 *[]------------------------------------------------------------[]
 */
long
fs_open(char *n)
{
        _dir_entry_t d;
        _file_desc_p f = &File_table[0];

	Dprintf(DBG_FLOW, ("fs_open (dos) for %s\n", (char _FAR_ *)n));

        if (lookuppn_dos(n, &d, &f->f_di)) {
        	Dprintf(DBG_FLOW, ("fs_open: lookuppn_dos failed\n"));
                return (-1);
	}

        f->f_startclust = d.d_cluster;
        f->f_off = 0;
        f->f_len = d.d_size;

       	Dprintf(DBG_FLOW, ("fs_open: succeeded\n"));
        return ((long)f);
}

/*
 *[]------------------------------------------------------------[]
 * | fs_seek -- position the file pointer        		|
 *[]------------------------------------------------------------[]
 */
long
fs_seek(long fd, long addr, int whence)
{
	_file_desc_p f = (_file_desc_p)fd;
	long new_offset;

	switch (whence) {
	case 0:
		new_offset = addr;
		break;
	case 1:
		new_offset = f->f_off + addr;
		break;
	case 2:
		new_offset = f->f_len + addr;
		break;
	default:
		return (-1);
	}
	if (new_offset < 0) {
		new_offset = -1;
	}
	else {
		f->f_off = new_offset;
	}
	return (new_offset);
}

/*
 *[]------------------------------------------------------------[]
 * | fs_read -- read some number of bytes into buffer 		|
 *[]------------------------------------------------------------[]
 */
long
fs_read(long fd, char far *b, long c)
{
	_file_desc_p f = (_file_desc_p)fd;
        u_long	off = f->f_off;
        short	blk = f->f_startclust;
        u_long	sector;
        u_short	count = 0, xfer, i;
        char	*sp;			/* ---- cluster size buffer */

        if (f->f_off >= f->f_len) {
                return 0;
        }

        c = MIN(f->f_len - f->f_off, c);
        while (off >= bpc()) {
                blk = map_fat(blk, 0);
                off -= bpc();
                if (!validc_util(blk, 0)) {
                        printf_util("read: Bad map entry\n");
                        return -1;
                }
        }

	if ((sp = (char *)malloc_util(bpc())) == 0) {
		printf_util("No memory for read!\n");
		while (1);
	}
	DPrint(DBG_DOS_GEN, ("Read: %d: ", c));
	while (count < c) {
		sector = ctodb_fat(blk, 0);

		/* ---- find out where we are in the cluster ---- */
		i = btodb(off) % spc();

		/* ---- find out how much we can xfer. ---- */
		xfer = MIN((spc() - i) * SECSIZ, c - count);

		/*
		 * If our transfer size is in multiples of SECSIZ and the
		 * current offset is aligned on a sector block we can
		 * transfer directly to the incomming buffer. This is done
		 * for speed to avoid an extra bcopy if possible.
		 */
		if ((((c - count) % SECSIZ) == 0) && ((off % SECSIZ) == 0)) {

			/*
			 * since we can only get full sectors make
			 * sure that the xfer size reflects that.
			 */
			if (xfer % SECSIZ) xfer = dbtob(btodb(xfer));

			DPrint(DBG_DOS_GEN,
				("(BLK: xfer %d, i %d, sec %ld, b 0x%x)",
				xfer, i, sector, b));
			/* ---- get the data ---- */
			if (ReadSect_bios(b, sector + i, btodb(xfer))) {
				free_util((u_short)sp, bpc());
				return -1;
			}
		} else {
			DPrint(DBG_DOS_GEN, ("(BCOPY: xfer %d, sec %ld)",
				xfer, sector));
			if (ReadSect_bios(sp, sector, (int)spc())) {
				free_util((u_short)sp, bpc());
				return -1;
			}
			bcopy_util(&sp[off % bpc()], b, xfer);
		}
		b += xfer;
		count += xfer;
		off += xfer;
		blk = map_fat(blk, 0);
		if (!validc_util(blk, 0)) break;
	}
	free_util((u_short)sp, bpc());
	DPrint(DBG_DOS_GEN, (" count %d\n", count));

        f->f_off += count;
        return count;
}

/*
 *[]------------------------------------------------------------[]
 * | readdir_dos -- copy dos entry data for index given in arg	|
 * | 'di' and return the next new index (normally di + 1).	|
 *[]------------------------------------------------------------[]
 */
int
fs_readdir(int di, _dir_entry_p dp)
{
        short c = curdir();
        int dir_index = di;
        int EpC;
        u_long adjust, sector;
	int rd;

	/* ---- are we dealing with the root directory ---- */
	rd = c == 0;

	/* ---- number of directory entries per cluster for easier comps */
        EpC = DIRENTS * spc();

	/* ---- first time through get some memory for cache ---- */
	if ((Dsec == 0) &&
		(!(Dsec = (_dir_entry_p)malloc_util(bpc())))) {
		printf_util("No memory available\n");
		return -1;
	}

        while(validc_util(c, rd)) {
                /* ---- see if we have the right cluster ---- */
                if (dir_index < EpC)
                        break;
                dir_index -= EpC;
                c = map_fat(c, rd);
        }

        /* ---- If we've hit EOF return 0 as an indication ---- */
        if (!validc_util(c, rd)) {
                printf_util("EOF of directory, c=%d, validc_util=%d\n",
                	c, validc_util(c, rd));
                return 0;
        }

        /* ---- Figure out which disk block we need to read ---- */
	sector = ctodb_fat(c, rd);

	/* ---- Do we already have this cached? ---- */
        if (sector != Last_dir_blk) {
		/* ---- If we can't read the dir sector return an error ---- */
                if (ReadSect_bios((char *)Dsec, sector, spc())) {
			printf_util("Failed to read sector %ld\n", sector);
                        return -1;
                }
                Last_dir_blk = sector;
        }

        bcopy_util((char *)&Dsec[dir_index % EpC],
                (char *)dp, sizeof(_dir_entry_t));

        /* ---- Return next index ---- */
        return ++di;
}

/*
 *[]------------------------------------------------------------[]
 * | Low level routines which in general should not be access by|
 * | anyone outside of this file.				|
 *[]------------------------------------------------------------[]
 */

/*
 *[]------------------------------------------------------------[]
 * | copyup_dos -- convert a lower case name to THE STUPID 	|
 * | STUFF THAT DOS REQUIRES.					|
 *[]------------------------------------------------------------[]
 */
static void
copyup_dos(char *s, char *d, int cc)
{
	while (cc--)
		*d++ = toupper_util(*s++);
}


_dir_entry_t De_zero = {0};

/*
 *[]------------------------------------------------------------[]
 * | lookuppn_dos -- Look up a path name such as		|
 * | \Solaris\boot.rc. Need to parse apart the pathname and	|
 * | lookup each piece.						|
 *[]------------------------------------------------------------[]
 */
static int
lookuppn_dos(char *n, _dir_entry_p dp, _de_info_p de)
{
	short dir_blk = curdir();
        char name[8 + 1 + 3 + 1];   /* 8 for name, 1 for ., 3 for ext + null */
        char *p, *ep;
        _dir_entry_t dd;
        int err;

        if ((*n == '\\') || (*n == '/')) {
                /* ---- Restart the dir search at the top. ---- */
                dir_blk = CLUSTER_ROOTDIR;

                /* ---- eat directory separators ---- */
                while ((*n == '\\') || (*n == '/')) n++;
        }
        dd.d_cluster = dir_blk;

	Dprintf(DBG_FLOW, ("lookuppn_dos, cluster %d, name %s\n",
		dd.d_cluster, (char _FAR_ *)n));

	ep = &name[0] + sizeof(name);
        while (*n) {
                bzero_util(name, sizeof(name));
                p = &name[0];
                while (*n && *n != '\\' && *n != '/') {
			if (p != ep)
				*p++ = *n++;
			else
				return (-1); /* name to long */
		}

                if (lookup_dos(name, &dd, dir_blk, de)) {
                	Dprintf(DBG_FLOW, ("lookup_dos %s failed\n",
                		(char _FAR_ *)name));
                	return (-1);
		}

                /* ---- eat directory seperators ---- */
                while ((*n == '\\') || (*n == '/')) n++;

		/*
		 * if there's more pathname left current node must be
		 * a directory so that we can descend.
		 */
                if (*n && ((dd.d_attr & DE_DIRECTORY) == 0)) {
                	Dprintf(DBG_FLOW, ("%s is not a dir\n",
                		(char _FAR_ *)name));
                	return (-1);
                }

                /* ---- if dd is a file this next statement is a noop ---- */
                dir_blk = dd.d_cluster;

        }
        bcopy_util((char *)&dd, (char *)dp, sizeof(_dir_entry_t));

	Dprintf(DBG_FLOW, ("lookuppn_dos succeeded\n"));

        return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | lookup_dos -- find name 'n' in parent directory.		|
 *[]------------------------------------------------------------[]
 */
static int
lookup_dos(char *n, _dir_entry_p dp, short dir_blk, _de_info_p de)
{
        _dir_entry_t dir_sect[DIRENTS];
        int i, j, rd;
        u_long adjust, sector;

	/*
	 * are we working on the root directory? Need to know because
	 * the mapping between clusters and disk blocks is different
	 * along with the mapping for the next directory cluster.
	 * If the cluster is zero we've got the root.
	 */
	rd = dir_blk == 0;

        while (validc_util(dir_blk, rd)) {
		sector = ctodb_fat(dir_blk, rd);
                for (j = 0; j < spc(); j++) {
                        if (ReadSect_bios((char *)&dir_sect[0],
                            (long)(sector + j), 1))
                               return -1;
                        for (i = 0; i < DIRENTS; i++) {
                                /* ---- If never used, we reached eof ---- */
                                if (dir_sect[i].d_name[0] == 0)
                                        goto eod;

                                /* ---- Ignore deleted entries ---- */
                                if ((u_char)dir_sect[i].d_name[0] ==
				    (u_char)0xe5)
                                        continue;
                                /*
                                 * If the first byte of the file name is 5
                                 * according to DOS it's actually 0xE5
                                 */
                                if (dir_sect[i].d_name[0] == 0x5)
                                        dir_sect[i].d_name[0] = 0xE5;

				DPrint(DBG_LOOKUP_0, ("%.8s%.3s 0x%02x\n",
						      dir_sect[i].d_name,
						      dir_sect[i].d_ext,
						      dir_sect[i].d_attr));
                                if (entrycmp_dos(n, (char *)dir_sect[i].d_name,
                                    (char *)dir_sect[i].d_ext) == 0) {
                                        bcopy_util((char *)&dir_sect[i],
                                        	(char *)dp,
                                                sizeof(_dir_entry_t));
					if (de) {
						de->di_index = i;
						de->di_block = sector + j;
					}
					DPrint(DBG_LOOKUP_0, ("cluster %d\n",
						      dir_sect[i].d_cluster));
                                        return 0;
                                }
                        }
                }
                dir_blk = map_fat(dir_blk, rd);
        }

/*
 * I hate goto's but this is the only way to break out of two
 * for loops nested inside of a while loop. :-(
 * I wish C had a 'break <n>' construct.
 */
eod:
        return -1;
}

/*
 *[]------------------------------------------------------------[]
 * | entrycmp_dos -- see if filename compares with dos name &	|
 * | extension.							|
 *[]------------------------------------------------------------[]
 */
static int
entrycmp_dos(char *fn, char *name, char *ext)
{
        int i, wherethedot;
        char *op, *fne;
        int rtn;

	/*
	 *[]------------------------------------------------------------[]
	 * | Yuck! Special case. If current name is ".." or "." this	|
	 * | routine will not work because the following code is looking|
	 * | for "." as a seperator because the name and extension.	|
	 *[]------------------------------------------------------------[]
         */
        if (EQ(fn, "..") && EQN(name, "..      ", NAMESIZ))
        	return 0;
        if (EQ(fn, ".") && EQN(name, ".       ", NAMESIZ))
        	return 0;

	if (op = strchr_util(fn, '.')) {
		*op = '\0';
		fne = op + 1;
	}
	else
		fne = "   ";
	if (component_dos(fn, name, NAMESIZ) || component_dos(fne, ext, EXTSIZ))
		rtn = 1;
	else
		rtn = 0;
	if (op) *op = '.';
	return rtn;
}

/*
 *[]------------------------------------------------------------[]
 * | component_dos -- see if component of dos name matches	|
 * | file name. DOS name are padded with spaces which need to 	|
 * | be checked as well.					|
 *[]------------------------------------------------------------[]
 */
static int
component_dos(char *pn, char *d, int cs)
{
	int l;

	if (namecmp_dos(pn, d)) {
		return 1;
	}

	l = strlen_util(pn);
	if ((l < cs) && !EQN(&d[l], "         ", cs - l)) {
		return 1;
	}

	return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | namecmp_dos -- Does the ascii part of the name pass.	|
 *[]------------------------------------------------------------[]
 */
static int
namecmp_dos(char *s, char *t)
{
	while (*s && *t) if (toupper_util(*s++) != toupper_util(*t++)) return 1;
	return 0;
}
