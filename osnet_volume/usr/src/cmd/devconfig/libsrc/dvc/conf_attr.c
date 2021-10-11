/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)conf_attr.c 1.8 95/02/28 SMI"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <libintl.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "util.h"

int
is_xinside_attr(attr_list_t *alist)
{
	char *c;
	if ((c = find_attr_str(alist, XINSIDE_ATTR)) != NULL)
		return (TRUE);
	return (FALSE);
}

void
make_val(struct val_list **vlist, val_t t, ...)
{
	va_list			ap;
	struct val_list 	*vp = (struct val_list *)
				    xmalloc(sizeof (struct val_list));
	char *c;
	char *s;

	va_start(ap, t);

	switch (vp->val_type = t) {
	case VAL_NUMERIC:
		vp->val.integer = va_arg(ap, int);
		break;
	case VAL_UNUMERIC:
		vp->val.uinteger = va_arg(ap, unsigned int);
		break;
	case VAL_STRING:
		c = va_arg(ap, char *);
		if (c)
			s = dgettext(DVC_MSGS_TEXTDOMAIN, c);
		else
			s = MSG(BADNODE);
		vp->val.string = xstrdup(s);
		break;
	}

	vp->next = *vlist;
	*vlist = vp;

	va_end(ap);
}

void
free_val(val_list_t *vp)
{
	if (vp) {
		if (vp->next)
			free_val(vp->next);
		if (vp->val_type == VAL_STRING)
			xfree(vp->val.string);
		xfree(vp);
	}
}

int
find_val_name(char *name, val_list_t *list)
{
	while (list) {
		if (streq(name, list->val.string))
			return (TRUE);
		list = list->next;
	}
	return (FALSE);
}

static val_list_t *
dup_vlist(val_list_t *vlist)
{
	val_list_t *new;

	if (vlist == NULL)
		return (NULL);

	new = (val_list_t *)xzmalloc(sizeof (val_list_t));
	new->val_type = vlist->val_type;

	switch (new->val_type) {
	case VAL_STRING:
		new->val.string = xstrdup(vlist->val.string);
		break;
	case VAL_NUMERIC:
		new->val.integer = vlist->val.integer;
		break;
	case VAL_UNUMERIC:
		new->val.uinteger = vlist->val.uinteger;
		break;
	}

	if (vlist->next)
		new->next = dup_vlist(vlist->next);

	return (new);
}

void
make_attr(attr_list_t **alist, char *name, val_t t, ...)
{
	char *c;
	char *s;
	va_list	ap;
	val_list_t *vlist;
	attr_list_t *new;

	new = (attr_list_t *)xzmalloc(sizeof (attr_list_t));
	new->name = xstrdup(name);
	new->next = *alist;
	*alist = new;

	vlist = (val_list_t *)xzmalloc(sizeof (val_list_t));
	new->vlist = vlist;

	va_start(ap, t);

	switch (vlist->val_type = t) {
	case VAL_NUMERIC:
		vlist->val.integer = va_arg(ap, int);
		break;
	case VAL_UNUMERIC:
		vlist->val.uinteger = va_arg(ap, unsigned int);
		break;
	case VAL_STRING:
		c = va_arg(ap, char *);
		if (c)
			s = dgettext(DVC_MSGS_TEXTDOMAIN, c);
		else
			s = MSG(BADNODE);
		vlist->val.string = xstrdup(s);
		break;
	}

	va_end(ap);
}

void
free_attr(struct attr_list *at)
{
	if (at) {
		if (at->next)
			free_attr(at->next);
		free_val(at->vlist);
		xfree(at->name);
		xfree(at);
	}
}

attr_list_t *
del_attr(attr_list_t **alist, char *attrstr)
{

	attr_list_t *ap;
	attr_list_t *prvap;

	ap = *alist;

	for (prvap = ap; ap; prvap = ap, ap = ap->next)
		if (streq(ap->name, attrstr))
			break;

	if (ap) {
		if (prvap == ap)
			*alist = ap->next;
		else
			prvap->next = ap->next;
	}
	return (ap);
}

void
remove_attr(attr_list_t **alist, char *name)
{
	attr_list_t *ap;

	ap = del_attr(alist, name);
#ifdef	XXX	/* free_attr() */
	if (ap)
		free_attr(ap);
#endif	XXX
}

int
match_attr(attr_list_t *alist, char *name, char *value)
{
	while (alist) {
		if (streq(alist->name, name))
			return (streq(alist->vlist->val.string, value));
		alist = alist->next;
	}
	return (FALSE);		/* No name found! */
}

/*
 * Find the first attribute that matches by name.
 */
struct attr_list *
find_attr_list(struct conf_list *cf, char *name, char *value)
{
	while (cf) {
		if (match_attr(cf->alist, name, value))
			return (cf->alist);
		cf = cf->next;
	}
	return (NULL);
}

conf_list_t *
find_attr_listc(struct conf_list *cf, char *name, char *value)
{
	while (cf) {
		if (match_attr(cf->alist, name, value))
			return (cf);
		cf = cf->next;
	}
	return (NULL);
}



struct val_list *
find_attr_val(struct attr_list *alist, char *name)
{
	while (alist) {
		if (streq(name, alist->name))
			return (alist->vlist);
		alist = alist->next;
	}
	return (NULL);
}

attr_list_t *
find_attr(attr_list_t *alist, char *name)
{
	while (alist) {
		if (streq(name, alist->name))
			return (alist);
		alist = alist->next;
	}
	return (NULL);
}

char *
find_attr_str(struct attr_list *alist, char *name)
{
	struct val_list *vlist = find_attr_val(alist, name);
	if (vlist == NULL)
		return (NULL);

	assert(vlist->val_type == VAL_STRING);
	return (vlist->val.string);
}

val_list_t *
get_attr_val(attr_list_t *typ, char *name)
{
	val_list_t *val;

	if ((val = find_attr_val(typ, name)) == NULL)
		return (NULL);

	/*
	 * If a var, get its value.
	 */
	if (match(val->val.string, VAR_STRING)) {
		char *v_name;

		v_name = val->val.string + sizeof (VAR_STRING) - 1;

		if ((val = find_attr_val(typ, v_name)) == NULL) {
			no_def_notice(v_name);
			return (NULL);
		}
	}

	return (val);
}

char *
get_attr_str(attr_list_t *typ, char *name)
{
	val_list_t *val;

	if ((val = get_attr_val(typ, name)) == NULL)
		return (NULL);

	return (val->val.string);
}

attr_list_t *
dup_alist(attr_list_t *alist)
{
	attr_list_t* new = (attr_list_t*)xzmalloc(sizeof (attr_list_t));

	new->name = xstrdup(alist->name);
	new->vlist = dup_vlist(alist->vlist);

	if (alist->next)
		new->next = dup_alist(alist->next);

	return (new);
}

attr_list_t *
dup_attr(attr_list_t *alist, char *attrval, ...)
{

	attr_list_t *new = (attr_list_t *)NULL;
	attr_list_t *new2 = (attr_list_t *)NULL;
	attr_list_t *attr;
	char *attrstr;

	va_list ap;

	va_start(ap, attrval);

	for (attrstr = attrval; attrstr; attrstr = va_arg(ap, char *)) {
		if ((attr = find_attr(alist, attrstr)) == NULL)
			continue;

		new2 = (attr_list_t *)xzmalloc(sizeof (attr_list_t));

		new2->name = xstrdup(attr->name);

		new2->vlist = dup_vlist(attr->vlist);
		new2->next = new;
		new = new2;
	}

	va_end(fp);
	return (new);
}



void
move_attr(attr_list_t **from_alist, attr_list_t **to_alist, char *attrval, ...)
{

	char *attrstr;
	attr_list_t *rem_attr;

	va_list ap;
	va_start(ap, attrval);


	for (attrstr = attrval; attrstr; attrstr = va_arg(ap, char *)) {


		/*
		 * if attribute is not in the from list or on the
		 * to list then skip this attribute
		 */
		if (find_attr(*from_alist, attrstr) == NULL)
			continue;

		if (find_attr(*to_alist, attrstr) != NULL)
			continue;

		rem_attr = del_attr(from_alist, attrstr);
		rem_attr->next = *to_alist;
		*to_alist = rem_attr;
	}

	va_end(ap);

}
