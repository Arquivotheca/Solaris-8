/*
 *	nisrm.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nisrm.c	1.8	98/05/18 SMI"

/*
 * nisrm.c
 *
 * nis+ object removal utility
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>

extern int 	optind;
extern char	*optarg;

extern nis_object nis_default_obj;

extern char *nisname_index();


void
usage()
{
	fprintf(stderr, "usage: nisrm [-if] name ...\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int		c;
	char 		ask_remove = 0, force_remove = 0;
	u_long		expand;
	char 		*name;
	int		error = 0;
	nis_result 	*res, *rres;
	nis_object 	*obj;
	char 		fname[NIS_MAXNAMELEN], buf[BUFSIZ];

	while ((c = getopt(argc, argv, "if")) != -1) {
		switch(c) {
		case 'i':
			ask_remove = 1;
			break;
		case 'f' :
			force_remove = 1;
			break;
		default :
			usage();
		}
	}

	if (optind == argc)
		usage();

	while (optind < argc) {
		name = argv[optind++];

		if (name[strlen(name)-1] != '.')
			expand = EXPAND_NAME;
		else
			expand = 0;

		/*
		 * Get the object to remove.
		 */
		res = nis_lookup(name, expand|MASTER_ONLY);
		if (res->status != NIS_SUCCESS) {
			if (!force_remove) {
				nis_perror(res->status, name);
				error = 1;
			}
			goto loop;
		}

		sprintf(fname, "%s.", res->objects.objects_val[0].zo_name);
		if (*(res->objects.objects_val[0].zo_domain) != '.')
			strcat(fname, res->objects.objects_val[0].zo_domain);

		if (res->objects.objects_val[0].zo_data.zo_type == NIS_DIRECTORY_OBJ) {
			if (!force_remove) {
				fprintf(stderr, "\"%s\" is a directory!\n",
				    fname);
				error = 1;
			}
			goto loop;
		}

		if (ask_remove || expand) {
			printf("remove %s? ", fname);
			gets(buf);
			if (tolower(*buf) != 'y')
				goto loop;
		}

		obj = res->objects.objects_val;
		rres = nis_remove(fname, obj);
		if ((rres->status == NIS_PERMISSION) && force_remove) {
			obj->zo_access |= 0x08080808;
			nis_freeresult(rres);
			rres = nis_modify(fname, obj);
			if (rres->status == NIS_SUCCESS) {
				nis_freeresult(rres);
				rres = nis_remove(fname, NULL);
			}
		}
		if (rres->status != NIS_SUCCESS) {
			if (!force_remove) {
				nis_perror(rres->status, name);
				error = 1;
			}
		}
		nis_freeresult(rres);

	loop:
		nis_freeresult(res);
	}

	exit(error);
}
