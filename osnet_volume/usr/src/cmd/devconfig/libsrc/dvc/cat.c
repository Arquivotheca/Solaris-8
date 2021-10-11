/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)cat.c 1.3 93/12/14 SMI"

#include <assert.h>
#include <libintl.h>
#include <stdio.h>
#include <string.h>

#include "dvc.h"
#include "conf.h"
#include "dev.h"
#include "conf.h"
#include "util.h"

typedef struct dev_name_list {
	struct dev_name_list*	next;
	char*			name;
	char*			title;
} dev_name_list_t;

typedef struct cat_info {
	struct cat_info*	next;
	char*			name;
	char*			title;
	dev_name_list_t*	nlist;
	void*			ui;
} cat_info_t;

static cat_info_t*	cat_head;
static cat_info_t*	current_cat;
static dev_name_list_t*	current_dev_name;

static cat_info_t*
get_cat_node(int idx)
{
	cat_info_t* cp;
	int		i = 0;

	for ( cp=cat_head; cp; cp=cp->next )
		if ( i++ == idx ) {
			++idx;
			return cp;
		}

	return NULL;
}

char*
next_cat_title(void)
{
	cat_info_t* catp;
	static int	idx = 0;

	fetch_cat_info();

	catp = get_cat_node(idx++);
	if ( catp == NULL ) {
		idx = 0;
		return NULL;
	}

	return catp->title;
}

char*
get_cat_name(int idx)
{
	cat_info_t* catp;
	catp = get_cat_node(idx);
	return catp->name;
}

#ifdef	XXX
int
get_cat_idx(char* name)
{
	cat_info_t* cp;
	int idx;

	for (idx = 0, cp = cat_head; cp; cp = cp->next, idx++)
		if (streq(cp->name, name))
			return idx;

	return -1;
}
#endif	XXX

void
set_cat_idx(int idx)
{
	current_cat = get_cat_node(idx);
	current_dev_name = current_cat->nlist;
} 

char*
next_cat_dev_title(void)
{
	if ( current_dev_name ) {
		char* title = current_dev_name->title;
		current_dev_name = current_dev_name->next;
		return title;
	}
	/* Reset for next time around. */
	current_dev_name = current_cat->nlist;

	return NULL;
}

static char*
next_cat_dev_name(void)
{
	if ( current_dev_name ) {
		char* name = current_dev_name->name;
		current_dev_name = current_dev_name->next;
		return name;
	}
	/* Reset for next time around. */
	current_dev_name = current_cat->nlist;

	return NULL;
}

char*
get_cat_dev_name(int idx)
{
	char* name = NULL;
	int   i;

	for ( i=0; i<=idx; ++i )
		name = next_cat_dev_name();

	/* Reset for next time around. */
	current_dev_name = current_cat->nlist;

	return name;
}

void*
get_cat_ui_info(int idx)
{
	cat_info_t* cp = get_cat_node(idx);
	return cp->ui;
}

void
set_cat_ui_info(void* ui, int idx)
{
	cat_info_t* cp = get_cat_node(idx);
	cp->ui = ui;
}

void
fetch_cat_info(void)
{
	conf_list_t*	cf;

	ONCE();

	fetch_dev_info();

	for ( cf=typ_head; cf; cf=cf->next ) {
		val_list_t* vlist;

		if (find_attr_val(cf->alist, AUTO_ATTR))
			continue;

		vlist = find_attr_val(cf->alist, CAT_ATTR);
		if ( vlist ) {
			cat_info_t* cp;
			dev_name_list_t* nlist;

			assert(vlist->val_type == VAL_STRING);

			for ( cp=cat_head; cp; cp=cp->next ) {
				if ( streq(cp->name, vlist->val.string) )
					break;
			}
			/* If we didn't find this one in our list, add it. */
			if ( cp == NULL ) {
				cp = (cat_info_t*)xmalloc(sizeof *cp);
				memset(cp, 0, sizeof *cp);
				cp->name = vlist->val.string;
				cp->next = cat_head;
				cat_head = cp;

				cp->title = expand_abbr(cp->name);
			}

			nlist = (dev_name_list_t*)xmalloc(sizeof(dev_name_list_t));
			memset(nlist, 0, sizeof *nlist);

			/* Try for a title, failing that use the name. */
			vlist = find_attr_val(cf->alist, NAME_ATTR);
			nlist->name = vlist->val.string;

			vlist = find_attr_val(cf->alist, TITLE_ATTR);
			if ( vlist )
				nlist->title = vlist->val.string;
			else
				nlist->title = nlist->name;

			nlist->next = cp->nlist;
			cp->nlist = nlist;
		}
	}
}
