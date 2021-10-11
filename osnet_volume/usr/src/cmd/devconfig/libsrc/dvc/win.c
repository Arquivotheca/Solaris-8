/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)win.c 1.10 94/04/18 SMI"

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
#include "win.h"

static attr_list_t *xds_dev;
static attr_list_t *xds_typ;

static int
fetch_xds_info(void) {


	if ((xds_typ = find_typ_info("XDISPLAY")) == NULL) {
		ui_notice(MSG(ENOXDPROTO));
		return (-1);
	}

	xds_dev = make_typ_alist(xds_typ,  xds_typ);
	return (0);
}

static void
make_xds_dev(attr_list_t *alist)
{
	xds_dev = make_dev_alist(alist, xds_typ);
}

static void
set_xds_attr(char *attr, char *val, int append)
{

	char *ov;
	char *nv;
	val_list_t *v;

	if (!streq(attr, "listOfScreens"))
		append = 0;

	if ((v = find_attr_val(xds_dev, attr)) == NULL ||
	    (ov = v->val.string) == NULL)
		return;
	
	if (append)
		nv = strcats(ov, ", ", val, NULL);
	else
		nv = xstrdup(val);

	xfree(ov);
	v->val.string = nv;
}

/*
 * See if we need to append to the XDISPLAY entry for this category.
 * This is to support the listOfScreens attribute in the XDISPLAY class.
 */
static int
append_xds_attr(char *cat)
{
	static int cnt = 0;
	static char *ctab[16];
	int i, found;

	for (found = i = 0; i < cnt; i++)
		if (streq(cat, ctab[i]))
			found++;

	if (cnt < 16)
		ctab[cnt++] = cat;

	return (found);
}

/*
 * Update the XDISPLAY class with current configuration data.
 */
static void
update_xds_conf(device_info_t *dp)
{
	static char *re;
	char *c;
	char *cat;
	char *name;
	attr_list_t *alist;

	if ((cat = find_attr_str(dp->typ_alist, CAT_ATTR)) == NULL ||
	    (name = find_attr_str(dp->dev_alist, NAME_ATTR)) == NULL)
		return;
	/*
	 *
	 * Lookup the attribute in the XDISPLAY prototype that
	 * has the corresponding value (pointer/keyboard/display)
	 * and change the value to the name of the device.
	 * For example,
	 *
	 * 	lookup(attr="pointer")
	 * 	set(attr="LOGI-S")
	 *
	 * Note that we don't have to care about the name of the
	 * attribute.
	 */
	for (alist = xds_typ; alist; alist = alist->next) {
		char *s;

		/*
		 * Skip internal attributes.
		 */
		if (re == NULL)
			re = regcmp(INTERNAL_ATTR, NULL);
		if (regex(re, alist->name))
			continue;

		s = alist->vlist->val.string;
		c = next_string(&s);

		if (streq(cat, c)) {
			int a = append_xds_attr(cat);
			set_xds_attr(alist->name, name, a);
		}
	}
}

static int
valid_xds_class(char *class)
{
	int n;
	char *classes;

	if ((classes = get_attr_str(xds_typ, "__xdclass__")) == NULL ||
	    (n = count_string(classes)) <= 0) {
		ui_notice(MSG(EBADXDPROTO));
		return (0);
	}

	while (n--) {
		char *c;

		c = next_string(&classes);
		if (streq(c, class))
			return (1);
	}

	ui_notice(MSG(EBADWCLASS));
	return (0);
}

static void
write_xds_conf(FILE *fp)
{
	val_list_t *Name;
	conf_list_t cf;

	if ((Name = find_attr_val(xds_dev, NAME_ATTR)) != NULL) {
		/*
		 * We support only one XDISPLAY for now. XXX
		 */
		xfree(Name->val.string);
		Name->val.string = xstrdup("0");
	}
	memset(&cf, 0, sizeof (cf));
	cf.alist = xds_dev;
	write_conf(fp, &cf);
}

char*
win_home_path()
{
	static char*	openwinhome;

	if (!openwinhome && !(openwinhome = getenv("OPENWINHOME")))
		openwinhome = "/usr/openwin";

	return (openwinhome);
}

/*
 * Return an attribute list with driver specific attributes removed.
 */
static attr_list_t *
get_win_alist(attr_list_t *base, val_list_t *drv)
{
	attr_list_t *alist;
	attr_list_t *ap;
	attr_list_t *last = NULL;

	alist = dup_alist(base);
	for (ap = alist; ap; ap = ap->next) {
		if (find_val_name(ap->name, drv)) {
			if (last)
				last->next = ap->next;
			else
				alist = ap->next;
		} else
			last = ap;
	}

	return (alist);
}

static FILE *
open_win_conf(int wr)
{
	static char *openwinlocal = "/etc/openwin/server/etc";
	static char *owc = "OWconfig";

#define	DBG_WIN
#ifdef	DBG_WIN
	if (getenv("OWLOCAL") != NULL)
		openwinlocal = getenv("OWLOCAL");
#endif	DBG_WIN
	return (open_conf_file(openwinlocal, owc, wr));
}

static int
cvt_win_in(conf_list_t *cf)
{
	extern void cvt_ds_name_in(conf_list_t *cf);
	char *class;
	attr_list_t *alist;

	alist = cf->alist;

	/*
	 * Make sure we have both a class and a name
	 * for this device.  Note that we check
	 * CLASS_ATTR here since we are reading the
	 * entries from the config file.  When
	 * the device is added to the configured
	 * device list (device_info_t *) the missing
	 * attributes including WCLASS_ATTR will
	 * be added.
	 */
	if (find_attr_str(alist, NAME_ATTR) == NULL ||
	    (class = find_attr_str(alist, CLASS_ATTR)) == NULL ||
	    !valid_xds_class(class))
		return (-1);

	/*
	 * Add all devices at this point.  We will prune the
	 * configured device list once we have performed
	 * per prototype/device processing.
	 */
	if (streq(class, "XDISPLAY")) {
		make_xds_dev(alist);
		return (0);
	} else if (streq(class, "XSCREENCONFIG"))
		cvt_ds_name_in(cf);

	return (1);
}

void
read_win_conf(void)
{
	int rv;
	FILE *fp;
	conf_list_t *dev;
	conf_list_t *nxtdev = NULL;

	if (fetch_xds_info() == -1)
		return;
	else if ((fp = open_win_conf(0)) == NULL) {
#ifdef	XXX
		ui_notice(MSG(ENOWCFR));
#endif	XXX
		fclose(fp);
		return;
	}
	
	for (dev = read_conf(fp); dev; dev = nxtdev) {
		nxtdev = dev->next;
		dev->next = NULL;
		if ((rv = cvt_win_in(dev)) == -1)
			break;
		else if (rv > 0)
			add_devi_node(dev);
		/* XXX free */
	}

	fclose(fp);
}

int
valid_win_conf(void)
{
	int found;
	char *c, *s;
	val_list_t *cats;
	device_info_t *dp;

	if ((cats = find_attr_val(xds_typ, "__xdcategory__")) == NULL) {
		ui_notice(MSG(EBADXDPROTO));
		return (0);
	}

	s = cats->val.string;
	/*
	 * Make sure we have at least one confiured device in each of
	 * the categories in the mandatory category list.
	 */
	while (c = next_string(&s)) {
		for (found = 0, dp = dev_head; dp; dp = dp->next) {
			char* cat = find_attr_str(dp->typ_alist, CAT_ATTR);

			if (streq(cat, c))
				found++;
		} 
		if (!found)
			return (0);
	}

	return (1);	/* we have configured devices in all categoires */
}

static void
cvt_win_out(device_info_t *dp)
{
	char *class;
	char *name;
	val_list_t *drv;
	val_list_t *Class;
	val_list_t *Name;
	val_list_t *wclass;
	val_list_t *wname;
	attr_list_t *dev;
	attr_list_t *typ;

	dev = dp->dev_alist;
	typ = dp->typ_alist;

	if (find_attr_str(dev, NAME_ATTR) == NULL ||
	    (class = find_attr_str(dev, CLASS_ATTR)) == NULL)
		return;

	/*
	 * If the device has driver-only attributes, remove them.
	 */
	if ((drv = find_attr_val(typ, DRIVER_ATTR)) != NULL) {
		dp->dev_alist = get_win_alist(dev, drv);

		/*
		 * Make sure "class" will be written out first.
		 * Since attributes are written in LRU order, we need
		 * to make sure that "class" is the oldest attribute.
		 * This can be the case for device coming from the
		 * devinfo tree, e.g. a bus mouse.
		 */
		for (dev = dp->dev_alist; dev; dev = dev->next) {
			if (streq(dev->name, CLASS_ATTR) &&
			    (dev->next != NULL)) {
				attr_list_t *a = NULL;

				remove_attr(&dp->dev_alist, CLASS_ATTR);
				make_attr(&a, CLASS_ATTR, VAL_STRING, class);
				for (dev = dp->dev_alist; dev; dev = dev->next)
					if (dev->next == NULL) {
						dev->next = a;
						break;
					}
				break;
			}
		}

		dev = dp->dev_alist;
	}

	/*
	 * wclass attribute overrides the class.
	 */
	if ((Class = find_attr_val(dev, CLASS_ATTR)) != NULL &&
	    (class = Class->val.string) != NULL &&
	    (wclass = find_attr_val(typ, WCLASS_ATTR)) != NULL &&
	    (wclass->val.string != NULL)) {
		xfree(class);
		Class->val.string = xstrdup(wclass->val.string);
	}

	/*
	 * wname attribute overrides the name.
	 */
	if ((Name = find_attr_val(dev, NAME_ATTR)) != NULL &&
	    (name = Name->val.string) != NULL &&
	    (wname = find_attr_val(typ, WNAME_ATTR)) != NULL &&
	    (wname->val.string != NULL)) {
		xfree(name);
		Name->val.string = xstrdup(wname->val.string);
	}

}

void
write_win_conf(void)
{
	FILE *fp;
	device_info_t *dp;
	conf_list_t cf;

	if (!valid_win_conf())
		return;

	if ((fp = open_win_conf(1)) == NULL) {
		ui_notice(MSG(ENOWCFW));
		return;
	}

	memset(&cf, 0, sizeof (cf));

	for (dp = dev_head; dp; dp = dp->next) {
		char *wclass;

		wclass = find_attr_str(dp->typ_alist, WCLASS_ATTR);
		if (wclass == NULL)
			continue;

		cvt_win_out(dp);

		update_xds_conf(dp);

		cf.alist = dp->dev_alist;
		write_conf(fp, &cf);
		fflush(fp);
	}

	write_xds_conf(fp);
	fflush(fp);

	fclose(fp);
}
