/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)typ.c 1.10 95/02/28 SMI"

#include <assert.h>
#include <errno.h>
#include <libintl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"

conf_list_t *typ_head;

attr_list_t *
find_typ_info(char *name)
{
	return (find_attr_list(typ_head, NAME_ATTR, name));
}

conf_list_t *
find_typ_infoc(char *name)
{
	return (find_attr_listc(typ_head, NAME_ATTR, name));
}


char *
get_typ_group(char *name)
{
	conf_list_t *cf;
	char *members;

	/*
	 * See if <name> is the member of any group and return the
	 * group name.
	 */
	for (cf = typ_head; cf; cf = cf->next)
		if ((members = find_attr_str(cf->alist, MEMBERS_ATTR))) {
			int n = count_string(members);

			while (n--)
				if (streq(name, next_string(&members))) {
					char *group;
					group = find_attr_str(cf->alist,
						    GROUP_ATTR);
					return (next_string(&group));
				}
		}

	return (NULL);
}

static val_list_t *
make_typ_vlist(val_list_t *vlist, attr_list_t *typ)
{
	val_list_t *nvlist;

	nvlist = (val_list_t *)xmalloc(sizeof (val_list_t));
	memset(nvlist, 0, sizeof (*nvlist));


	if (vlist->next)
		nvlist->next = make_typ_vlist(vlist->next, typ);

	assert(vlist->val_type == VAL_STRING);

	/* If a var, get its value. */
	if (match(vlist->val.string, VAR_STRING)) {
		char *v_name = vlist->val.string + sizeof (VAR_STRING) - 1;
		vlist = find_attr_val(typ, v_name);
		if (vlist == NULL) {
			no_def_notice(v_name);
			return (NULL);
		}
	}

	if (match(vlist->val.string, NUMERIC_STRING)) {
		char *vs = vlist->val.string;
		int   n;
		unsigned int un;
		int usint = 0;

		next_numeric(&vs, &n, &usint);
		if (usint) {
			nvlist->val_type = VAL_UNUMERIC;
			nvlist->val.uinteger = (unsigned int) n;
		} else {
			nvlist->val_type = VAL_NUMERIC;
			nvlist->val.integer = n;
		}
	} else if (match(vlist->val.string, STRING_STRING)) {
		char *vs = vlist->val.string;
		char *s;

		s = next_string(&vs);
		nvlist->val_type = VAL_STRING;
		nvlist->val.string = s;
	} else {		/* Just copy the string itself. */
		nvlist->val_type = VAL_STRING;
		nvlist->val.string = xstrdup(vlist->val.string);
	}

	return (nvlist);
}


attr_list_t *
make_typ_attr(attr_list_t *val_string, attr_list_t *typ)
{
	attr_list_t *nalist;

	nalist = (attr_list_t *)xzmalloc(sizeof (attr_list_t));
	nalist->name = xstrdup(val_string->name);
	if (val_string->vlist)
		nalist->vlist = make_typ_vlist(val_string->vlist, typ);

	return (nalist);
}

attr_list_t *
make_typ_alist(attr_list_t *val_string, attr_list_t *typ)
{
	attr_list_t *nalist;

	/* Skip attributes that begin and end with underbar. */
	static char *re;

	if (re == NULL)
		re = regcmp(INTERNAL_ATTR, NULL);
	if (regex(re, val_string->name)) {
		if (val_string->next)
			return (make_typ_alist(val_string->next, typ));

		return (NULL);
	}

	nalist = (attr_list_t *)xzmalloc(sizeof (attr_list_t));

	if (val_string->next)
		nalist->next = make_typ_alist(val_string->next, typ);

	nalist->name = xstrdup(val_string->name);
	if (val_string->vlist)
		nalist->vlist = make_typ_vlist(val_string->vlist, typ);

	return (nalist);
}




/*ARGSUSED*/
static void
read_cfinfo(char *path, char *unused)
{
	conf_list_t *cf;
	conf_list_t *last;
	FILE *fp;

	errno = 0;
	if ((fp = fopen(path, "r")) == NULL) {
		vrb(MSG(READERR), path, strerror(errno));
		return;
	}

	if ((cf = read_conf(fp)) != NULL) {
		last = cf;
		while (last->next)
			last = last->next;
		last->next = typ_head;
		typ_head = cf;
	}

	fclose(fp);
}

void
fetch_typ_info(void)
{
	conf_list_t *typ, *prvtyp;

	/*
	 * Read all of the info files and chain the tree together.
	 */
	if (scan_dir(dvc_home(), ".*\\.cfinfo$", read_cfinfo) == 0)
		ui_error_exit(MSG(NOCFINFO));

	if (find_typ_info("master") == NULL)
		ui_error_exit(MSG(NOMASTER));

	fetch_pmi_info();

	/*
	 * Do prototype internal processing for each category.
	 * If a new set of prototypes are generated as a result
	 * (as in display case), replace the  prototype with the
	 * new set.
	 * (XXX free the old one).
	 */
	for (prvtyp = typ = typ_head; typ; prvtyp = typ, typ = typ->next) {
		conf_list_t *clist;

		if ((clist = cvt_typ_in(typ->alist)) != NULL) {
			conf_list_t *cl, *last;


			for (cl = last = clist; cl; last = cl, cl = cl->next)
				;

			last->next = typ->next;
			if (typ == typ_head)
				typ_head = clist;
			else
				prvtyp->next = clist;
			typ = last;

		}
	}
}
