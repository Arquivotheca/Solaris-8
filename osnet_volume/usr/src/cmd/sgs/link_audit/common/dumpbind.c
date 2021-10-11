/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dumpbind.c	1.3	97/11/23 SMI"

#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/lwp.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sys/mman.h>
#include	<synch.h>
#include	<errno.h>

#include	"bindings.h"

void
usage()
{
	printf("usage: dumpbind [-pqsc] <bindings.data>\n");
	printf("\t-p\tdisplay output in parsable format\n");
	printf("\t-q\tquery all mutex_locks in data buffer\n");
	printf("\t-c\tclear all mutex_locks in data buffer\n");
	printf("\t-s\tset all mutex_locks in data buffer\n");
	printf("\t-b\tprint bucket usage statistics\n");
}

/*
 * Returns 1 if lock held - 0 otherwise.
 */
static int
query_lock(lwp_mutex_t * lock) {
	if (_lwp_mutex_trylock(lock) == 0) {
		_lwp_mutex_unlock(lock);
		return (0);
	} else
		return (1);
}

static void
query_buffer_locks(bindhead * bhp)
{
	int	i;
	int	bkt_locks_held = 0;

	printf("bh_strlock: ");
	if (query_lock(&bhp->bh_strlock) == 1)
		(void) printf("lock held\n");
	else
		(void) printf("free\n");
	printf("bh_lock: ");
	if (query_lock(&bhp->bh_lock) == 1)
		(void) printf("lock held\n");
	else
		(void) printf("free\n");

	printf("Buckets: %d - locks held:\n", bhp->bh_bktcnt);
	for (i = 0; i < bhp->bh_bktcnt; i++) {
		if (query_lock(&bhp->bh_bkts[i].bb_lock) == 1) {
			bkt_locks_held++;
			(void) printf("\tbkt[%d]: lock held\n", i);
		}
	}
	if (bkt_locks_held == 0)
		(void) printf("\tnone.\n");
	else
		(void) printf("\t[%d bucket(s) locked]\n", bkt_locks_held);
}

static void
clear_buffer_locks(bindhead * bhp)
{
	int	i;
	if (query_lock(&bhp->bh_strlock) == 1) {
		_lwp_mutex_unlock(&bhp->bh_strlock);
		printf("bh_strlock: cleared\n");
	}
	if (query_lock(&bhp->bh_lock) == 1) {
		_lwp_mutex_unlock(&bhp->bh_lock);
		printf("bh_lock: cleared\n");
	}
	for (i = 0; i < bhp->bh_bktcnt; i++) {
		if (query_lock(&bhp->bh_bkts[i].bb_lock) == 1) {
			_lwp_mutex_unlock(&bhp->bh_bkts[i].bb_lock);
			printf("bkt[%d]: lock cleared\n", i);
		}
	}
}

static void
set_buffer_locks(bindhead * bhp)
{
	int	i;
	for (i = 0; i < bhp->bh_bktcnt; i++)
		_lwp_mutex_lock(&bhp->bh_bkts[i].bb_lock);
	_lwp_mutex_lock(&bhp->bh_strlock);
	_lwp_mutex_lock(&bhp->bh_lock);
}


main(int argc, char **argv)
{
	int		fd;
	char *		fname;
	bindhead *	bhp;
	bindhead *	tmp_bhp;
	struct stat	stbuf;
	int		i;
	int		bflag = 0;
	int		pflag = 0;
	int		qflag = 0;
	int		cflag = 0;
	int		sflag = 0;
	extern char *	optarg;
	extern int	optind;
	int		c;
	char *		format_string;
	ulong_t		symcount;
	ulong_t		callcount;

	while ((c = getopt(argc, argv, "bspcq")) != EOF)
		switch (c) {
		case 'b':
			bflag++;
			break;
		case 'p':
			pflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 's':
			sflag++;
			break;
		case '?':
			usage();
			exit(1);
			break;
		}

	if (optind == argc) {
		usage();
		exit(1);
	}
	fname = argv[optind];
	if ((fd = open(fname, O_RDWR)) == -1) {
		fprintf(stderr, "dumpbindings: unable to open file: %s\n",
			fname);
		perror("open");
		exit(1);
	}
	if ((bhp = (bindhead *)mmap(0, sizeof (bindhead),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "dumpbind: mmap failed\n");
		perror("mmap");
		exit(1);
	}

	if (qflag) {
		query_buffer_locks(bhp);
		return (0);
	}

	if (cflag) {
		clear_buffer_locks(bhp);
		return (0);
	}
	if (sflag) {
		set_buffer_locks(bhp);
		return (0);
	}

	if ((tmp_bhp = (bindhead *)mmap(0, bhp->bh_size,
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "dumpbind: remap: mmap failed\n");
		perror("mmap");
		exit(1);
	}
	close(fd);

	munmap((caddr_t)bhp, sizeof (bindhead));
	bhp = tmp_bhp;

	if (pflag)
		format_string = "%s|%s|%8d\n";
	else {
		if (!bflag)
		    printf(
			"                           Bindings Summary Report\n\n"
			"Library                             Symbol"
			"                   Call Count\n"
			"----------------------------------------------"
			"--------------------------\n");
		format_string = "%-35s %-25s %5d\n";
	}

	symcount = 0;
	callcount = 0;
	for (i = 0; i < bhp->bh_bktcnt; i++) {
		int		ent_cnt;
		binding_entry *	bep;
		unsigned int	bep_off;
		bep_off = bhp->bh_bkts[i].bb_head;
		ent_cnt = 0;
		while (bep_off) {
			bep = (binding_entry *)((char *)bhp + bep_off);
			if (!bflag) {
				printf(format_string,
				    (char *)bhp + bep->be_lib_name,
				    (char *)bhp + bep->be_sym_name,
				    bep->be_count);
				symcount++;
				callcount += bep->be_count;
			}
			bep_off = bep->be_next;
			ent_cnt++;
		}
		if (bflag)
			printf("bkt[%d] - %d entries\n", i, ent_cnt);
	}

	if (!bflag && !pflag)
		printf(
			"----------------------------------------------"
			"--------------------------\n"
			"Symbol Count: %d    Call Count: %d\n\n",
			symcount, callcount);

	return (0);
}
