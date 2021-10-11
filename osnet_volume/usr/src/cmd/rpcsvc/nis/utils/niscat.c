/*
 *	niscat.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)niscat.c	1.14	96/05/13 SMI"

/*
 * niscat.c
 *
 * nis+ table cat utility
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>

extern int 	optind;
extern char	*optarg;


#define	BINARY_STR "*BINARY*"


struct pl_data {
	unsigned flags;
	char ta_sep;
};

#define	PL_BINARY 1

int
print_line(tab, ent, udata)
	char *tab;
	nis_object *ent;
	void *udata;
{
	register entry_col *ec = ent->EN_data.en_cols.en_cols_val;
	register int ncol = ent->EN_data.en_cols.en_cols_len;
	register struct pl_data *d = (struct pl_data *)udata;
	register int i;
	int len;

	for (i = 0; i < ncol; i++) {
		if (i > 0)
			printf("%c", d->ta_sep);
		len = ec[i].ec_value.ec_value_len;
		if (len != 0) {
			if (ec[i].ec_flags & EN_BINARY) {
				if (d->flags & PL_BINARY) {
					fwrite(ec[i].ec_value.ec_value_val,
						1, len, stdout);
				} else {
					printf(BINARY_STR);
				}
			} else {
				printf("%s", ec[i].ec_value.ec_value_val);
			}
		}
	}
	printf("\n");

	return (0);
}

int
print_object(tab, ent, udata)
	char *tab;
	nis_object *ent;
	void *udata;
{
	nis_print_object(ent);
	return (0);
}


#define	F_HEADER 1
#define	F_OBJECT 2

void
usage()
{
	fprintf(stderr, "usage: niscat [-LAMhv] [-s sep] tablename ...\n");
	fprintf(stderr, "       niscat [-LPAM] -o name ...\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	unsigned flags = 0;
	char *name;
	nis_result *tres, *eres;
	char tname[NIS_MAXNAMELEN];
	struct pl_data pld;
	int ncol, i;
	u_long flinks = 0, fpath = 0, allres = 0, master = 0;
	int error = 0;
	u_long list_flags;
	nis_object *obj;

	/*
	 * By default, don't print binary data to ttys.
	 */
	pld.flags = (isatty(1))?0:PL_BINARY;

	pld.ta_sep = '\0';
	while ((c = getopt(argc, argv, "LPAMohvs:")) != -1) {
		switch (c) {
		case 'L' :
			flinks = FOLLOW_LINKS;
			break;
		case 'P':
			fpath = FOLLOW_PATH;
			break;
		case 'A':
			allres = ALL_RESULTS;
			break;
		case 'M':
			master = MASTER_ONLY;
			break;
		case 'h':
			flags |= F_HEADER;
			break;
		case 'v':
			pld.flags |= PL_BINARY;
			break;
		case 'o':
			flags |= F_OBJECT;
			break;
		case 's':
			if (strlen(optarg) != 1) {
				fprintf(stderr,
				    "separator must be a single character\n");
				exit(1);
			}
			pld.ta_sep = *optarg;
			break;
		default:
			usage();
		}
	}

	if (optind == argc) /* no table name */
		usage();

	while (optind < argc) {
		name = argv[optind++];

		if (flags & F_OBJECT) {
			if (*name == '[') {
				list_flags = fpath|allres|master|
						FOLLOW_LINKS|EXPAND_NAME;
				tres = nis_list(name, list_flags,
					print_object, 0);
				if ((tres->status != NIS_CBRESULTS) &&
				    (tres->status != NIS_NOTFOUND)) {
					nis_perror(tres->status, name);
					error = 1;
					goto loop;
				}
			} else {
				list_flags = flinks|master|EXPAND_NAME;
				tres = nis_lookup(name, list_flags);
				if (tres->status != NIS_SUCCESS) {
					nis_perror(tres->status, name);
					error = 1;
					goto loop;
				}
				nis_print_object(tres->objects.objects_val);
			}
			goto loop;
		}

		/*
		 * Get the table object using expand name magic.
		 */
		tres = nis_lookup(name, master|FOLLOW_LINKS|EXPAND_NAME);
		if (tres->status != NIS_SUCCESS) {
			nis_perror(tres->status, name);
			error = 1;
			goto loop;
		}

		/*
		 * Construct the name for the table that we found.
		 */
		sprintf(tname, "%s.", tres->objects.objects_val[0].zo_name);
		if (*(tres->objects.objects_val[0].zo_domain) != '.')
			strcat(tname, tres->objects.objects_val[0].zo_domain);

		/*
		 * Make sure it's a table object.
		 */
		if (tres->objects.objects_val[0].zo_data.zo_type != NIS_TABLE_OBJ) {
			fprintf(stderr, "%s is not a table!\n", tname);
			error = 1;
			goto loop;
		}

		/*
		 * Use the table's separator character when printing entries.
		 * Unless one was specified w/ -s
		 */
		if (pld.ta_sep == '\0') {
			pld.ta_sep
			    = tres->objects.objects_val[0].TA_data.ta_sep;
		}

		/*
		 * Print column names if F_HEADER is set.
		 */
		if (flags & F_HEADER) {
			obj = &tres->objects.objects_val[0];
			ncol = obj->TA_data.ta_cols.ta_cols_len;
			c = pld.ta_sep;
			printf("# ");
			for (i = 0; i < ncol; i++) {
				if (i > 0)
					printf("%c", c);
				printf("%s",
		obj->TA_data.ta_cols.ta_cols_val[i].tc_name);
			}
			printf("\n");
		}

		/*
		 * Cat the table using a callback function.
		 */
		list_flags = allres|master;
		eres = nis_list(tname, list_flags, print_line, (void *)&(pld));
		if ((eres->status != NIS_CBRESULTS) &&
		    (eres->status != NIS_NOTFOUND)) {
			nis_perror(eres->status, "can't list table");
			error = 1;
		}
		nis_freeresult(eres);

	loop:
		nis_freeresult(tres);
	}

	exit(error);
}
