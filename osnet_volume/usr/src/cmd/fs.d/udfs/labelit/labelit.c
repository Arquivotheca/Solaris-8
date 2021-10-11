/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)labelit.c	1.5	99/05/28 SMI"

/*
 * Label a file system volume.
 */


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>

#include <sys/fs/udf_volume.h>
#include "ud_lib.h"

static uint8_t buf[MAXBSIZE];
static uint64_t off;
#define	BUF_LEN	0x200

typedef unsigned short unicode_t;

static void usage();
static void label(int32_t, char *, int32_t, char *, int32_t);
static void print_info(struct vds *, char *, int32_t);
static void label_vds(struct vds *, char *, int32_t,
				char *, int32_t, int32_t);

static int32_t
ud_convert2unicode(int8_t *mb, int8_t *comp, int32_t mblen)
{
	wchar_t buf4c[128];
	int32_t len, i, j;
	uint8_t c = 8;

	len = mbstowcs(buf4c, mb, mblen);

	for (i = 0; i < len; i++) {
		if (buf4c[i] & 0xFFFFFF00) {
			c = 16;
			break;
		}
	}

	comp[0] = c;
	j = 1;
	for (i = 0; i < len; i++) {
		if (c == 16) {
			comp[j] = (buf4c[i] & 0xFF00) >> 8;
		}
		comp[j++] = buf4c[i] & 0xFF;
	}

	return (j);
}

int
main(int32_t argc, char *argv[])
{
	int opt, fd, flags, ret;
	int fsname_len, volname_len;
	int8_t fsname[BUF_LEN];
	int8_t volname[BUF_LEN];

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	if ((argc != 2) && (argc != 4)) {
		usage();
	}
	while ((opt = getopt(argc, argv, "o:")) != EOF) {
		switch (opt) {

		case 'o':	/* specific options (none defined yet) */
			break;

		case '?':
			usage();
		}
	}

	if (argc > 2) {

		(void) bzero(fsname, BUF_LEN);
		(void) bzero(volname, BUF_LEN);
		/*
		 * There are restrictions on the
		 * length of the names
		 * fsname is 8 characters
		 * volume name is 32 characters
		 * The extra byte is for compression id
		 */
		fsname_len = ud_convert2unicode(argv[optind + 1],
				fsname, strlen(argv[optind + 1]));
		if (fsname_len > (8 + 1)) {
			(void) fprintf(stderr,
	gettext("udfs labelit: fsname can not be longer than 8 characters\n"));
			exit(31+1);
		}

		volname_len = ud_convert2unicode(argv[optind + 2],
				volname, strlen(argv[optind + 2]));
		if (volname_len > (32 + 1)) {
			(void) fprintf(stderr,
	gettext("udfs labelit: volume can not be longer than 32 characters\n"));
			exit(31+1);
		}
	} else {
		fsname_len = volname_len = 0;
	}

	/*
	 * Open special device
	 */
	if (fsname_len == 0) {
		flags = O_RDONLY;
	} else {
		flags = O_RDWR;
	}
	if ((fd = ud_open_dev(argv[optind], flags)) < 0) {
		(void) fprintf(stderr,
		gettext("udfs labelit: cannot open <%s> errorno <%d>\n"),
					argv[optind], errno);
		exit(1);
	}

	if ((ret = ud_fill_udfs_info(fd)) != 0) {
		goto close_dev;
	}

	if ((udfs.flags & VALID_UDFS) == 0) {
		ret = 1;
		goto close_dev;
	}

	label(fd, fsname, fsname_len, volname, volname_len);

close_dev:
	ud_close_dev(fd);

	return (ret);
}

static void
usage()
{
	(void) fprintf(stderr, gettext(
"udfs usage: labelit [-F udfs] [generic options] special [fsname volume]\n"));
	exit(31+1);
}

static void
label(int32_t fd, char *fsname, int32_t fsname_len,
		char *volume, int32_t volname_len)
{
	if ((fsname_len == 0) ||
		(volname_len == 0)) {
		if (udfs.flags & VALID_MVDS) {
			print_info(&udfs.mvds, "mvds", fd);
		}
		if (udfs.flags & VALID_RVDS) {
			print_info(&udfs.rvds, "rvds", fd);
		}
		return;
	} else {
		if (udfs.flags & VALID_MVDS) {
			label_vds(&udfs.mvds, fsname, fsname_len,
					volume, volname_len, fd);
		}
		if (udfs.flags & VALID_RVDS) {
			label_vds(&udfs.rvds, fsname, fsname_len,
					volume, volname_len, fd);
		}
		if (udfs.fsd_len != 0) {
			struct file_set_desc *fsd;

			off = udfs.fsd_loc * udfs.lbsize;
			if (ud_read_dev(fd, off, buf, udfs.fsd_len) != 0) {
				return;
			}

			/* LINTED */
			fsd = (struct file_set_desc *)buf;

			set_dstring(fsd->fsd_lvid, volume, 128);
			set_dstring(fsd->fsd_fsi, volume, 32);

			ud_make_tag(&fsd->fsd_tag, UD_FILE_SET_DESC,
				SWAP_32(fsd->fsd_tag.tag_loc),
				SWAP_16(fsd->fsd_tag.tag_crc_len));

			(void) ud_write_dev(fd, off, buf, udfs.fsd_len);
		}
	}
}

static void
print_info(struct vds *v, char *name, int32_t fd)
{
	uint8_t outbuf[BUF_LEN];
	struct pri_vol_desc *pvd;

	if (v->pvd_len != 0) {
		off = v->pvd_loc * udfs.lbsize;
		if (ud_read_dev(fd, off, buf, v->pvd_len) == 0) {

			/* LINTED */
			pvd = (struct pri_vol_desc *)buf;

			(void) ud_convert2local(
					(int8_t *)pvd->pvd_vsi,
					(int8_t *)outbuf, strlen(pvd->pvd_vsi));
			(void) fprintf(stdout,
				gettext("fsname in  %s : %s\n"),
					name, outbuf);

			(void) ud_convert2local(
					(int8_t *)pvd->pvd_vol_id,
					(int8_t *)outbuf,
					strlen(pvd->pvd_vol_id));
			(void) fprintf(stdout,
				gettext("volume label in %s : %s\n"),
					name, outbuf);
		}
	}
}

/* ARGSUSED */
static void
label_vds(struct vds *v, char *fsname, int32_t fsname_len,
	char *volume, int32_t volname_len, int32_t fd)
{
	if (v->pvd_len) {
		struct pri_vol_desc *pvd;

		off = v->pvd_loc * udfs.lbsize;
		if (ud_read_dev(fd, off, buf, udfs.fsd_len) == 0) {

			/* LINTED */
			pvd = (struct pri_vol_desc *)buf;
			bzero((int8_t *)&pvd->pvd_vsi[9], 119);
			(void) strncpy((int8_t *)&pvd->pvd_vsi[9],
					&fsname[1], fsname_len - 1);

			set_dstring(pvd->pvd_vol_id, volume, 32);

			ud_make_tag(&pvd->pvd_tag,
				SWAP_16(pvd->pvd_tag.tag_id),
				SWAP_32(pvd->pvd_tag.tag_loc),
				SWAP_16(pvd->pvd_tag.tag_crc_len));

			(void) ud_write_dev(fd, off, buf, udfs.fsd_len);
		}
	}
	if (v->iud_len) {
		struct iuvd_desc *iuvd;

		off = v->iud_loc * udfs.lbsize;
		if (ud_read_dev(fd, off, buf, udfs.fsd_len) == 0) {

			/* LINTED */
			iuvd = (struct iuvd_desc *)buf;
			set_dstring(iuvd->iuvd_lvi, volume, 128);

			ud_make_tag(&iuvd->iuvd_tag,
				SWAP_16(iuvd->iuvd_tag.tag_id),
				SWAP_32(iuvd->iuvd_tag.tag_loc),
				SWAP_16(iuvd->iuvd_tag.tag_crc_len));

			(void) ud_write_dev(fd, off, buf, udfs.fsd_len);
		}
	}
	if (v->lvd_len) {
		struct log_vol_desc *lvd;

		off = v->lvd_loc * udfs.lbsize;
		if (ud_read_dev(fd, off, buf, udfs.fsd_len) == 0) {

			/* LINTED */
			lvd = (struct log_vol_desc *)buf;
			set_dstring(lvd->lvd_lvid, volume, 128);

			ud_make_tag(&lvd->lvd_tag,
				SWAP_16(lvd->lvd_tag.tag_id),
				SWAP_32(lvd->lvd_tag.tag_loc),
				SWAP_16(lvd->lvd_tag.tag_crc_len));

			(void) ud_write_dev(fd, off, buf, udfs.fsd_len);
		}
	}
}
