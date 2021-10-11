/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)dev.c 1.11 95/02/28 SMI"

#include <assert.h>
#include <libintl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"
#include "win.h"

device_info_t *dev_head;

static char *
unit_str(int n)
{
	static char bfr[25];
	char *cp = "";

	switch (n) {
	case 0:			break;

	case 1:	cp = "first";	break;
	case 2:	cp = "second";	break;
	case 3:	cp = "third";	break;
	case 4:	cp = "fourth";	break;
	case 5:	cp = "fifth";	break;

	default:
		sprintf(bfr, "%dth", n);
		cp = bfr;
	}
	return (xstrdup(cp));
}

static int
get_unit(attr_list_t *alist)
{
	int unit;
	val_list_t *vlist;

	unit = 0;
	vlist = find_attr_val(alist, INSTANCE_ATTR);
	if (vlist && vlist->val.integer) {
		assert(vlist->val_type == VAL_NUMERIC);
		unit = vlist->val.integer;
	}

	return (unit);
}

static char *
get_title(char *name, attr_list_t *alist)
{
	val_list_t *vlist;
	vlist = find_attr_val(alist, TITLE_ATTR);

	if (vlist) {
		assert(vlist->val_type == VAL_STRING);
		return (vlist->val.string);
	}
	/*
	 * We found the named type info, but no title!.
	 */
	vrb(MSG(NOTITLE), name);
	return (NULL);
}

char *
make_title(char *name, int unit, attr_list_t *typ)
{
	char *title;

	if ((title = get_title(name, typ)) == NULL)
		return (NULL);

	if (unit) {
		char *unitstr;
		char *newtitle;

		unitstr = unit_str(unit + 1);
		newtitle = strcats(title, " - ", unitstr, MSG(UNIT), NULL);
		xfree(unitstr);
		return (newtitle);
	} else
		return (xstrdup(title));
}

/*
 * Look at each attribute for a device and check type info.
 * If the type info specifies "numeric," convert the string to
 * correct internal form.
 */
static void
cvt_dev_alist(attr_list_t *dev_alist, attr_list_t *typ_alist)
{
	int *vp;
	val_list_t *dat_val;
	val_list_t *typ_val;
	attr_list_t *dat;
	attr_list_t *master_typ;

	master_typ = find_typ_info("master");

	for (dat = dev_alist; dat; dat = dat->next) {

		/*
		 * Get the value and the type info for this attribute.
		 */
		dat_val = dat->vlist;
		if ((typ_val = find_attr_val(typ_alist, dat->name)) == NULL)
			typ_val = find_attr_val(master_typ, dat->name);
		if (typ_val == NULL || dat_val == NULL)
			continue;

		/*
		 * All attributes from the kernel dev_info tree are
		 * typeless, so are stored as a single string at first.
		 *
		 * When using "fake_data" there may be type info, so
		 * just skip the conversion process.
		 */
		if (dat_val->val_type != VAL_STRING || dat_val->next != NULL)
			continue;

		/*
		 * If the type information for this attribute specifies
		 * something else, we convert the string accordingly.
		 */
		vp = NULL;

		while (typ_val) {
			val_list_t *typ_val_x;

			/*
			 * If we ever see a string spec in the type info,
			 * we are done.  This is also true if we find a
			 * var ref that specifies a string.
			 */

			/*
			 * All type specifications must themselves
			 * be string objects.
			 */
			assert(typ_val->val_type == VAL_STRING);

			if (match(typ_val->val.string, VAR_STRING)) {
				char *v_name;

				v_name = typ_val->val.string +
					    sizeof (VAR_STRING) - 1;
				typ_val_x = find_attr_val(typ_alist, v_name);

				if (typ_val_x == NULL)
					typ_val_x = find_attr_val(master_typ,
							    v_name);
				if (typ_val_x == NULL) {
					no_def_notice(v_name);
					break;
				}
			} else
				typ_val_x = typ_val;

			if (!match(typ_val_x->val.string, NUMERIC_STRING))
				break;

			/*
			 * Create the prop value and inc the pointer.
			 */
			if (vp == NULL) {
				vp = (int *)dat_val->val.string;
				dat->vlist = NULL;
			}

			make_val(&(dat->vlist), VAL_NUMERIC, *vp);
			typ_val = typ_val->next;
			vp++;
		}
		/* XXX free dat_val, old value of dat->vlist. */
	}
}

/*
 * Add any of the attributes which are in the prototype list but
 * not in the device list.
 */
attr_list_t *
make_dev_alist(attr_list_t *dev_alist, attr_list_t *typ_alist)
{
	static char *re;
	attr_list_t *typ_ap;
	attr_list_t *dev_ap;

	cvt_dev_alist(dev_alist, typ_alist);

	if (re == NULL)
		re = regcmp(INTERNAL_ATTR, NULL);

	for (typ_ap = typ_alist; typ_ap; typ_ap = typ_ap->next) {
		/*
		 * Skip internal attributes.
		 */
		if (regex(re, typ_ap->name))
			continue;

		for (dev_ap = dev_alist; dev_ap; dev_ap = dev_ap->next)
			if (streq(typ_ap->name, dev_ap->name))
				break;

		if (dev_ap == NULL) {

			attr_list_t *nalist;
			/*
			 * Add the missing attribute.
			 */
			nalist = make_typ_attr(typ_ap, typ_alist);
			nalist->next = dev_alist;
			dev_alist = nalist;
		}
	}

	return (dev_alist);
}

void
fetch_dev_info(void)
{
	int unit;
	char *name;
	conf_list_t *devi;
	attr_list_t *dev;
	attr_list_t *typ;
	device_info_t *dp = NULL;

	ONCE();

	/*
	 * Get the prototype and configured device data.
	 */
	fetch_typ_info();
	fetch_devi_info();

	for (devi = devi_head; devi; devi = devi->next) {

		if ((name = find_attr_str(devi->alist, NAME_ATTR)) == NULL ||
		    (typ = find_typ_info(name)) == NULL ||
		    get_title(name, typ) == NULL)
			continue;

		dev = make_dev_alist(devi->alist, typ);
		unit = get_unit(dev);

		dp = (device_info_t *)xzmalloc(sizeof (*dp));
		dp->name = xstrdup(name);
		dp->unit = unit;
		dp->title = make_title(name, unit, typ);
		dp->typ_alist = typ;
		dp->dev_alist = dev;

		cvt_dev_in(dp);

		dp->next = dev_head;
		dev_head = dp;

	}
}

device_info_t *
get_dev_node(int idx)
{
	device_info_t *dp;
	int i = 0;

	for (dp = dev_head; dp; dp = dp->next)
		if (i++ == idx)
			return (dp);

	return (NULL);
}

attr_list_t *
get_typ_info(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->typ_alist);
}


attr_list_t *
get_dev_info(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->dev_alist);
}

char *
get_dev_name(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->name);
}

int
get_dev_unit(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->unit);
}

void *
get_ui_info(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->ui);
}

void
set_ui_info(void* ui, int idx)
{
	device_info_t *dp = get_dev_node(idx);
	dp->ui = ui;
}

static int
next_unit(char *name)
{
	device_info_t *dp;
	int unit = 0;

	for (dp = dev_head; dp; dp = dp->next) {
		if (streq(dp->name, name)) {
			if (dp->unit >= unit)
				unit = dp->unit + 1;
		}
	}
	return (unit);
}

/*
 * Return device titles one at a time and a single NULL after the last one.
 * The function then resets itself to be called again.
 */

char *
next_dev_title(void)
{
	device_info_t *dp;
	static int	idx = 0;

	fetch_dev_info();

	dp = get_dev_node(idx++);
	if (dp == NULL) {
		idx = 0;
		return (NULL);
	}

	return (dp->title);
}

char *
get_dev_title(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	return (dp->title);
}

char *
get_dev_cat(device_info_t *dp)
{
	return (find_attr_str(dp->typ_alist, CAT_ATTR));
}

void
set_dev_modified(int idx)
{
	device_info_t *dp = get_dev_node(idx);
	dp->modified = TRUE;
}

device_info_t *
make_dev_node1(attr_list_t *typ)
{
	device_info_t *dp;
	int unit;
	char *name;
	char *title;

	name = find_attr_str(typ, NAME_ATTR);
	unit = next_unit(find_attr_str(typ, NAME_ATTR));
	title = make_title(name, unit, typ);

	dp = (device_info_t *)xzmalloc(sizeof (device_info_t));

	dp->name = xstrdup(name);
	dp->unit = unit;
	dp->title = title ? title : xstrdup(name);
	dp->typ_alist = typ;
	dp->dev_alist = make_typ_alist(typ, typ);
	dp->modified = TRUE;
	cvt_dev_in(dp);

	return (dp);
}


device_info_t *
make_dev_node(int idx)
{
	char *name;
	attr_list_t *typ;

	name = get_cat_dev_name(idx);
	typ = find_typ_info(name);

	return (make_dev_node1(typ));
}

void
free_dev_node(device_info_t *dp)
{
	xfree(dp->name);
	xfree(dp->title);
	xfree(dp);
}

int
add_dev_node(device_info_t *dp)
{
	if (find_attr_val(dp->typ_alist, AUTO_ATTR)) {
		ui_notice(MSG(ADDSELFID));
		return (0);
	}

	dp->next = dev_head;
	dev_head = dp;

	return (1);
}

int
remove_dev_node(int idx)
{
	int i = 0;
	device_info_t *dp;
	device_info_t *dp_prev = NULL;

	if (find_attr_val(get_typ_info(idx), AUTO_ATTR)) {
		ui_notice(MSG(RMSELFID));
		return (0);
	}

	for (dp = dev_head; dp; dp = dp->next) {
		if (i++ == idx) {
			if (dp_prev)
				dp_prev->next = dp->next;
			else
				dev_head = dev_head->next;

			free_dev_node(dp);
			return (1);
		}
		dp_prev = dp;
	}

	return (0);
}

void
update_conf(void)
{
	conf_list_t *typ;
	device_info_t *dp;

	/*
	 * Do all external prototype processing before doing
	 * anything with the devices themselves.
	 */
	for (typ = typ_head; typ; typ = typ->next)
		cvt_typ_out(typ->alist);

	/*
	 * Do per device external processing.  If new devices are
	 * generated as a result, add those to the device list.
	 */
	for (dp = dev_head; dp; dp = dp->next) {
		device_info_t *dlist;

		if ((dlist = cvt_dev_out(dp)) != NULL) {
			device_info_t *dl, *last;

			for (dl = last = dlist; dl; last = dl, dl = dl->next)
				;
			last->next = dp->next;
			dp->next = dlist;
			dp = last;

		}
	}


	/*
	 * Write the driver conf files and load the appropriate drivers.
	 * Do this before updating the window configuration since the
	 * window system update affects device nodes.
	 */
	update_mod_conf();
	write_win_conf();
}

int
modified_conf(void)
{
	int i;
	device_info_t *dp;

	for (i = 0, dp = dev_head; dp; dp = dp->next)
		if (dp->modified)
			i++;

	return (i);
}
