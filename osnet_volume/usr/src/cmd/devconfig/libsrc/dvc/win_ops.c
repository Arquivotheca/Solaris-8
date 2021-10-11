/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)win_ops.c 1.20 95/10/04 SMI"

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <libintl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#if !__ppc
#include <sys/kd.h>
#endif
#include <sys/sysmacros.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"

#if __ppc

#include <sys/kbio.h>
#include <sys/kbd.h>
#include <stropts.h>
#include <fcntl.h>
#include <unistd.h>

#define KIOCSLAYOUT     (KIOC|19)

#endif


/*
 * Table for translating between pcmapfiles file names,
 * OWconfig keyboard layout codes, and user-visible names
 * of keyboard types.
 */

extern char *xqa_path();

struct kbx {
	char *locale;
	char *mapdir;
	char *mapfile;
	char *code;
	char *type;
};

#define	KBXCNT	64
static int kbxcnt;
static struct kbx kbxtab[KBXCNT];


extern void move_attr(attr_list_t **, attr_list_t **, char *, ...);


/*
 * Get the keyboard translation table from the prototype.
 * This table converts between layout codes and keyboard types
 * (required by OWconfig), locales (displayed to the user)
 * and map files (requried by the keyboard default file).
 */
static void
make_kb_xtab(attr_list_t *typ)
{
	int i;
	char *type;
	char *layout;
	char *locale;
	char *mapdir;
	char *mapfile;

	if (typ == NULL ||
	    (type = find_attr_str(typ, "type")) == NULL ||
	    (layout = find_attr_str(typ, "__layout__")) == NULL ||
	    (locale = find_attr_str(typ, "__locale__")) == NULL ||
	    (mapdir = find_attr_str(typ, "__mapdir__")) == NULL ||
	    (mapfile = find_attr_str(typ, "__mapfile__")) == NULL)
		return;

	kbxcnt = count_string(layout);
	/*
	 * Build the keyboard layout translation table.
	 */
	for (i = 0; i < kbxcnt; i++) {
		if (kbxcnt >= KBXCNT) {
			ui_notice(MSG(EKBXCNT));
			break;
		}
		kbxtab[i].locale = next_string(&locale);
		kbxtab[i].code = next_string(&layout);
		kbxtab[i].mapdir = next_string(&mapdir);
		kbxtab[i].mapfile = next_string(&mapfile);
		kbxtab[i].type = next_string(&type);
	}
}

static char *
get_kb_mapdir(char *code)
{
	int i;

	for (i = 0; i < kbxcnt; i++)
		if (streq(kbxtab[i].code, code))
			return (kbxtab[i].mapdir);

	return (NULL);
}

static char *
get_kb_mapfile(char *code)
{
	int i;

	for (i = 0; i < kbxcnt; i++)
		if (streq(kbxtab[i].code, code))
			return (kbxtab[i].mapfile);

	return (NULL);
}

/*
 * Switch the layout and locale lists around.  This is because
 * we want to display the locale as the choice to the user,
 * but the real variable is layout which will be written out
 * to OWconfig.
 */
static void
cvt_kb_typ(attr_list_t *typ)
{
	attr_list_t *alist;
	attr_list_t *layout;
	attr_list_t *locale;

	for (alist = typ; alist; alist = alist->next) {
		if (streq(alist->name, "__layout__"))
			layout = alist;
		else if (streq(alist->name, "__locale__"))
			locale = alist;
	}
	if (layout && locale) {
		val_list_t *tmp = layout->vlist;

		layout->vlist = locale->vlist;
		locale->vlist = tmp;
	}
}

conf_list_t *
cvt_kb_typ_in(attr_list_t *typ)
{
	make_kb_xtab(typ);	/* make the translation table */
	cvt_kb_typ(typ);	/* switch the lists around */

	return (NULL);
}

void
cvt_kb_typ_out(attr_list_t *typ)
{
	cvt_kb_typ(typ);	/* switch the lists around */
}

#ifdef	XXX	/* Disable untill title update works. */
/*
 * Add the locale to the keyboard title.
 */
static void
cvt_kb_title_in(device_info_t *dp)
{
	char *layout;

	if (layout = find_attr_str(dp->dev_alist, "layout")) {
		char *title;

		title = dp->title;
		title = strcats(title, " - ", layout, NULL);
		xfree(dp->title);
		dp->title = title;
	}
}
#endif	XXX

/*
 * Convert the layout attribute to the form required by the user.
 */
static void
cvt_kb_layout_in(attr_list_t *alist)
{
	int i;
	val_list_t *layout;

	if ((layout = find_attr_val(alist, "layout")) == NULL)
		return;
	for (i = 0; i < kbxcnt; i++)
		if (streq(kbxtab[i].code, layout->val.string)) {
			xfree(layout->val.string);
			layout->val.string = xstrdup(kbxtab[i].locale);
		}
}

/*
 * Convert the layout attribute to the form required by OWconfig.
 */
static void
cvt_kb_layout_out(attr_list_t *alist)
{
	int i;
	val_list_t *layout;

	if ((layout = find_attr_val(alist, "layout")) == NULL)
		return;
	for (i = 0; i < kbxcnt; i++)
		if (streq(kbxtab[i].locale, layout->val.string)) {
			xfree(layout->val.string);
			layout->val.string = xstrdup(kbxtab[i].code);
		}
}

/*
 * Select the correct keyboard type as required by OWconfig.
 */
static void
cvt_kb_type_out(attr_list_t *alist)
{
	int i;
	char *layout;
	val_list_t *type;

	if ((layout = find_attr_str(alist, "layout")) == NULL ||
	    (type = find_attr_val(alist, "type")) == NULL)
		return;
	for (i = 0; i < kbxcnt; i++)
		if (streq(kbxtab[i].code, layout)) {
			xfree(type->val.string);
			type->val.string = xstrdup(kbxtab[i].type);
		}
}

/* We search for the lang attribute and its value which kdmconfig obtains
 * from the at.cfinfo file.  Before it writes it into the kd.conf file
 * we need to make sure that we set its value to be the layout that was
 * selected by the user.
 */
#if __ppc
static void
cvt_kb_lang_out(attr_list_t *alist)
{
        val_list_t *lang_val;
        val_list_t *layout_val;
        attr_list_t *lang_attr;

        if ((lang_val = find_attr_val(alist,"lang")) != NULL &&
             (layout_val = find_attr_val(alist,"layout")) != NULL)
            lang_val->val.integer = atoi(layout_val->val.string);
}
#endif

static void
map_kb_layout(char *kb)
{
#if	0
	char cmd[256];
	unsigned char mode;

	/*
	 * The following ioctl test is taken from pckmapkeys command.
	 * See the pcmapkeys code for more explanation.
	 */
	if (dvc_tmp_root || ioctl(0, KDGKBMODE, &mode) == -1 || mode == K_RAW) {
		ui_notice(MSG(EKBCONFIG));
		return;
	}

	sprintf(cmd, "/usr/bin/pcmapkeys -f %s", kb);
	if (system(cmd))
		ui_notice(MSG(EPCMAPKEYS));
#endif
}


static void
write_kb_def(attr_list_t *alist)
{
#if !__ppc
	FILE *fp;
	char *kb;
	char *layout;
	char *mapdir;
	char *mapfile;
	static char *kbpath = "/usr/share/lib/keyboards/";

	if ((fp = open_conf_file("/etc", "defaultkb", 1)) == NULL) {
		ui_notice(MSG(EDEFKBOPEN));
		return;
	}

	if ((layout = find_attr_str(alist, "layout")) == NULL) {
		ui_notice(MSG(ENOKBATTR));
		return;
	} else if ((mapdir = get_kb_mapdir(layout)) == NULL ||
		    (mapfile = get_kb_mapfile(layout)) == NULL) {
		ui_notice(MSG(ENOKBATTR));
		return;
	}

	kb = strcats(kbpath, mapdir, "/", mapfile, "\n", NULL);
	if (!fwrite(kb, 1, strlen(kb), fp)) {
		ui_notice(MSG(EDEFKBWRITE));
		xfree(kb);
		return;
	}
	fclose(fp);
	map_kb_layout(kb);
	xfree(kb);
#else
	struct strioctl strioctl;
        int fd;
        char *new_layout;
        int layout;
        int i;
        char *kb;
        static char *kbpath = "/usr/share/lib/keytables/type_101/";
        char ppc_layout[100];
        char cmd[256];

	/* Obtain the layout selected by the user and use the KIOCSLAYOUT
	 * ioctl to set the new layout.
       	 */
        if ((new_layout = find_attr_str(alist, "layout")) == NULL) {
                ui_notice(MSG(ENOKBATTR));
                return;
        }
        layout = atoi(new_layout);
        if ((fd = open("/dev/kbd", O_WRONLY)) == -1)
        	ui_notice(MSG(CANTOPEN_KBD));
        strioctl.ic_cmd = KIOCSLAYOUT;
        strioctl.ic_timout = 1;
        strioctl.ic_dp = (char *)&layout;
        strioctl.ic_len = sizeof(layout);
        if (ioctl(fd, I_STR, &strioctl) == -1)
            vrb(MSG(CANTKIOCS), errno);
	close(fd);

	/* Apparently the layout files have names like layout_01 for
	 * English and layout_23 for French. Since the layout returned
	 * by KIOCLAYOUT is a single decimal digit we need to do this hack
	 * to perform the conversion.
	 */
  
        if (layout == 1)
                sprintf(ppc_layout, "layout_0%d", layout);
        else
                sprintf(ppc_layout, "layout_%x", layout);

	/* loadkeys as opposed to pcmapkeys is run on PPC to map 
	 * the keyboard.
 	 */

        kb = strcats(kbpath, ppc_layout, "\n", NULL);
        sprintf(cmd, "/usr/bin/loadkeys %s", kb);
        if (system(cmd))
                ui_notice(MSG(ELOADKEYS));
#endif
}

void
cvt_kb_dev_in(device_info_t *dp)
{
	cvt_kb_layout_in(dp->dev_alist);
#ifdef	XXX	/* Disable untill title update works. */
	cvt_kb_title_in(dp);
#endif	XXX
}

device_info_t *
cvt_kb_dev_out(device_info_t *dp)
{
	cvt_kb_layout_out(dp->dev_alist);
	cvt_kb_type_out(dp->dev_alist);
#if __ppc
	cvt_kb_lang_out(dp->dev_alist);
#endif
	if (dp->modified)
		write_kb_def(dp->dev_alist);

	return (NULL);
}

/*
 * Table for translating between monitor size and dimensions.
 */

#define	DSXDCNT	16
#define DSXCNT_MUL 2			/* multipier for table size realloc   */
static int DSXCNT = 32;			/* starting table size		 */

typedef struct dsxdtab {
	char *name;
	int dimx;
	int dimy;
} dsxdtab_t;

typedef struct dsxtab {
	char *name;
	int dsxrcnt;
	int dsxscnt;
	dsxdtab_t dsxrtab[DSXDCNT];
	dsxdtab_t dsxstab[DSXDCNT];
} dsxtab_t;

static int dsxcnt;
static dsxtab_t *dsxtab = NULL;
static int dsx_badsize;
static int dsx_badres;
static int ds_badname;

/*
 * Make the monitor size conversion table.
 */
static int
make_ds_xstab(int idx, char *size, char *dimx, char *dimy)
{
	int i, j;
	int x, y;
	int dsxdcnt;
	dsxdtab_t *dsxdtab;

	dsxdtab = dsxtab[idx].dsxstab;
	if ((dsxdcnt = count_string(size)) >= DSXDCNT) {
		ui_notice(MSG(EDSXCNT));
		dsxdcnt = DSXDCNT;
	}

	for (i = j = 0; i < dsxdcnt; i++) {
		char *name;

		name = next_string(&size);
		x = atoi(next_string(&dimx));
		y = atoi(next_string(&dimy));
		if (x <= 0 || y <= 0)
			dsx_badsize++;
		else {
			dsxdtab[j].dimx = x;
			dsxdtab[j].dimy = y;
			dsxdtab[j].name = xstrdup(name);
			j++;
		}
	}

	return (dsxtab[idx].dsxscnt = j);
}

/*
 * Make the display resolution conversion table.
 */
static int
make_ds_xrtab(int idx, char *res)
{
	int i, j;
	int x, y;
	int dsxdcnt;
	dsxdtab_t *dsxdtab;

	dsxdtab = dsxtab[idx].dsxrtab;
	if ((dsxdcnt = count_string(res)) >= DSXDCNT) {
		ui_notice(MSG(EDSXCNT));
		dsxdcnt = DSXDCNT;
	}

	for (i = j = 0; i < dsxdcnt; i++) {
		char *name, *r;

		name = xstrdup(next_string(&res));
		if ((r = strchr(name, 'x')) == NULL) {
			xfree(name);
			continue;
		}
		*r++ = NULL;
		x = atoi(name);
		y = atoi(r);
		if (x <= 0 || y <= 0) {
			dsx_badres++;
			xfree(name);
			continue;
		} else {
			dsxdtab[j].dimx = x;
			dsxdtab[j].dimy = y;
			dsxdtab[j].name = name;
			*--r = 'x';
			j++;
		}
	}

	return (dsxtab[idx].dsxrcnt = j);
}

/*
 * Build the display translation tables from the device prototype.
 * These tables convert between monitor sizes displayed to the user
 * and dpi attributes required in the OWconfig file.
 */
static void
make_ds_xtab(attr_list_t *typ)
{
	char *device;
	char *size;
	char *dimx;
	char *dimy;
	char *res;
	dsxtab_t *new_dsxtab;

	if (dsxtab == NULL) {
		if ((dsxtab = (dsxtab_t *) malloc(sizeof(dsxtab_t) * DSXCNT)) ==
		    NULL) {
			ui_notice(MSG(EDSXCNT));
			return;
		}
	}

	if (dsxcnt >= (DSXCNT - 1)) {
		DSXCNT *= DSXCNT_MUL ;	/* double the table size	*/
		if ((new_dsxtab = (dsxtab_t *) realloc(dsxtab,
		    sizeof(dsxtab_t) * DSXCNT)) == NULL) {
			ui_notice(MSG(EDSXCNT));
			return;
		}
		dsxtab = new_dsxtab;
	}

	if (typ == NULL ||
	    (device = get_attr_str(typ, "device")) == NULL ||
	    (size = get_attr_str(typ, "size")) == NULL ||
	    (dimx = get_attr_str(typ, "__dimx__")) == NULL ||
	    (dimy = get_attr_str(typ, "__dimy__")) == NULL ||
	    (res = get_attr_str(typ, "res")) == NULL)
		return;

	if (make_ds_xstab(dsxcnt, size, dimx, dimy) &&
	    make_ds_xrtab(dsxcnt, res)) {
		char *group;

		if ((group = get_attr_str(typ, GROUP_ATTR)) != NULL)
			device = group;

		dsxtab[dsxcnt].name = xstrdup(device);
		dsxcnt++;
	}
}

static char *ds_type;
static attr_list_t *ds_typ;
static conf_list_t *ds_typ_list;
static char *isxin;

static void
add_ds_typ(char *pmi_base, char *title)
{
	conf_list_t *new;
	conf_list_t typ;
	val_list_t *val;
	char *pmi_fname;
	char typname[100];

	typ.alist = ds_typ;
	typ.next = NULL;

	val = find_attr_val(typ.alist, NAME_ATTR);
	new = dup_conf(&typ);


	/* Change the name to pmi filename base. */
	val = find_attr_val(new->alist, NAME_ATTR);
	strcpy(typname, val->val.string);
	xfree(val->val.string);
	val->val.string = get_pmi_name(pmi_base);

	/* Change the title to result of type/name lookup. */
	val = find_attr_val(new->alist, TITLE_ATTR);
	xfree(val->val.string);
	val->val.string = xstrdup(title);

	/* Change the pmifile attribute. */
	pmi_fname = make_pmi_fname(ds_type, pmi_base, isxin);
	val = find_attr_val(new->alist, "pmifile");
	xfree(val->val.string);
	val->val.string = xstrdup(pmi_fname);

	new->next = ds_typ_list;
	ds_typ_list = new;
}


static void
add_mn_typ(char *pmi_base, char *title)
{
	conf_list_t *new;
	conf_list_t typ;
	val_list_t *val;
	char *monitor_fname;

	typ.alist = ds_typ;
	typ.next = NULL;

	new = dup_conf(&typ);


	/* Change the name to pmi filename base. */
	val = find_attr_val(new->alist, NAME_ATTR);
	xfree(val->val.string);
	val->val.string = get_pmi_name(pmi_base);

	/* Change the title to result of type/name lookup. */
	val = find_attr_val(new->alist, TITLE_ATTR);
	xfree(val->val.string);
	val->val.string = xstrdup(title);

	/* Change the monitor attribute	*/
	val = find_attr_val(new->alist, "monitor");
	monitor_fname = make_vda_fname(val->val.string, pmi_base);
	xfree(val->val.string);
	val->val.string = monitor_fname;

	new->next = ds_typ_list;
	ds_typ_list = new;
}


/*
 * Given a display prototype, return a list of new display
 * prototypes (one for each PMI file).
 */
conf_list_t *
cvt_ds_typ_in(attr_list_t *typ)
{
	char *name;


	make_ds_xtab(typ);

	ds_typ = typ;
	ds_typ_list = NULL;

	isxin = find_attr_str(typ, XINSIDE_ATTR);
	name = find_attr_str(typ, NAME_ATTR);
	ds_type = find_attr_str(typ, "device");
	each_pmi_title(name, ds_type, add_ds_typ);

	return (ds_typ_list);
}

conf_list_t *
cvt_mn_typ_in(attr_list_t *typ)
{
	char *name;

	ds_typ = typ;
	ds_typ_list = NULL;

	name = find_attr_str(typ, NAME_ATTR);
	ds_type = find_attr_str(typ, "monitor");
	each_vda_title(name, name, add_mn_typ);

	return (ds_typ_list);
}


/*
 * Parse the device name. A device name might have the form
 * <NAME> | <NAME[INSTANCE]> | <NAME POSITION> | <NAME[INSTANCE] POSITION>
 * in which case the instance and the  position are picked up from the name.
 * if they exist.
 */
static void
parse_ds_name(char *id, char **name, int *instance, char **position)
{
	char *p, *q, *s;
	int gotname;

	gotname = 0;
	*name = 0;
	*instance = 0;
	*position = NULL;

	s = xstrdup(id);

	for (p = q = s; *q != NULL; q++) {
		if (*q == '[') {
			*q++ = NULL;
			*name = xstrdup(p);
			gotname++;
			p = q;
			while (*q != ']') {
				if (*q == NULL || isspace(*q)) {
					ds_badname++;
					return;
				}
				q++;
			}
			*q = NULL;
			*instance = atoi(p);
		} else if (isspace(*q)) {
			if (!gotname) {
				*q++ = NULL;
				*name = xstrdup(p);
			}
			while (isspace(*q))
				q++;
			*position = xstrdup(q);
		}
	}

	xfree(s);
}

/*
 * If the name denotes more than one instance of this device,
 * record the instance in an  INSTANCE_ATTR, record the position in
 * a "position" attribute, and convert the name to <NAME> only.
 * If the device does not have a position, then mark it as the main display.
 */
void
cvt_ds_name_in(conf_list_t *cf)
{
	int instance;
	char *id;
	char *name;
	char *position;
	val_list_t *Name;

	if ((Name = find_attr_val(cf->alist, NAME_ATTR)) == NULL ||
	    (id = Name->val.string) == NULL)
		return;

	parse_ds_name(id, &name, &instance, &position);

	if (name != NULL) {
		xfree(id);
		Name->val.string = name;
	}
	if (instance)
		make_attr(&cf->alist, INSTANCE_ATTR, VAL_NUMERIC, instance);
	if (position != NULL) {
		make_attr(&cf->alist, "position", VAL_STRING, position);
		make_attr(&cf->alist, "__ds__", VAL_STRING, "true");
		xfree(position);
	} else
		make_attr(&cf->alist, "position", VAL_STRING, "main");
}


static int
win_class_cnt(device_info_t *dp)
{
	int wcnt;
	char *w;
	char *wclass;
	device_info_t *d;

	if ((wclass = get_attr_str(dp->typ_alist, WCLASS_ATTR)) == NULL)
		return (0);

	for (wcnt = 0, d = dev_head; d; d = d->next)
		if ((d != dp) &&
		    ((w = get_attr_str(d->typ_alist, WCLASS_ATTR)) != NULL) &&
		    streq(wclass, w))
			wcnt++;

	return (wcnt);
}

/*
 * Remove the "position" attribute for the first display since it
 * does not need it.  This is to get around the fact that
 * add_missing_attributes() adds any missing prototype attribtues
 * to the device list.
 */
static void
cvt_ds_position_in(device_info_t *dp)
{
	int newds;
	char *position;


	/*
	 * See if this is the main display.  The main display is
	 * designated either by a "main" "postition" value or
	 * (when devices are read in from) or by the fact that
	 * it is the first device in its class to be created
	 * (when device are added by the user).
	 * We mark the existing devices (__ds__) since win_class_cnt()
	 * gives us a meaningful answer only for new devices (the device
	 * list is complete only after ALL devices have been read in
	 * and converted).
	 */
	if ((position = get_attr_str(dp->dev_alist, "position")) == NULL)
		return;
	newds = (get_attr_str(dp->dev_alist, "__ds__") == NULL);

	if (streq(position, "main") || (!win_class_cnt(dp) && newds)) {
		/*
		 * Dup the prototype before modifying it since the prototype
		 * does not belong only to this device.  The dev_alist however
		 * is private to this device so we don't need to dup it.
		 */
		dp->typ_alist = dup_alist(dp->typ_alist);
		remove_attr(&dp->typ_alist, "position");
		remove_attr(&dp->dev_alist, "position");
		if (!newds)
			remove_attr(&dp->dev_alist, "__ds__");
	}
}

static void
cvt_ds_title_in(device_info_t *dp)
{
	char *title;
#ifdef	XXX
	char *position;
#endif	XXX
	xfree(dp->title);
	title = make_title(dp->name, dp->unit, dp->typ_alist);
	dp->title = title;

#ifdef	XXX	/* title */
	/*
	 * Do not convert display title till we deal correctly with updating it.
	 */
	if ((position = find_attr_str(dp->dev_alist, "position")) != NULL) {
		title = strcats(title, " - ", position, " screen", NULL);
		xfree(dp->title);
		dp->title = title;
	}
#endif	XXX

}

void
cvt_ds_dev_in(device_info_t *dp)
{
	cvt_ds_position_in(dp);
	cvt_ds_title_in(dp);
}

/*
 * If there is more than one instance of this device,
 * convert the device's name to the form <NAME[INSTANCE] POSITION>.
 * and remove the "position" attribute since it should no be written
 * to the OWconfig file.
 */
static void
cvt_ds_name_out(device_info_t *dp)
{
	char *name;
	char *position;
	attr_list_t *alist;
	val_list_t *Name;

	alist = dp->dev_alist;

	if ((Name = find_attr_val(alist, NAME_ATTR)) == NULL ||
	    (name = Name->val.string) == NULL ||
	    (position = find_attr_str(alist, "position")) == NULL ||
	    win_class_cnt(dp) == 0)
		return;

	if (dp->unit) {
		char unit[8];

		sprintf(unit, "%d", dp->unit);
		Name->val.string =
			strcats(name, "[", unit, "] ", position, NULL);
	} else
		Name->val.string = strcats(name, " ", position, NULL);

	xfree(name);
	remove_attr(&dp->dev_alist, "position");
}

/*
 * See if the "pmifile" attributes specifies the correct pmi file.
 * For example, for a mach32 type (e.g. mach32_60.pmi), the lower
 * resolution is supported by "ati.pmi" (see comment in pmi.c).
 */
static void
cvt_ds_pmi_out(device_info_t *dp)
{
	int n;
	char *res;
	char *reslist;
	char *pmilist;
	char *group;
	char *members;
	char *pmifile;
	char *pminame;
	val_list_t *val;
	attr_list_t *dsp;
	attr_list_t *proto;
	attr_list_t *typ;

	dsp = dp->dev_alist;
	proto = dp->typ_alist;


	if (find_attr_str(proto, XINSIDE_ATTR) != NULL) {
		for (typ = dsp; typ; typ = typ->next)
			if (streq(typ->name, "pmifile")) {
				xfree(typ->name);
				typ->name = xstrdup("board");
			}
	}


	pmilist = find_attr_str(proto, "__pmimap__");
	if (!pmilist)
		return;
	res = find_attr_str(dsp, "res");
	reslist = find_attr_str(proto, "__resolution__");
	members = find_attr_str(proto, MEMBERS_ATTR);
	val = find_attr_val(dsp, "pmifile");

	if (!(res && reslist && members && val))
		return;

	pmifile = val->val.string;
	pminame = get_pmi_name(pmifile);
	group = get_typ_group(pminame);

	n = count_string(reslist);

	while (n--) {
		char *p;
		char *r;

		p = next_string(&pmilist);
		r = next_string(&reslist);

		if (!streq(r, res))
			continue;

		if (!streq(p, group)) {
			char *type;

			type = get_pmi_type(pmifile);
			xfree(val->val.string);
			val->val.string = make_pmi_fname(type, p, NULL);
			xfree(type);
		}
	}
}

static dsxdtab_t *
get_ds_xsentry(char *device, char *key)
{
	int i, j;
	dsxdtab_t *dsxe;

	for (dsxe = NULL, i = 0; i < dsxcnt; i++)
		if (streq(dsxtab[i].name, device)) {
			int dsxdcnt;
			dsxdtab_t *dsxdtab;

			dsxdcnt = dsxtab[i].dsxscnt;
			dsxdtab = dsxtab[i].dsxstab;

			for (j = 0; j < dsxdcnt; j++)
				if (streq(dsxdtab[j].name, key)) {
					dsxe = &dsxdtab[j];
					break;
				}
		}

	return (dsxe);
}

static dsxdtab_t *
get_ds_xrentry(char *device, char *key)
{
	int i, j;
	dsxdtab_t *dsxe;

	for (dsxe = NULL, i = 0; i < dsxcnt; i++)
		if (streq(dsxtab[i].name, device)) {
			int dsxdcnt;
			dsxdtab_t *dsxdtab;

			dsxdcnt = dsxtab[i].dsxrcnt;
			dsxdtab = dsxtab[i].dsxrtab;

			for (j = 0; j < dsxdcnt; j++)
				if (streq(dsxdtab[j].name, key)) {
					dsxe = &dsxdtab[j];
					break;
				}
		}

	return (dsxe);
}

/*
 * Set the dpix/dpiy attributes according to the resolution.
 */
static void
cvt_ds_dpi_out(device_info_t *dp)
{
	int dpix, dpiy;
	char s[8];
	char *res;
	char *size;
	char *device;
	char *group;
	val_list_t *Dpix, *Dpiy;
	attr_list_t *dev;
	dsxdtab_t *sz;
	dsxdtab_t *rs;

	dev = dp->dev_alist;

	if ((res = get_attr_str(dev, "res")) == NULL ||
	    (size = get_attr_str(dev, "size")) == NULL ||
	    (device = get_attr_str(dev, "device")) == NULL ||
	    (Dpix = get_attr_val(dev, "dpix")) == NULL ||
	    (Dpiy = get_attr_val(dev, "dpiy")) == NULL) {
		remove_attr(&dp->dev_alist, "size");
		return;
	}

	if ((group = get_attr_str(dp->typ_alist, GROUP_ATTR)) != NULL)
		device = group;

	if ((sz = get_ds_xsentry(device, size)) == NULL ||
	    (rs = get_ds_xrentry(device, res)) == NULL) {
		remove_attr(&dp->dev_alist, "size");
		return;
	}

	dpix = rs->dimx / sz->dimx;
	dpix = MIN(90, dpix);
	sprintf(s, "%d", dpix);
	xfree(Dpix->val.string);
	Dpix->val.string = xstrdup(s);
	dpiy = rs->dimy / sz->dimy;
	dpiy = MIN(90, dpiy);
	sprintf(s, "%d", dpiy);
	xfree(Dpiy->val.string);
	Dpiy->val.string = xstrdup(s);
}

static void
cvt_ds_device_xinside(device_info_t *dp, char *board, device_info_t *new)
{
	char  monitor_str_xqa[] = "Module";
	char *path;
	char *fullpath;
	char *ddxmodule;
	char *c;
	val_list_t *device_val;
	val_list_t *name_val;
	val_list_t *init_val;
	val_list_t *handle_val;

	path  = xqa_path();
	fullpath = strcats(path, "/", board, NULL);
	ddxmodule = extract_keyword(fullpath, monitor_str_xqa);
	c = strrchr(ddxmodule, '.');
	*c = '\0';
	name_val = find_attr_val(new->dev_alist, NAME_ATTR);
	device_val = find_attr_val(dp->dev_alist, "device");
	init_val = find_attr_val(new->dev_alist, "ddxInitFunc");
	handle_val = find_attr_val(new->dev_alist, "ddxHandler");
	xfree(name_val->val.string);
	xfree(device_val->val.string);
	xfree(init_val->val.string);
	xfree(handle_val->val.string);
	name_val->val.string = strcats("SUNW", ddxmodule, NULL);
	device_val->val.string = strcats("SUNW", ddxmodule, NULL);
	handle_val->val.string = strcats("ddxSUNW", ddxmodule, ".so.1", NULL);
	init_val->val.string = strcats("SUNW", ddxmodule, "Init", NULL);
}

static int
win_pci_class(device_info_t *dp)
{
	struct val_list *bus_type;
	struct attr_list* alist;


	if (match_attr(dp->typ_alist, "class", "win") ||
	    (find_attr_val(dp->typ_alist, "__wclass__"))) {

	    if (bus_type = find_attr_val(dp->dev_alist, "bustype")) {
		val_list_t *vlist;
		for (vlist = bus_type; vlist; vlist = vlist->next) {
		    if ((vlist->val_type == VAL_STRING) 
			&& (streq(vlist->val.string, "PCI")))
			    return (1);
		}

	    }
	}
	return (0);
}

static void
cvt_ds_device_krndrv(device_info_t *dp, device_info_t *new)
{

	device_info_t *new2 =  (device_info_t *)
				    xzmalloc(sizeof (device_info_t));
	char *common;
	val_list_t *dev_val;
	val_list_t *name_val;
	val_list_t *class_val;

	attr_list_t *attr;
	attr_list_t *attr1, *prevattr;

	new2->name = xstrdup(dp->name);
	new2->unit = dp->unit;
	new2->title = dp->title;
	new2->modified = TRUE;
	move_attr(&dp->dev_alist, &new2->dev_alist, DRVR_ATTR,
				"reg", "present", "unit", "bustype", NULL);

	new2->typ_alist = dup_attr(dp->typ_alist, CLASS_ATTR,
				DRVR_ATTR, COMMON_ATTR, DRIVER_ATTR, NULL);
	attr = dup_attr(dp->dev_alist, NAME_ATTR,
			    CLASS_ATTR, WCLASS_ATTR, NULL);
	attr1 = new2->dev_alist;
	for (prevattr = attr1; attr1; prevattr = attr1, attr1 = attr1->next)
		;
	/*
	 * insert dup'ed attrs to dev_alist
	 */
	prevattr->next = attr;
	/*
	 * tricky, remove dev entry so it does drvconfig is run on
	 *  the XSCREEN config entry
	 */
	remove_attr(&dp->typ_alist, DRVR_ATTR);
	/*
	 * change name to kernel driver name
	 * dangerous depeding on name
	 *
	 */
	/* remove_attr(&new2->typ_alist,WCLASS_ATTR); */
	if (win_pci_class(new2)) {
	    remove_attr(&new2->dev_alist,"reg");
	}
	dev_val = find_attr_val(new2->dev_alist, DRVR_ATTR);
	name_val = find_attr_val(new2->dev_alist, NAME_ATTR);
	class_val = find_attr_val(new2->dev_alist, CLASS_ATTR);
	xfree(class_val->val.string);
	xfree(name_val->val.string);
	xfree(name_val->val.string);
	class_val->val.string = xstrdup("sysbus");
	name_val->val.string = xstrdup(dev_val->val.string);
	new2->name = xstrdup(dev_val->val.string);
	new->next = new2;

}

/*
 * Given an XSCREENCONFIG class device, create an XSCREEN device of
 * the type specified by the device attribute if one does not exist.
 * This code has two portions.  The first is for third party
 * XSCREENCONFIG and the second is for normal XSCREENCONFIG devices
 *
 */
static device_info_t *
cvt_ds_device_out(device_info_t *dp)
{
	char  monitor_str_xqa[] = "Module";
	char *device;
	attr_list_t *typ;
	device_info_t *d;
	device_info_t *new;
	char *board;
	char *devname;

	if ((device = find_attr_str(dp->dev_alist, "device")) == NULL)
		return (NULL);


	for (d = dev_head; d; d = d->next)
		if (streq(d->name, device))
			break;

	if (d != NULL || (typ = find_typ_info(device)) == NULL)
		return (NULL);

	new = make_dev_node1(typ);
	/*
	 * checks for Xinside case board keyword present, if so
	 * change the name,device,ddxInit and ddxHandler names
	 *
	 */
	if ((board = find_attr_str(dp->dev_alist, "board")) != NULL) {
		cvt_ds_device_xinside(dp, board, new);

	}
	/*
	 * special case ; graphics card has a kernel driver
	 * copy existing node and modify so drvconfig is performed
	 * and .conf file is written.
	 *
	 */
	if ((devname = find_attr_str(dp->dev_alist, "devname")) != NULL) {

		cvt_ds_device_krndrv(dp, new);
	}


	return (new);

}


void
store_mn_dev(device_info_t *dev)
{
	device_info_t *ds;
	char *mn_name;
	char *cat_name;
	int i;
	val_list_t *val;


	mn_name = find_attr_str(dev->dev_alist, "monitor");
	for (ds = dev_head; ds; ds = ds->next) {
		if (find_attr_str(ds->dev_alist, "pmifile") != NULL) {
			break;
		}
	}

	val = find_attr_val(ds->dev_alist, "monitor");
	if (val != NULL)
		xfree(val->val.string);
	val->val.string = xstrdup(mn_name);

}

device_info_t *
cvt_ds_dev_out(device_info_t *dp)
{

	cvt_ds_name_out(dp);
	cvt_ds_pmi_out(dp);
	cvt_ds_dpi_out(dp);

	return (cvt_ds_device_out(dp));
}

/*
 * If the device support more than protocol, set it accordingly.
 */
static void
cvt_ms_strmod_out(attr_list_t *dev, attr_list_t *typ)
{
	int n;
	char *b;
	char *blist;
	char *buttons;
	char *s;
	char *slist;
	val_list_t *Strmod;

	/*
	 * Make sure we have the "buttons" and "strmod" specs
	 * in both the prototype and device list and that
	 * there is more that one possible streams module
	 * for this device.
	 */
	if ((blist = get_attr_str(typ, "buttons")) == NULL ||
	    (slist = get_attr_str(typ, "strmod")) == NULL ||
	    (n = count_string(slist)) <= 1 ||
	    (buttons = get_attr_str(dev, "buttons")) == NULL ||
	    (Strmod = get_attr_val(dev, "strmod")) == NULL ||
	    (Strmod->val.string) == NULL)
		return;

	/*
	 * Match the actual "buttons" value agains the buttons list
	 * and set the corresponding "strmod" value.
	 */
	while (n--) {
		if ((b = next_string(&blist)) == NULL ||
		    (s = next_string(&slist)) == NULL)
			break;
		else if (streq(b, buttons)) {
			xfree(Strmod->val.string);
			Strmod->val.string = xstrdup(s);
			break;
		}
	}
}

device_info_t *
cvt_ms_dev_out(device_info_t *dp)
{
	cvt_ms_strmod_out(dp->dev_alist, dp->typ_alist);

	return (NULL);
}
