/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)protocmp.c	1.2	99/08/25 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>

#include "list.h"
#include "protocmp.h"
#include "proto_list.h"
#include "protodir.h"
#include "exception_list.h"

static elem_list first_list;
static char *first_file_name;

static elem_list second_list;
static char *second_file_name;

static elem_list exception_list;

static FILE *need_add_fp;
static char *need_add_file;
static FILE *need_rm_fp;
static char *need_rm_file;
static FILE *differ_fp;
static char *differ_file;


/*
 * default flag values
 */
static int verbose = 0;

static void
usage(void)
{
	(void) fputs("usage: protocmp [-guplmsLv][-e <file>] "
	    "<protolist|pkg dir> <protolist|pkg dir>\n", stderr);
	(void) fputs("   where:\n", stderr);
	(void) fputs("\t-g       : don't compare group\n", stderr);
	(void) fputs("\t-u       : don't compare owner\n", stderr);
	(void) fputs("\t-p       : don't compare permissions\n", stderr);
	(void) fputs("\t-l       : don't compare link counts\n", stderr);
	(void) fputs("\t-m       : don't compare major/minor numbers\n",
	    stderr);
	(void) fputs("\t-s       : don't compare symlink values\n", stderr);
	(void) fputs("\t-e <file>: exceptions file\n", stderr);
	(void) fputs("\t-L       : list filtered exceptions\n", stderr);
	(void) fputs("\t-v       : verbose output\n", stderr);
}


static void
open_output_files(void)
{
	if ((need_add_fp =
	    fopen((need_add_file = tempnam(NULL, "add")), "w")) == NULL) {
		perror(need_add_file);
		exit(1);
	}

	if ((need_rm_fp =
	    fopen((need_rm_file = tempnam(NULL, "rm")), "w")) == NULL) {
		perror(need_rm_file);
		exit(1);
	}

	if ((differ_fp =
	    fopen((differ_file = tempnam(NULL, "diff")), "w")) == NULL) {
		perror(differ_file);
		exit(1);
	}
}

static void
close_output_files(void)
{
	(void) fclose(need_add_fp);
	(void) fclose(need_rm_fp);
	(void) fclose(differ_fp);
}

static void
print_file(char *file)
{
	FILE	*fp;
	int	count;
	char	buff[BUF_SIZE];

	if ((fp = fopen(file, "r")) == NULL) {
		perror(need_add_file);
	}

	while (count = fread(buff, sizeof (char), BUF_SIZE, fp))
		(void) fwrite(buff, sizeof (char), count, stdout);
	(void) fclose(fp);
}

static void
print_header(void)
{
	(void) printf("%c %-30s %-20s %-4s %-5s %-5s %-5s %-2s %2s %2s %-9s\n",
	    'T', "File Name", "Reloc/Sym name", "perm", "owner", "group",
	    "inode", "lnk", "maj", "min", "package(s)");
	(void) puts("-------------------------------------------------------"
	    "-----------------------------------------------------");
}

static void
print_results(void)
{
	(void) puts("*******************************************************");
	(void) puts("*");
	(void) printf("* Entries found in %s, but not found in %s\n",
	    first_file_name, second_file_name);
	(void) puts("*");
	(void) puts("*******************************************************");
	print_header();
	print_file(need_add_file);
	(void) puts("*******************************************************");
	(void) puts("*");
	(void) printf("* Entries found in %s, but not found in %s\n",
	    second_file_name, first_file_name);
	(void) puts("*");
	(void) puts("*******************************************************");
	print_header();
	print_file(need_rm_file);
	(void) puts("*******************************************************");
	(void) puts("*");
	(void) printf("* Entries that differ between %s and %s\n",
	    first_file_name, second_file_name);
	(void) puts("*");
	(void) printf("* filea == %s\n", first_file_name);
	(void) printf("* fileb == %s\n", second_file_name);
	(void) puts("*");
	(void) puts("*******************************************************");
	(void) fputs("Unit   ", stdout);
	print_header();
	print_file(differ_file);
}

static void
clean_up(void)
{
	(void) unlink(need_add_file);
	(void) unlink(need_rm_file);
	(void) unlink(differ_file);
}

/*
 * do_compare(a,b)
 *
 * Args:
 *	different_types - see elem_compare() for explanation.
 */
static void
do_compare(elem *a, elem *b, int different_types)
{
	int	rc;

	if ((rc = elem_compare(a, b, different_types)) != 0) {
		(void) fputs("filea: ", differ_fp);
		print_elem(differ_fp, a);
		(void) fputs("fileb: ", differ_fp);
		print_elem(differ_fp, b);
		(void) fputs("    differ: ", differ_fp);

		if (rc & SYM_F)
			(void) fputs("symlink", differ_fp);
		if (rc & PERM_F)
			(void) fputs("perm ", differ_fp);
		if (rc & REF_F)
			(void) fputs("ref_cnt ", differ_fp);
		if (rc & TYPE_F)
			(void) fputs("file_type ", differ_fp);
		if (rc & OWNER_F)
			(void) fputs("owner ", differ_fp);
		if (rc & GROUP_F)
			(void) fputs("group ", differ_fp);
		if (rc & MAJMIN_F)
			(void) fputs("major/minor ", differ_fp);
		(void) putc('\n', differ_fp);
		(void) putc('\n', differ_fp);
	}
}

static void
check_second_vs_first(int verbose)
{
	int	i;
	elem	*cur;

	for (i = 0; i < second_list.num_of_buckets; i++) {
		for (cur = second_list.list[i]; cur; cur = cur->next) {
			if (!(cur->flag & VISITED_F)) {
				if ((first_list.type != second_list.type) &&
				    find_elem(&exception_list, cur,
				    FOLLOW_LINK)) {
					/*
					 * this entry is filtered, we don't
					 * need to do any more processing.
					 */
					if (verbose) {
						(void) printf(
						    "Filtered: Need Deletion "
						    "of:\n\t");
						print_elem(stdout, cur);
					}
					continue;
				}
				/*
				 * It is possible for arch specific files to be
				 * found in a protodir but listed as arch
				 * independent in a protolist file.  If this is
				 * a protodir vs. a protolist we will make
				 * that check.
				 */
				if ((second_list.type == PROTODIR_LIST) &&
				    (cur->arch != P_ISA) &&
				    (first_list.type != PROTODIR_LIST)) {
					/*
					 * do a lookup for same file, but as
					 * type ISA.
					 */
					elem	*e;

					e = find_elem_isa(&first_list, cur,
					    NO_FOLLOW_LINK);
					if (e) {
						do_compare(e, cur,
						    first_list.type -
						    second_list.type);
						continue;
					}
				}

				print_elem(need_rm_fp, cur);
			}
		}
	}
}

static void
check_first_vs_second(int verbose)
{
	int	i;
	elem	*e;
	elem	*cur;

	for (i = 0; i < first_list.num_of_buckets; i++) {
		for (cur = first_list.list[i]; cur; cur = cur->next) {
			if ((first_list.type != second_list.type) &&
			    find_elem(&exception_list, cur, FOLLOW_LINK)) {
				/*
				 * this entry is filtered, we don't need to do
				 * any more processing.
				 */
				if (verbose) {
					(void) printf("Filtered: Need "
					    "Addition of:\n\t");
					print_elem(stdout, cur);
				}
				continue;
			}

			/*
			 * Search package database for file.
			 */
			e = find_elem(&second_list, cur, NO_FOLLOW_LINK);

			/*
			 * It is possible for arch specific files to be found
			 * in a protodir but listed as arch independent in a
			 * protolist file.  If this is a protodir vs. a
			 * protolist we will make that check.
			 */
			if (!e && (first_list.type == PROTODIR_LIST) &&
			    (cur->arch != P_ISA) &&
			    (second_list.type != PROTODIR_LIST)) {
				/*
				 * do a lookup for same file, but as type ISA.
				 */
				e = find_elem_isa(&second_list, cur,
				    NO_FOLLOW_LINK);
			}

			if (!e && (first_list.type != PROTODIR_LIST) &&
			    (cur->arch == P_ISA) &&
			    (second_list.type == PROTODIR_LIST)) {
				/*
				 * do a lookup for same file, but as any
				 * type but ISA
				 */
				e = find_elem_mach(&second_list, cur,
				    NO_FOLLOW_LINK);
			}

			if (e == NULL)
				print_elem(need_add_fp, cur);
			else {
				do_compare(cur, e,
				    first_list.type - second_list.type);
				e->flag |= VISITED_F;
			}
		}
	}
}

static int
read_in_file(const char *file_name, elem_list *list)
{
	struct stat	st_buf;
	int		count = 0;

	if (stat(file_name, &st_buf) == 0) {
		if (S_ISREG(st_buf.st_mode)) {
			if (verbose) {
				(void) printf("file(%s): trying to process "
				    "as protolist...\n", file_name);
			}
			count = read_in_protolist(file_name, list, verbose);
		} else if (S_ISDIR(st_buf.st_mode)) {
			if (verbose)
				(void) printf("directory(%s): trying to "
				    "process as protodir...\n", file_name);
			count = read_in_protodir(file_name, list, verbose);
		} else {
			(void) fprintf(stderr,
			    "%s not a file or a directory.\n", file_name);
			usage();
			exit(1);
		}
	} else {
		perror(file_name);
		usage();
		exit(1);
	}

	return (count);
}

int
main(int argc, char **argv)
{
	int	errflg = 0;
	int	c;
	char	*exceptions_file = NULL;
	int	list_filtered_exceptions = NULL;

	while ((c = getopt(argc, argv, "guplmnsLe:v")) != EOF) {
		switch (c) {
		case 's':
			check_sym = 0;
			break;
		case 'm':
			check_majmin = 0;
			break;
		case 'g':
			check_group = 0;
			break;
		case 'u':
			check_user = 0;
			break;
		case 'l':
			check_link = 0;
			break;
		case 'p':
			check_perm = 0;
			break;
		case 'e':
			exceptions_file = optarg;
			break;
		case 'L':
			list_filtered_exceptions++;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			errflg++;
			break;
		}
	}

	if ((argc - optind) != 2) {
		usage();
		exit(1);
	}

	init_list(&first_list, HASH_SIZE);
	init_list(&second_list, HASH_SIZE);
	init_list(&exception_list, HASH_SIZE);

	first_file_name = argv[optind];
	(void) read_in_file(first_file_name, &first_list);

	second_file_name = argv[optind + 1];
	(void) read_in_file(second_file_name, &second_list);

	if (first_list.type != second_list.type)
		(void) read_in_exceptions(exceptions_file, &exception_list,
		    verbose);

	open_output_files();

	if (verbose)
		(void) puts("comparing build to packages...");

	check_first_vs_second(list_filtered_exceptions);

	if (verbose)
		(void) puts("checking over packages...");
	check_second_vs_first(list_filtered_exceptions);

	close_output_files();

	print_results();

	clean_up();

	return (0);
}
