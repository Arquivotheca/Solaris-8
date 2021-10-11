/*
 *	Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)ldmon.c	1.4	93/10/25 SMI"

/*
 * Program to create a `mon' buffer from a profiling structure created from
 * ld.so.1.
 * The default action is to create a gmon.out buffer suitable for use with
 * gprof(1).  The `-p' option can be used to create a (simpler) mon.out
 * buffer suitable for prof(1).
 *
 *	i)  ldmon -p	/usr/tmp/x.profile	/usr/tmp/x.mon.out
 *	    prof -g -m	/usr/tmp/x.mon.out	x.syms
 *
 *	ii) ldmon	/usr/tmp/x.profile	/usr/tmp/x.gmon.out
 *	    gprof -b	/usr/tmp/x.gmon.out	x.syms	| tr -d '\014'
 *
 * Note. the x.syms files represent the file being profiled who's symbols
 * are required to generate the profile output.  At this time prof(1)
 * refuses to read non-executable files, so use `dyntoexec' to create a
 * dummy symbol file.
 */
#include	<sys/types.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sys/mman.h>
#include	<memory.h>
#include	<stdlib.h>
#include	"profile.h"

extern	char *	optarg;
extern	int	optind;

static const char *	usage_msg = "usage: %s [-p] infile outfile\n";

main(int argc, char ** argv)
{
	int		pflag = 0;
	int 		fd, c, size, cgarcs;
	char *		in, * out;
	L_hdr * 	lhdr, hdr;
	M_hdr * 	mhdr;
	L_cgarc *	lcgarc;
	M_cgarc *	mcgarc;

	while ((c = getopt(argc, argv, "p")) != -1) {
		switch (c) {
		case 'p':
			pflag++;
			break;
		case '?':
			(void) fprintf(stderr, usage_msg, argv[0]);
			exit(1);
		default:
			break;
		}
	}

	if ((argc - optind) != 2) {
		(void) fprintf(stderr, usage_msg, argv[0]);
		exit(1);
	}

	/*
	 * open the profile buffer file created by ld.so and map it in.
	 */
	if ((fd = open(argv[optind], O_RDONLY)) == -1) {
		(void) fprintf(stderr, "can't open %s for reading\n",
			argv[optind]);
		exit(1);
	}
	lhdr = &hdr;
	if (read(fd, lhdr, sizeof (L_hdr)) != sizeof (L_hdr)) {
		(void) fprintf(stderr, "can't read header from %s\n",
			argv[optind]);
		exit(1);
	}
	if ((in = (char *)mmap(0, lhdr->hd_fsize, PROT_READ, MAP_SHARED,
		fd, 0)) == (char *)-1) {
		(void) fprintf(stderr, "can't mmap %s\n", argv[optind]);
		exit(1);
	}

	/*
	 * Determine the size of the output file.
	 */
	if (pflag) {
		/*
		 * If we just want a simple mon.out buffer then we need
		 * to reserve one entry for a dummy cnt structure, prof(1)
		 * needs at least one cnt entry filled in to be able to
		 * analyze the rest of the date.
		 */
		size = sizeof (M_hdr) + sizeof (M_cnt) + lhdr->hd_psize;
	} else {
		/*
		 * For a gmon.out buffer the size is proportional to the
		 * file size of the profiled information created by ld.so.1
		 */
		cgarcs = lhdr->hd_lcndx - 1;
		size = sizeof (M_hdr) + lhdr->hd_psize +
			(cgarcs * sizeof (M_cgarc));
	}

	/*
	 * Create the output file, truncate it to the right size and
	 * map it in.
	 */
	(void) unlink(argv[++optind]);
	if ((fd = open(argv[optind], O_RDWR|O_CREAT, 0666)) == -1) {
		(void) fprintf(stderr, "can't open %s for writing\n",
			argv[optind]);
		exit(1);
	}
	if (ftruncate(fd, size) == -1) {
		(void) fprintf(stderr, "can't truncate %s\n", argv[optind]);
		exit(1);
	}
	if ((out = (char *)mmap(0, size, PROT_WRITE, MAP_SHARED,
		fd, 0)) == (char *)-1) {
		(void) fprintf(stderr, "can't mmap %s\n", argv[optind]);
		exit(1);
	}

	/*
	 * Translate the input buffer to the new output buffer.
	 */
	/* LINTED */
	mhdr = (M_hdr *)out;
	mhdr->hd_lpc = 0;
	mhdr->hd_hpc = lhdr->hd_hpc;
	if (pflag) {
		M_cnt *	mcnt;
		/*
		 * If we just want a simple mon.out buffer then the header
		 * offset field contains the count of the number of call count
		 * entries to follow.  In this case we have reserved one dummy
		 * cnt structure and its is initialized to 1 (this is
		 * sufficient to have prof(1) analyze the rest of the file).
		 */
		/* LINTED */
		mcnt = (M_cnt *)(out + sizeof (M_hdr));
		mcnt->fc_mcnt = 1;
		out = out + sizeof (M_hdr) + sizeof (M_cnt);
		mhdr->hd_off = 1;
	} else {
		/*
		 * For a gmon.out buffer the header offset field contains
		 * the start address of the call count structures (which
		 * directly follow the profil(1) buffer).
		 */
		out = out + sizeof (M_hdr);
		mhdr->hd_off = sizeof (M_hdr) + lhdr->hd_psize;
	}

	/*
	 * Copy the profil(1) buffer, as is, from the ld.so structure to the
	 * output buffer structure.
	 */
	in = in + sizeof (L_hdr);
	(void) memcpy(out, in, lhdr->hd_psize);

	/*
	 * If we are creating a mon.out buffer, we're done.
	 */
	if (pflag)
		return (0);

	/*
	 * If we are creating a gmon.out buffer then traverse the call count
	 * elements of the ld.so.1 profile structure and create the appropriate
	 * gmon output buffer structures.
	 */
	/* LINTED */
	lcgarc = (L_cgarc *)(in + lhdr->hd_psize);
	/* LINTED */
	mcgarc = (M_cgarc *)(out + lhdr->hd_psize);

	for (c = 0; c < cgarcs; c++, lcgarc++, mcgarc++) {
		if (lcgarc->cg_to == 0)
			continue;
		/*
		 * If the `from' address is unknown then thie element was
		 * filled in during an initial relocation (ie. LD_BIND_NOW)
		 * but the associated function was never called.
		 */
		if (lcgarc->cg_from == PRF_UNKNOWN)
			continue;

		/*
		 * If the `from' address was outside the address range of the
		 * image being profiled then assign it to the high pc.  This
		 * effectively changes all calls to `etext'.  This looks
		 * pretty stupid in the gprof output, but its the only generic
		 * way I've found of keeping gprof happy so far.
		 */
		if (lcgarc->cg_from == PRF_OUTADDR)
			mcgarc->cg_from = (unsigned long)lhdr->hd_hpc;
		else
			mcgarc->cg_from = (unsigned long)lcgarc->cg_from;

		/*
		 * Record the function address and its associated call count.
		 */
		mcgarc->cg_to = (unsigned long)lcgarc->cg_to;
		mcgarc->cg_count = lcgarc->cg_count;
	}

	return (0);
}
