/*
 *	nisln.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nisln.c	1.11	98/05/18 SMI"

/*
 * nisln.c
 *
 * nis+ link utility
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>

extern int 	optind;
extern char	*optarg;

extern nis_object nis_default_obj;

extern char *nisname_index();
extern void nisname_split(char *, char *, char *);


void
usage()
{
	fprintf(stderr, "usage: nisln [-D defaults] [-L] name linkname\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	char *defstr = 0;
	u_long flinks = 0, master = 0;
	char *name, *lname;
	nis_result *res, *ares;
	nis_object *obj, lobj;
	char *p, oname[NIS_MAXNAMELEN], fbase[NIS_MAXNAMELEN];
	char srch[NIS_MAXNAMELEN], base[NIS_MAXNAMELEN];
	nis_error s;
	ib_request ibr;
	int i, j;

	while ((c = getopt(argc, argv, "D:L")) != -1) {
		switch (c) {
		case 'D':
			defstr = optarg;
			break;
		case 'L':
			flinks = FOLLOW_LINKS;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 2)
		usage();

	name = argv[optind];
	lname = argv[optind+1];

	/*
	 * Get the object to link to.
	 */
	nisname_split(name, base, srch);
	if (base[0] == 0) {
		nis_perror(NIS_BADNAME, name);
		exit(1);
	}
	res = nis_lookup(base, flinks|MASTER_ONLY|EXPAND_NAME);
	if (res->status != NIS_SUCCESS) {
		nis_perror(res->status, base);
		exit(1);
	}
	obj = res->objects.objects_val;

	sprintf(fbase, "%s.", obj->zo_name);
	if (*(obj->zo_domain) != '.')
		strcat(fbase, obj->zo_domain);

	if (obj->zo_data.zo_type == NIS_DIRECTORY_OBJ) {
		fprintf(stderr, "\"%s\" is a directory!\n", fbase);
		exit(1);
	}

	if (srch[0]) {
		if (obj->zo_data.zo_type != NIS_TABLE_OBJ) {
			fprintf(stderr, "\"%s\" is not a table!\n", fbase);
			exit(1);
		}

		sprintf(oname, "%s,%s", srch, fbase);
		s = nis_get_request(oname, 0, 0, &ibr);
		if (s != NIS_SUCCESS) {
			nis_perror(s, oname);
			exit(1);
		}

		for (i = 0; i < ibr.ibr_srch.ibr_srch_len; i++) {
			for (j = 0; j < obj->TA_data.ta_cols.ta_cols_len; j++)
				if
			    (strcmp(ibr.ibr_srch.ibr_srch_val[i].zattr_ndx,
			    obj->TA_data.ta_cols.ta_cols_val[j].tc_name) == 0)
					break;
			if (j == obj->TA_data.ta_cols.ta_cols_len) {
				nis_perror(NIS_BADATTRIBUTE, name);
				exit(1);
			}
		}
	} else {
		memset(&ibr, 0, sizeof (ibr));
		ibr.ibr_name = fbase;
	}

	if (!nis_defaults_init(defstr))
		exit(1);

	/*
	 * Construct link object.
	 */
	lobj = nis_default_obj;
	lobj.zo_data.zo_type = NIS_LINK_OBJ;
	if (srch[0])
		lobj.LI_data.li_rtype = NIS_ENTRY_OBJ;
	else
		lobj.LI_data.li_rtype = obj->zo_data.zo_type;
	lobj.LI_data.li_attrs.li_attrs_len = ibr.ibr_srch.ibr_srch_len;
	lobj.LI_data.li_attrs.li_attrs_val = ibr.ibr_srch.ibr_srch_val;
	lobj.LI_data.li_name = ibr.ibr_name;

	ares = nis_add(lname, &lobj);
	if (ares->status != NIS_SUCCESS) {
		nis_perror(ares->status, "can't add link");
		exit(1);
	}

	exit(0);
}
