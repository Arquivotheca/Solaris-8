/*
 * Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident "@(#)mkinet.c	1.2	96/03/11 SMI"

#undef _KERNEL

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 *  We will create a divot in the boot image at the exact point which gets
 *  loaded into memory address 0x35000.  This should be MAKE_DIVOT_OFFSET
 *  (0x35000 - 0x8000).  We'll save the bytes that are there into the new
 *  image at STORE_DIVOT_OFFSET, right after the DIVOT_SIGNATURE (which
 *  indicates to the booter the divot's existence.  The new image will then
 *  have an absolute jump to the start address placed in the divot spot.
 */
#define	POST_RPL_JUMP_ADDR	0x35000
#define	BOOT_START_ADDR		0x8000

#define	MAKE_DIVOT_OFFSET	((POST_RPL_JUMP_ADDR) - (BOOT_START_ADDR))
#define	STORE_DIVOT_OFFSET	0x7

unsigned short Divot_sig = 0x9090;

unsigned char The_divot[] = {
	0xea, 0x00, 0x80, 0x00, 0x00, 0x90, 0x90, 0x90
};

main(int argc, char **argv)
{
	struct stat 	sbuf;
	off_t		woff;
	void		*infile;
	char		*cpp;
	int 		ifd, ofd;


	if (argc < 3) {
		(void) printf("usage: mkdivot image_file new_image_file \n");
		exit(1);
	}

	if ((ifd = open(argv[1], O_RDONLY)) ==  -1) {
		perror("open source image input");
		exit(2);
	}

	if ((ofd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, 0777)) ==  -1) {
		perror("open output image binary");
		exit(3);
	}

	if (fstat(ifd, &sbuf) == -1) {
		perror("fstat");
		exit(1);
	}

	/*
	 *  mmap() source file so we can easily copy it.
	 */
	if ((infile = (void *)mmap(NULL, sbuf.st_size, PROT_READ,
			MAP_PRIVATE, ifd, 0)) == MAP_FAILED) {
		perror("mmap failed");
		exit(1);
	}

	/*
	 * duplicate the input file in the output file.
	 */
	if (write(ofd, (char *)infile, sbuf.st_size) < 0) {
		perror("duplicate source image");
		exit(1);
	}

	cpp = (char *)infile + MAKE_DIVOT_OFFSET;
	woff = (off_t)STORE_DIVOT_OFFSET;

	/*
	 * Seek to divot storage in new image.
	 */
	if (lseek(ofd, woff, SEEK_SET) != woff) {
		perror("seek to divot storage");
		exit(1);
	}

	/*
	 * Write a signature so boot will know it needs to replace a divot.
	 */
	if (write(ofd, (char *)&Divot_sig, sizeof (Divot_sig)) < 0) {
		perror("signing new image");
		exit(1);
	}

	/*
	 * Copy the original image's contents from the magic address.
	 */
	if (write(ofd, cpp, sizeof (The_divot)) < 0) {
		perror("preserving original bytes");
		exit(1);
	}

	/*
	 * Seek to magic offset in new image.
	 */
	woff = MAKE_DIVOT_OFFSET;
	if (lseek(ofd, woff, SEEK_SET) != woff) {
		perror("seek to divot");
		exit(1);
	}

	/*
	 * Write out the divot.
	 */
	if (write(ofd, (char *)The_divot, sizeof (The_divot)) < 0) {
		perror("writing divot");
		exit(1);
	}

	(void) close(ifd);
	(void) close(ofd);
	exit(0);
}
