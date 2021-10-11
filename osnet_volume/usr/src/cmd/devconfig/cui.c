/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)cui.c 1.50 99/03/12 SMI"

#include <assert.h>
#include <stdio.h>
#include <locale.h>
#include <libintl.h>

#include "cui.h"
#include "dvc.h"
#include "devcfg_msgs.h"

#ifdef __STDC__
#define	TAG(m)		(DEVCFG_##m)
#else
#define	TAG(m)		(DEVCFG_/**/m)
#endif

#define	MSG(i)	dgettext(DEVCFG_MSGS_TEXTDOMAIN, devcfg_msgs[TAG(i)-DEVCFG_MSGS_BASE])

#define NUM_EX_WIDTH	4	/* Number of columns in numeric exclusives. */
#define TEXT_EX_WIDTH	2	/* Number of columns in text exclusives. */
#define MAX_EXCLUSIVE	15	/* Max number of items in exclusives. */
#define BOXED		2

#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define MIN(a, b)	((a) < (b) ? (a) : (b))

extern bool CUI_doRefresh;

static int		device_idx;
static const int	hspace = 2;
static int		new_device;
static CUI_Widget	toplevel;
static CUI_Widget	overview;
static CUI_Widget	summary_list;
static int		realized;
static int		list_size;
static int		litem_size;

int update_done;
int notice_up;

#define	X_NULL		0
#define	X_MAIN		1
#define	X_SHOW		2
#define	X_SEL_CAT	3
#define	X_SEL_DEV	4
#define	X_MODIFY	5
#define	X_DONE		6
#define	X_HELP		7
#define	X_CONT_INST	8
#define	X_CANCEL	9
#define	X_ADD		10
#define	X_SHOW_ATTR	11
#define	X_UNCONF	12
#define	X_APPLY		13
#define	X_RESET		14
#define X_CANCEL_CHG	15
#define X_OVERVIEW	16

static CUI_Widget help_msg[] = {
	NULL,
	"Devconfig Main help",
	"Devconfig show",
	"Devconfig select category",
	"Devconfig select device",
	"Devconfig modify window",
	"Devconfig Done",
	"Devconfig Help...",
	"Devconfig Continue install...",
	"Devconfig Cancel",
	"Devconfig Add...",
	"Devconfig Show attributes",
	"Devconfig Unconfigure",
	"Devconfig Apply",
	"Devconfig Reset",
	"Devconfig Cancel changes",
	"Devconfig Overview"
};
static CUI_Widget footer_msg[] = {
	NULL,
	"Main help",
	"Show",
	"Select category",
	"Select device",
	"modify",
	"Done",
	"Help...",
	"Continue install...",
	"Cancel",
	"Add...",
	"Show attributes",
	"Unconfigure",
	"Apply",
	"Reset",
	"Cancel changes",
	"Overview"
};

typedef struct {
	char *name;
	int idx;
	CUI_CallbackProc proc;
	void * data;
	int col;
	int row;
	int just;
} ButtonSpec;

static
CUI_Widget
footer_key(int idx)
{
	return CUI_STR_RESOURCE(gettext(footer_msg[idx]));
}

static
CUI_Widget
help_key(int idx)
{
	return CUI_STR_RESOURCE(gettext(help_msg[idx]));
}

 
static char*
normalize(char* str, int line_size)
{
	int	len;
	char*	new;

	if ( str == NULL )
		return NULL;

	new = (char*)xmalloc(line_size+1);
	*(new+line_size) = '\0';
	memset(new, ' ', line_size);
	len = strlen(str);
	memcpy(new, str, MIN(len, line_size));

	return new;
}

static char*
adjust(char* str, int line_size)
{
	char*	new;
	int	size;

	if (str == NULL)
		return NULL;

	size = MIN(strlen(str), line_size);
	new = (char*)xmalloc(size+1);
	memcpy(new, str, size);
	new[size] = '\0';

	return new;
}


static int
is_it_set(CUI_Widget w)
{
	CUI_Resource	res[] = {
		setId,		(void*) 0,
		nullStringId,	(void*) 0
		};

	CUI_getValues(w, res);

	/* Only the low byte will have real data. */
	return (int)res[0].value & 1;
}


static char*
unique_name()
{
#define UNIQUE		"unique-"
#define NDIGITS		5

	static char	bfr[sizeof(UNIQUE) + NDIGITS];
	static int	cnt = 0;

	sprintf(bfr, "%s%.*d", UNIQUE, NDIGITS, cnt++);
	return bfr;
}


/*ARGSUSED*/
static int
confirm_done(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_popdown(CUI_parent(w));
	/* FIX FIX FIX FIX FIX FIX */
	/* Should delete the notice widget. */
	return 0;
}

static void
ui_confirm(char *text, CUI_CallbackProc proc)
{
	CUI_Widget	noticeBox;
	int		label;
	int		width;
	int		height;
	static void	make_button();

	width = CUI_screenCols() / 3;
	height = strlen(text) / width;

	label = CUI_compileString(text);
	noticeBox = CUI_vaCreateWidget(unique_name(), CUI_NOTICE_ID, NULL,
		widthId,		(void*) width,
		heightId,		(void*) height,
		colId,			(void*) -1,
		rowId,			(void*) -1,
		hPadId,			(void*) 2,
		vPadId,			(void*) 1,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) 1,
		stringId,		(void*) label,
		nullStringId);

	make_button(noticeBox, gettext("No"), X_NULL, confirm_done, NULL, 0,0,centerId);
	make_button(noticeBox, gettext("Yes"), X_NULL, proc, 1, 1,0,centerId);

	CUI_popup(noticeBox, CUI_GRAB_EXCLUSIVE);
}

/*ARGSUSED*/
static int
done(CUI_Widget w, void* client_data, void* call_data)
{
	int force = (int)client_data;

	/*
	 * If we don't have the required OpenWindows devices
	 * ask the user if he wants to configure them.
	 */
	if (!force && !valid_win_conf()) {
		ui_confirm(MSG(NOWINDOWS), done);
		return 0;
	}

	CUI_popdown(CUI_parent(w));

	if (!update_done) {
		notice_up = 0;
		update_conf();
		update_done++;
		if (notice_up) {
			notice_up = 0;
			return 0;
		}
	}

	CUI_exit(toplevel);
	exit(0);
	return 0;
}


/*ARGSUSED*/
static int
done_no_save(CUI_Widget w, void* client_data, void* call_data)
{
	int force = (int)client_data;

	/* Put up a dialog asking for confirmation. */
	if (!force && modified_conf()) {
		ui_confirm(MSG(CANCELMSG), done_no_save);
		return 0;
	}

	CUI_popdown(CUI_parent(w));
	CUI_exit(toplevel);
	exit(0);
	return 0;
}


/*ARGSUSED*/
static int
help_summary(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_showHelp(help_msg[X_MAIN]);
	return 0;
}

/*ARGSUSED*/
static int
help_show(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_showHelp(help_msg[X_SHOW]);
	return 0;
}

/*ARGSUSED*/
static int
help_attribute(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_showHelp(help_msg[X_MODIFY]);
	return 0;
}


/*ARGSUSED*/
static int
help_select_a_device(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_showHelp(help_msg[X_SEL_DEV]);
	return 0;
}


/*ARGSUSED*/
static int
help_select_a_category(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_showHelp(help_msg[X_SEL_CAT]);
	return 0;
}

void
ui_notice(char* text)
{
	CUI_Widget	noticeBox;

	if (!realized)
		CUI_realizeWidget(toplevel);

	noticeBox = CUI_vaCreateWidget(unique_name(), CUI_NOTICE_ID, NULL,
		colId,			(void*) -1,
		rowId,			(void*) -1,
		hPadId,			(void*) 2,
		vPadId,			(void*) 1,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) 1,
		widthId,		CUI_RESOURCE(60),
		stringId,		CUI_STR_RESOURCE(text),
		nullStringId);

	CUI_popup(noticeBox, CUI_GRAB_EXCLUSIVE);
	notice_up++;
}


void
ui_error_exit(char* text)
{
	CUI_Widget	button;
	CUI_Widget	noticeBox;
	int		label;
	int		width;
	int		height;

	width = CUI_screenCols() / 3;
	height = strlen(text) / width;

	if ( ! realized )
		CUI_realizeWidget(toplevel);

	label = CUI_compileString(text);
	noticeBox = CUI_vaCreateWidget(unique_name(), CUI_NOTICE_ID, NULL,
		widthId,		(void*) width,
		heightId,		(void*) height,
		colId,			(void*) -1,
		rowId,			(void*) -1,
		hPadId,			(void*) 2,
		vPadId,			(void*) 1,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) 1,
		stringId,		(void*) label,
		nullStringId);

	label = CUI_compileString( MSG(OKMSG) );
	button = CUI_vaCreateWidget(unique_name(),CUI_OBLONGBUTTON_ID,noticeBox,
		labelId,		(void*) label,
		nullStringId);
	CUI_addCallback(button, CUI_SELECT_CALLBACK, done_no_save, (void*)1);

	CUI_popup(noticeBox, CUI_GRAB_EXCLUSIVE);

	CUI_mainLoop();
}


static int
control_label(char* text)
{
	return CUI_compileString(expand_abbr(text));
}

/*
 * Names are useless when we have CUI_Widget handles, so I just assign
 * any unique name to most of the widgets.
 */

static char*
item_name(char* base, int n)
{
	static int	bfr_size;
	static char*	bfr;
	int		size;

	size = strlen(base) + NDIGITS + 1;
	if ( bfr_size < size )
		bfr = (char*)xrealloc((void*)bfr, bfr_size = size);
	sprintf(bfr, "%s%.*d", base, NDIGITS, n);
	return bfr;
}

static void
make_button(CUI_Widget area, char* text, int msg_idx,
	    CUI_CallbackProc proc, void* data, int col, int row, int just)
{
	CUI_Widget	button;
	int		label;

	label = CUI_compileString(gettext(text));

	button = CUI_vaCreateWidget(unique_name(),
		CUI_OBLONGBUTTON_ID,
		area,
		labelId,		(void*) label,
		colId,			(void*) col,
		rowId,			(void*) row,
		adjustId,		(void*) just,
		nullStringId);

	if (msg_idx)
		CUI_vaSetValues(button,
			footerMessageId, footer_key(msg_idx),
			nullStringId);

	CUI_addCallback(button, CUI_SELECT_CALLBACK, proc, data);
}


static void
make_buttons( CUI_Widget area, int sWidth, ButtonSpec spec[], int num_buttons)
{
	int x, y, n;
	int len = 4*num_buttons+4; /* Account for parens plus space on each side. */

	/* find if the length is bigger than the area */
	for (x=0; x<num_buttons; x++) {
		if (!spec[x].name) return;
		len += strlen( spec[x].name );
	}

	/* fill in the rest of the struct. */
	if (len >= sWidth ) {
		/* Split this row, and left justify it */
		for (x=0,n=0; n<num_buttons; x++ )
			for(y=0; y<num_buttons/2; y++) {
				spec[n].col = y;
				spec[n].row = x;
				spec[n].just = leftId;
				n++;
			}
	}
	else {
		/* do not split this row. */
		for(x=0; x<num_buttons; x++) {
			spec[x].col = x;
			spec[x].row = 0;
			spec[x].just = centerId;
		}
	}

	/* make each button, from the spec */
	for(x=0; x< num_buttons; x++) 
		make_button( area, spec[x].name, spec[x].idx, 
		             spec[x].proc, spec[x].data, spec[x].col, 
		             spec[x].row,  spec[x].just);
}


static void
make_separator(CUI_Widget parent, int* row)
{
	CUI_vaCreateWidget(unique_name(), CUI_SEPARATOR_ID, parent,
		rowId,			(void*) *row,
		nullStringId);
	++(*row);
}

static void
make_screen(CUI_Widget parent, CUI_Widget* main_area, CUI_Widget* button_area)
{
	int		row = 0;

	*main_area = CUI_vaCreateWidget(unique_name(), CUI_CONTROL_AREA_ID, parent,
		rowId,			(void*) row,
		colId,			(void*) 0,
		vPadId,			(void*) 1,
		vSpaceId,		(void*) 0,
		hPadId,			(void*) 1,
		nullStringId);

	row = CUI_screenRows() - 8;
	make_separator(parent, &row);

	*button_area = CUI_vaCreateWidget(unique_name(), CUI_CONTROL_AREA_ID, parent,
		rowId,			(void*) row,
		colId,			(void*) 0,
		hPadId,			(void*) 1,
		vPadId,			(void*) 0,
		vSpaceId,		(void*) 0,
		nullStringId);

	row ++;
	row ++;
	CUI_vaCreateWidget(unique_name(), CUI_FOOTER_ID, parent,
	rowId, (void *)row,
 	nullStringId);

}

static void
empty_list(CUI_Widget list)
{
	int		nitems = CUI_countListItems(list);
	int		i;

	for ( i=0; i<nitems; i++ ) {
		CUI_Widget item = CUI_lookupItem(list, 0);
		if ( CUI_deleteItem(list, 0) == 0 )
			CUI_deleteWidgetByHandle(item);
	}
}

static void
rebuild_list(CUI_Widget list, char* (*get_item)(void))
{
	CUI_Widget	item;
	char*		item_string;
	int		i = 0;

	CUI_updateDisplay(FALSE);
	empty_list(list);
	while ( (item_string = normalize(get_item(), litem_size)) != NULL ) {
		item = CUI_vaCreateWidget(unique_name(), CUI_LITEM_ID, list,
			labelId, (void*) CUI_compileString(item_string),
			nullStringId);
		CUI_addItem(list, item, i++);
		xfree(item_string);
	}
	CUI_refreshItems(summary_list);
	CUI_updateDisplay(TRUE);
}

static int
populate_list(CUI_Widget list, char* (*get_item)(void))
{
	char*		item_string;
	int		cnt = 0;

	while ( (item_string = normalize(get_item(), litem_size)) != NULL ) {
		CUI_vaCreateWidget(unique_name(), CUI_LITEM_ID, list,
			labelId, (void*) CUI_compileString(item_string),
			nullStringId);
		xfree(item_string);
		++cnt;
	}

	return cnt;
}


/*
 * Build a unique control name by combining the device name,
 * unit number, attribute name and value number.
 */

static char*
cntl_name(char* name, int unit, attr_list_t* alist, int vn)
{
	char		bfr1[30];
	char		bfr2[30];

	sprintf(bfr1, "%d", unit);
	sprintf(bfr2, "%d", vn);

	return strcats("name:",		name,
		       "/unit:",	bfr1,
		       "/attribute:",	alist->name,
		       "/value:",	bfr2,
		       NULL);
}

static val_store_t*
get_a_val_ref(attr_list_t* alist, char* name, int idx)
{
	int		i = 0;
	val_list_t*	vlist;

	for ( vlist=find_attr_val(alist, name); vlist; vlist=vlist->next )
		if ( i++ == idx )
			return &(vlist->val);
	return NULL;
}


static int
get_a_val(val_store_t* val, attr_list_t* alist, char* name, int idx)
{
	val_store_t* vp = get_a_val_ref(alist, name, idx);
	if ( vp ) {
		*val = *vp;
		return TRUE;
	}
	return FALSE;
}


static void
set_numeric_exclusives(char*		cn,
		       char*		var_name,
		       val_list_t*	var_typ,
		       int		nchoices,
		       val_store_t	var_val)
{
	int		i;
	int		label;

	char* typ_str = var_typ->val.string;

	for ( i=0; i<nchoices; ++i ) {
		CUI_Widget	box;
		char		numeric_string[30];
		int		n;
		int		usint;

		next_numeric(&typ_str, &n,&usint);
		print_num(numeric_string, n,0);
		label = CUI_compileString(numeric_string);

		box = CUI_lookupWidget(item_name(cn, i));
		CUI_vaSetValues(box,
			labelId,		(void*) label,
			setId,			(void*) (n==var_val.integer)
							?trueId:falseId,
			nullStringId);
	}
}


static void
set_text_field(char*		cn,
	       char*		var_name,
	       val_list_t*	var_typ,
	       val_store_t	var_val)
{
	CUI_Widget	w;
	char		bfr[30];

	w = CUI_lookupWidget(cn);
	print_num(bfr, var_val.integer,0);
	CUI_vaSetValues(w,
		stringId,		(void*) CUI_compileString(bfr),
		nullStringId);
}


static void
set_string_exclusives(char*		cn,
		      char*		var_name,
		      val_list_t*	var_typ,
		      int		nchoices,
		      val_store_t	var_val)
{
	CUI_Widget	check;
	int		i;
	int		label;

	char* typ_str = var_typ->val.string;

	for ( i=0; i<nchoices; ++i ) {
		char* cp = next_string(&typ_str);
		label = CUI_compileString(cp);

		check = CUI_lookupWidget(item_name(cn, i));

		CUI_vaSetValues(check,
			labelId,		(void*) label,
			setId,			(void*) streq(var_val.string, cp)
							?trueId:falseId,
			nullStringId);
		xfree(cp);
	}
}


static void
set_string_list(char*		cn,
		char*		var_name,
		val_list_t*	var_typ,
		int		nchoices,
		val_store_t	var_val)
{
	/* FIX FIX FIX FIX FIX FIX FIX FIX */
}


static void
set_control(char* cn, char* var_name, val_list_t* var_typ, val_store_t var_val)
{
	int		nchoices;

	assert(var_typ->val_type == VAL_STRING);

	if ( match(var_typ->val.string, NUMERIC_STRING) ) {
		nchoices = count_numeric(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			set_numeric_exclusives(cn, var_name, var_typ,
				nchoices, var_val);
		else
			set_text_field(cn, var_name, var_typ,
				var_val);
	} else if ( match(var_typ->val.string, STRING_STRING) ) {
		nchoices = count_string(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			set_string_exclusives(cn, var_name, var_typ,
				nchoices, var_val);
		else
			set_string_list(cn, var_name, var_typ,
				nchoices, var_val);
	}
}

/*
 * Rewind the display after a new device has been added.  Popdown
 * the attribute popup (if any), the list of the devices in the
 * category and the category list.
 */
#define MAX_POPUP	10
static CUI_Widget popup_stack[MAX_POPUP];
static int popup_idx;

static void
push_popup(CUI_Widget popup)
{
	if (popup_idx >= MAX_POPUP) {
		ui_notice(gettext("display stack overflow"));
		return;
	}
	popup_stack[popup_idx++] = popup;
}

static void
rewind_display()
{
	CUI_updateDisplay(FALSE);
	while (popup_idx > 0)
		CUI_popdown(popup_stack[--popup_idx]);
	popup_idx = 0;
	CUI_updateDisplay(TRUE);
}

/*ARGSUSED*/
static int
reset(CUI_Widget w, void* client_data, void* call_data)
{
	attr_list_t*	typ;
	attr_list_t*	alist;
	char*		var_name;
	val_list_t*	var_typ;
	val_store_t	var_val;

	typ = get_typ_info(device_idx);

	/* For each attribute in the type information: */
	for ( alist=typ; alist; alist=alist->next ) {
		val_list_t*	vlist;
		int		vn = 0;

		/* For each value of the type spec: */
		for ( vlist=alist->vlist,vn=0; vlist; vlist=vlist->next, ++vn ){
			char*		cn;

			/* All type information must be strings. */
			assert(vlist->val_type == VAL_STRING);

			/* If type info does not specify a variable, skip. */
			if ( ! match(vlist->val.string, VAR_STRING) )
				continue;

			/* Find the attribute name that defines the real data.*/
			var_name = vlist->val.string + sizeof(VAR_STRING) - 1;
			var_typ = find_attr_val(typ, var_name);

			/* Get the current value for this device. */
			get_a_val(&var_val, get_dev_info(device_idx),
					alist->name, vn);
				/* XXX No current value if FALSE! */

			/* build a name for the control. */
			cn = cntl_name(get_dev_name(device_idx),
				       get_dev_unit(device_idx),
				       alist, vn);

			/* Reset appropriate control for this attribute. */
			set_control(cn, var_name, var_typ, var_val);

			xfree(cn);
		}
	}

	return 0;
}


static int
apply_numeric_exclusives(char*		cn,
			 char*		var_name,
			 val_list_t*	var_typ,
			 int		nchoices,
			 val_store_t*	var_val)
{
	int		i;

	char* typ_str = var_typ->val.string;

	for ( i=0; i<nchoices; ++i ) {
		CUI_Widget	box;
		int		n;
		int		usint;

		next_numeric(&typ_str, &n, &usint);
		box = CUI_lookupWidget(item_name(cn, i));

		if ( is_it_set(box) ) {
			if ( var_val->integer != n ) {
				var_val->integer = n;
				return TRUE;
			}
			return FALSE;
		}
	}
	return FALSE;
}


static int
typecheck_text_field(char*		cn,
		     char*		var_name,
		     val_list_t*	var_typ,
		     val_store_t*	var_val)
{
	char*		bfr;
	int		len;
	int		n;
	char*		str;
	CUI_Widget	w;

	w = CUI_lookupWidget(cn);
	str = CUI_textFieldGetString(w, &len);

	n = (int)strtol(str, NULL, 0);

	if ( chk_num(var_typ->val.string, n) )
		return TRUE;

	bfr = strcats(gettext("Incorrect value for "),
		      expand_abbr(var_name),
		      " ",
		      str,
		      NULL);
	ui_notice(bfr);
	xfree(bfr);
		
	return FALSE;
}


static int
apply_text_field(char*		cn,
		 char*		var_name,
		 val_list_t*	var_typ,
		 val_store_t*	var_val)
{
	int		len;
	int		n;
	char*		str;
	CUI_Widget	w;

	w = CUI_lookupWidget(cn);
	str = CUI_textFieldGetString(w, &len);

	n = (int)strtol(str, NULL, 0);
	if ( var_val->integer != n ) {
		var_val->integer = n;
		return TRUE;
	}
	return FALSE;
}


static int
apply_string_exclusives(char*		cn,
			char*		var_name,
			val_list_t*	var_typ,
			int		nchoices,
			val_store_t*	var_val)
{
	CUI_Widget	check;
	int		i;

	char* typ_str = var_typ->val.string;

	for ( i=0; i<nchoices; ++i ) {
		char* cp = next_string(&typ_str);

		check = CUI_lookupWidget(item_name(cn, i));

		if ( is_it_set(check) ) {
			if ( strcmp(var_val->string, cp) ) {
				var_val->string = cp;
				return TRUE;
			}
			return FALSE;
		}
		xfree(cp);
	}
	return FALSE;
}


static int
apply_string_list(char*		cn,
		  char*		var_name,
		  val_list_t*	var_typ,
		  int		nchoices,
		  val_store_t*	var_val)
{
	/* FIX FIX FIX FIX FIX FIX FIX FIX */
	return FALSE;
}


static int
typecheck_control(char*		cn,
		  char*		var_name,
		  val_list_t*	var_typ,
		  val_store_t*	var_val)
{
	int		nchoices;

	assert(var_typ->val_type == VAL_STRING);

	if ( match(var_typ->val.string, NUMERIC_STRING) ) {
		nchoices = count_numeric(var_typ->val.string);
		if ( nchoices >= MAX_EXCLUSIVE )
			/* Text field is the only control worth typechecking. */
			return typecheck_text_field(cn, var_name, var_typ, var_val);
	}
	return TRUE;
}


static int
apply_control(char*		cn,
	      char*		var_name,
	      val_list_t*	var_typ,
	      val_store_t*	var_val)
{
	int		nchoices;
	int		mods = 0;

	assert(var_typ->val_type == VAL_STRING);

	if ( match(var_typ->val.string, NUMERIC_STRING) ) {
		nchoices = count_numeric(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			mods += apply_numeric_exclusives(cn, var_name, var_typ,
				nchoices, var_val);
		else
			mods += apply_text_field(cn, var_name, var_typ,
				var_val);
	} else if ( match(var_typ->val.string, STRING_STRING) ) {
		nchoices = count_string(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			mods += apply_string_exclusives(cn, var_name, var_typ,
				nchoices, var_val);
		else
			mods += apply_string_list(cn, var_name, var_typ,
				nchoices, var_val);
	}

	return mods;
}


/*ARGSUSED*/
static int
apply(CUI_Widget w, void* client_data, void* call_data)
{
	attr_list_t*	typ;
	attr_list_t*	alist;
	char*		var_name;
	val_list_t*	var_typ;
	val_store_t*	var_val;

	typ = get_typ_info(device_idx);

	/* For each attribute in the type information: */
	for ( alist=typ; alist; alist=alist->next ) {
		val_list_t*	vlist;
		int		vn = 0;

		/* For each value of the type spec: */
		for ( vlist=alist->vlist, vn=0; vlist; vlist=vlist->next, ++vn ) {
			char*		cn;

			/* All type information must be strings. */
			assert(vlist->val_type == VAL_STRING);

			/* If type info does not specify a variable, skip. */
			if ( ! match(vlist->val.string, VAR_STRING) )
				continue;

			/* Find the attribute name that defines the real data. */
			var_name = vlist->val.string + sizeof(VAR_STRING) - 1;
			var_typ = find_attr_val(typ, var_name);

			/* Get the current value for this device. */
			var_val = get_a_val_ref(get_dev_info(device_idx), alist->name, vn);

			/* build a name for the control. */
			cn = cntl_name(get_dev_name(device_idx),
				       get_dev_unit(device_idx),
				       alist, vn);

			if ( !typecheck_control(cn, var_name, var_typ, var_val))
				return 0;

			/* Set attribute from control value. */
			if ( apply_control(cn, var_name, var_typ, var_val) )
				set_dev_modified(device_idx);

			xfree(cn);
		}
	}

	rewind_display();
	rebuild_list(summary_list, next_dev_title);

	return 0;
}


/*ARGSUSED*/
static int
cancel(CUI_Widget w, void* client_data, void* call_data)
{
	reset(w, client_data, call_data);
	apply(w, client_data, call_data);

	if ( new_device ) {
		CUI_Widget popup;
		if ( (popup = (CUI_Widget)get_ui_info(device_idx)) != NULL )
			CUI_deleteWidgetByHandle(popup);
		(void)remove_dev_node(0);
	}

	rewind_display();
	rebuild_list(summary_list, next_dev_title);

	return 0;
}


static CUI_Widget
make_exclusives(char* title, CUI_Widget parent, int row, int width)
{
	CUI_Widget	set;
	CUI_Widget	setLabel;

	int		label;

	label = control_label(title);
	setLabel = CUI_vaCreateWidget(unique_name(), CUI_CAPTION_ID, parent,
		colId,			(void*) 1,
		rowId,			(void*) row,
		labelId,		(void*) label,
		nullStringId);

	set = CUI_vaCreateWidget(unique_name(), CUI_EXCLUSIVES_ID, setLabel,
		layoutTypeId,		(void*) fixedColsId,
		measureId,		(void*) width,
		borderWidthId,		(void*) BOXED,
		helpId,			help_key(X_MODIFY),
		footerMessageId,	footer_key(X_MODIFY),
		nullStringId);

	return set;
}


static int
exclusives_height(int nchoices, int width)
{
	int		nrows;

	nrows = (nchoices + width - 1) / width;
	/*
	 * Each row takes up two lines, plus one line each for top
	 * and bottom boarders.
	 */
	return nrows * 2 + 2;
}


static void
make_numeric_exclusives(char*		cn,
			CUI_Widget	parent,
			char*		var_name,
			val_list_t*	var_typ,
			int*		row,
			int		nchoices,
			val_store_t	var_val)
{
	int		i;
	int		label;

	CUI_Widget set = make_exclusives(var_name, parent, *row, NUM_EX_WIDTH);
	char* typ_str = var_typ->val.string;

	*row += exclusives_height(nchoices, NUM_EX_WIDTH);

	for ( i=0; i<nchoices; ++i ) {
		char		numeric_string[30];
		int		n;
		int		usint;

		next_numeric(&typ_str, &n, &usint);
		print_num(numeric_string, n,0);
		label = CUI_compileString(numeric_string);

		CUI_vaCreateWidget(item_name(cn, i), CUI_CHECKBOX_ID, set,
			labelId,		(void*) label,
			setId,			(void*) (n==var_val.integer)
							?trueId:falseId,
			nullStringId);
	}
}


static void
make_string_exclusives(char*		cn,
		       CUI_Widget	parent,
		       char*		var_name,
		       val_list_t*	var_typ,
		       int*		row,
		       int		nchoices,
		       val_store_t	var_val)
{
	int		i;
	int		label;

	CUI_Widget set = make_exclusives(var_name, parent, *row, TEXT_EX_WIDTH);
	char* typ_str = var_typ->val.string;

	*row += exclusives_height(nchoices, TEXT_EX_WIDTH);

	for ( i=0; i<nchoices; ++i ) {
		char* cp = next_string(&typ_str);
		label = CUI_compileString(cp);
		CUI_vaCreateWidget(item_name(cn, i), CUI_CHECKBOX_ID, set,
			labelId,		(void*) label,
			setId,			(void*) streq(var_val.string, cp)
							?trueId:falseId,
			nullStringId);
		xfree(cp);
	}
}

/* FIX FIX FIX FIX FIX FIX FIX FIX */
/* Make one of the items selected. */
static void
make_string_list(char*		cn,
		 CUI_Widget	parent,
		 char*		var_name,
		 val_list_t*	var_typ,
		 int*		row,
		 int		nchoices,
		 val_store_t	var_val)
{
	CUI_Widget	caption;
	CUI_Widget	list;

	int		i;
	int		label;

	char* typ_str = var_typ->val.string;

	label = control_label(var_name);
	caption = CUI_vaCreateWidget(unique_name(), CUI_CAPTION_ID, parent,
		colId,			(void*) 1,
		rowId,			(void*) *row,
		labelId,		(void*) label,
		nullStringId);

	list = CUI_vaCreateWidget(unique_name(), CUI_LIST_ID, caption,
		/* This should extend to the end of the window: */
		heightId,		(void*) MIN(nchoices, 10),
		toggleId,		(void *) trueId,
		nullStringId);
	++(*row);

	for ( i=0; i<nchoices; ++i ) {
		char *cp, *cp2;

		cp = next_string(&typ_str);
		cp2 = normalize(cp, 30);
		xfree(cp);
		label = CUI_compileString(cp2);
		xfree(cp2);

		CUI_vaCreateWidget(item_name(cn, i), CUI_LITEM_ID, list,
			labelId, (void*) label,
			nullStringId);
	}
}


static void
make_text_field(char*		cn,
		CUI_Widget	parent,
		char*		var_name,
		val_list_t*	var_typ,
		int*		row,
		val_store_t	var_val)
{
	CUI_Widget	caption;

	int		label;
	char		bfr[30];

	print_num(bfr, var_val.integer,0);

	label = control_label(var_name);
	caption = CUI_vaCreateWidget(unique_name(), CUI_CAPTION_ID, parent,
		colId,			(void*) 1,
		rowId,			(void*) *row,
		labelId,		(void*) label,
		nullStringId);

	CUI_vaCreateWidget(cn, CUI_TEXTFIELD_ID, caption,
		widthId,		(void*) 20,
		stringId,		(void*) CUI_compileString(bfr),
		nullStringId);
	++(*row);
}


static void
make_control(char*		cn,
	     CUI_Widget		parent,
	     char*		var_name,
	     val_list_t*	var_typ,
	     val_store_t	var_val,
	     int*		row)
{
	int		nchoices;

	if ( match(var_typ->val.string, NUMERIC_STRING) ) {
		nchoices = count_numeric(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			make_numeric_exclusives(cn, parent, var_name, var_typ,
				row, nchoices, var_val);
		else
			make_text_field(cn, parent, var_name, var_typ,
				row, var_val);
	} else if ( match(var_typ->val.string, STRING_STRING) ) {
		nchoices = count_string(var_typ->val.string);
		if ( nchoices < MAX_EXCLUSIVE )
			make_string_exclusives(cn, parent, var_name, var_typ,
				row, nchoices, var_val);
		else
			make_string_list(cn, parent, var_name, var_typ,
				row, nchoices, var_val);
	}
}

/*ARGSUSED*/
static int
unconfigure(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_Widget	popup;

	CUI_popdown(CUI_parent(w));

	device_idx = CUI_getCurrentItem(client_data);
	popup = (CUI_Widget)get_ui_info(device_idx);

	if (!remove_dev_node(device_idx))
		return (0);

	if (popup != NULL)
		CUI_deleteWidgetByHandle(popup);

	rebuild_list(client_data, next_dev_title);
	return 0;
}


static ButtonSpec attr_bspec[] = {
{ "dummy",	X_APPLY,		apply, 			NULL },
{ "dummy",	X_RESET,		reset, 			NULL },
{ "dummy",	X_CANCEL_CHG,		cancel, 		NULL },
{ "dummy",	X_HELP, 		help_attribute, 	NULL },
};

static CUI_Widget
make_attribute_popup(int* nattr)
{
	attr_list_t*	alist;
	CUI_Widget	main_area;
	CUI_Widget	button_area;
	char*		cp;
	int		label;
	int		ncontrols = 0;
	CUI_Widget	popup;
	int		row = 0;
	char*		title;
	attr_list_t*	typ;
	char*		var_name;
	val_list_t*	var_typ;
	val_store_t	var_val;

	if ( (popup = (CUI_Widget)get_ui_info(device_idx)) != NULL )
		return popup;

	title = get_dev_title(device_idx);
	cp = strcats(title, gettext(" attributes"), NULL);
	title = adjust(cp, litem_size);
	xfree(cp);
	label = CUI_compileString(title);
	xfree(title);

	popup = CUI_vaCreateWidget(unique_name(), CUI_POPWIN_ID, NULL,
		widthId,		(void*) CUI_screenCols(),
		heightId,		(void*) CUI_screenRows(),
		hPadId,			(void*) 2,
		vPadId,			(void*) 0,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) -1,
		titleId,		(void*) label,
		alignCaptionsId,	(void*) trueId,
		nullStringId);

	make_screen(popup, &main_area, &button_area);

	CUI_vaSetValues(main_area, defaultId, trueId, NULL);
	CUI_vaSetValues(button_area, defaultId, falseId, NULL);

	attr_bspec[0].name = gettext("Apply");
	attr_bspec[1].name = gettext("Reset");
	attr_bspec[2].name = gettext("Cancel");
	attr_bspec[3].name = gettext("Help...");
	make_buttons( button_area, CUI_screenCols(), attr_bspec, 4 );

	typ = get_typ_info(device_idx);

	/* For each attribute in the type information: */
	for ( alist=typ; alist; alist=alist->next ) {
		val_list_t*	vlist;
		int		vn = 0;

		/* For each value of the type spec: */
		for ( vlist=alist->vlist, vn=0; vlist; vlist=vlist->next, ++vn ) {
			char*		cn;

			/* All type information must be strings. */
			assert(vlist->val_type == VAL_STRING);

			/* If type info does not specify a variable, skip. */
			if ( ! match(vlist->val.string, VAR_STRING) )
				continue;

			/* Find the attribute name that defines the real data. */
			var_name = vlist->val.string + sizeof(VAR_STRING) - 1;
			var_typ = find_attr_val(typ, var_name);

			/* Get the current value for this device. */
			get_a_val(&var_val, get_dev_info(device_idx), alist->name, vn);

			/* build a name for the control. */
			cn = cntl_name(get_dev_name(device_idx),
				       get_dev_unit(device_idx),
				       alist, vn);

			/* Make an appropriate control for this attribute. */

			if ( (var_typ != NULL) && (var_typ->val_type == VAL_STRING) ) {
				make_control(cn, main_area, var_name,
					     var_typ, var_val, &row);
				++ncontrols;
			}

			xfree(cn);
		}
	}

	if ( ncontrols == 0 ) {	/* FIX FIX FIX FIX FIX FIX FIX  */
#ifdef	XXX
		ui_notice(gettext("This device has no user configurable attributes."));
#endif	XXX
		*nattr = 0;
		return NULL;	/* Should free allocated memory! */
	}

	set_ui_info((void*)popup, device_idx);

	return popup;
}

/*ARGSUSED*/
static int
show_attributes_done(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_popdown((CUI_Widget)client_data);
	return 0;
}

static CUI_Widget
make_show_popup()
{
	CUI_Widget	popup;
	CUI_Widget	main_area;
	CUI_Widget	button_area;
	CUI_Widget	attr_list;
	attr_list_t*	master_typ;
	attr_list_t*	alist;
	val_list_t*	vlist;
	val_list_t*	var_typ;
	char*		var_name;
	char*		cp;
	char*		item;
	char*		title;
	int		label;

#ifdef	XXX	/* add another field to device_info */
	if ( (popup = (CUI_Widget)get_ui_info(device_idx)) != NULL )
		return popup;
#endif	XXX

	label = CUI_compileString(gettext("Device Configuration"));

	popup = CUI_vaCreateWidget(unique_name(), CUI_POPWIN_ID, NULL,
		widthId,		(void*) CUI_screenCols(),
		heightId,		(void*) CUI_screenRows(),
		hPadId,			(void*) 2,
		vPadId,			(void*) 0,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) -1,
		titleId,		(void*) label,
		alignCaptionsId,	(void*) trueId,
		nullStringId);

	make_screen(popup, &main_area, &button_area);

	CUI_vaSetValues(main_area, defaultId, falseId, NULL);
	CUI_vaSetValues(button_area, defaultId, trueId, NULL);

	title = get_dev_title(device_idx);
	cp = strcats(title, gettext(" attributes"), NULL);
	title = adjust(cp, litem_size);
	xfree(cp);
	label = CUI_compileString(title);
	xfree(title);

	attr_list = CUI_vaCreateWidget(unique_name(), CUI_LIST_ID, main_area,
		vPadId,			(void*) 2,
		widthId,		(void*) (CUI_screenCols() - 9),
		toggleId,		(void *) trueId,
		titleId,		(void*) label,
		helpId,			help_key(X_SHOW),
		footerMessageId,	footer_key(X_SHOW),
		nullStringId);

	master_typ = find_typ_info("master");

	for (alist = get_dev_info(device_idx); alist; alist = alist->next) {
		if (streq(alist->name,"name"))
			continue;
		var_typ = find_attr_val(master_typ, alist->name);
		if (!var_typ)
			continue;

		for(vlist = alist->vlist; vlist; vlist = vlist->next) {
			int len;
			int display_len;
			char *name;
			char num[32];

			switch (vlist->val_type) {
			case VAL_NUMERIC:
				print_num(num, vlist->val.integer,0);
				item = num;
				break;
			case VAL_UNUMERIC:
				print_num(num, vlist->val.uinteger,1);
			case VAL_STRING:
				item = vlist->val.string;
				break;
			default:
				ui_error_exit("bad variable");
			}
			var_name = var_typ->val.string;
			if (match(var_name, VAR_STRING))
				var_name += sizeof(VAR_STRING) - 1;
			else
				var_name = alist->name;
			
			var_name = expand_abbr(var_name);
			name = NULL;
			len = strlen(var_name);
			display_len = litem_size / 2;
			if (len >= display_len)
				var_name[display_len] = NULL;
			else {
				name = xmalloc(display_len + 1);
				memset(name, ' ', display_len - len);
				strcpy(&name[display_len-len], var_name);
				var_name = name;
			}
			cp = strcats(var_name, " ", item, NULL);
			if (name)
				xfree(name);
			item = normalize(cp, litem_size);
			xfree(cp);

			CUI_vaCreateWidget(unique_name(), CUI_LITEM_ID,
				attr_list,
				labelId,	CUI_STR_RESOURCE(item),
				adjustId,	(void*) centerId,
				nullStringId);
			xfree(item);
			var_typ = var_typ->next;
		}
	}

	make_button(button_area, gettext("Done"), X_DONE, show_attributes_done, popup,0,0,centerId);
	make_button(button_area, gettext("Help..."), X_HELP, help_show, NULL, 1,0,centerId);

#ifdef	XXX
	set_ui_info((void*)popup, device_idx);
#endif	XXX

	return popup;
}



/*ARGSUSED*/
static int
show_attributes(CUI_Widget w, void* client_data, void* call_data)
{
	int		nattr;
	CUI_Widget	popup;

	CUI_popdown(CUI_parent(w));

	nattr = 0;
	device_idx = CUI_getCurrentItem(client_data);

	if (find_attr_val(get_typ_info(device_idx), AUTO_ATTR))
		popup = make_show_popup();
	else
		popup = make_attribute_popup(&nattr);

	if (popup) {
		new_device = FALSE;
		CUI_popup(popup, CUI_GRAB_EXCLUSIVE);
		push_popup(popup);
		CUI_doRefresh = FALSE;
	} else if (!nattr)
		ui_notice(gettext("This device has no user configurable attributes."));

	return 0;
}


/*ARGSUSED*/
static int
select_a_device_done(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_popdown((CUI_Widget)client_data);
	return 0;
}

/*ARGSUSED*/
static int
select_a_category_done(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_popdown((CUI_Widget)client_data);
	rebuild_list(summary_list, next_dev_title);
	return 0;
}

/*ARGSUSED*/
static int
add_new_device(CUI_Widget w, void* client_data, void* call_data)
{
	int		idx;
	int		nattr;
	CUI_Widget	popup;
	device_info_t*	dp;

	/* Get the index to the device title selection. */
	idx = CUI_getCurrentItem(client_data);

	/* Add a device instance to the top of the list. */
	dp = make_dev_node(idx);
	if (!add_dev_node(dp)) {
		free_dev_node(dp);
		return (0);
	}

	/*
	 * Make the popup.  If the device has configurable attributes
	 * display them to the user.  Otherwise add it automatically.
	 * Remove the device instance in case of error.
	 */
	nattr = 0;
	device_idx = 0;
	popup = make_attribute_popup(&nattr);
	if ( popup ) {
		new_device = TRUE;
		CUI_popup(popup, CUI_GRAB_EXCLUSIVE);
		push_popup(popup);
		CUI_doRefresh = FALSE;
	} else if (!nattr) {
		new_device = TRUE;
		rewind_display();
		rebuild_list(summary_list, next_dev_title);
	} else {
		new_device = FALSE;
		(void)remove_dev_node(device_idx);
	}

	return 0;
}

/*ARGSUSED*/
static int
select_a_device(CUI_Widget w, void* client_data, void* call_data)
{
	CUI_Widget		popup;
	int			idx;

	/* Get the selected item number. */
	idx = CUI_getCurrentItem(client_data);

	popup = (void*)get_cat_ui_info(idx);

	if ( popup == NULL ) {
		CUI_Widget	main_area;
		CUI_Widget	button_area;
		CUI_Widget	caption;
		CUI_Widget	list;
		CUI_Widget	item;

		int		i;
		int		label;

		char*		title;
		char*		cat_name;
		char*		item_string;

		cat_name = get_cat_name(idx);
		title = adjust(cat_name, litem_size);
		label = control_label(title);
		xfree(title);

		popup = CUI_vaCreateWidget(unique_name(), CUI_POPWIN_ID, NULL,
			widthId,		(void*) CUI_screenCols(),
			heightId,		(void*) CUI_screenRows(),
			hPadId,			(void*) 2,
			vPadId,			(void*) 0,
			hSpaceId,		(void*) hspace,
			vSpaceId,		(void*) -1,
			titleId,		(void*) label,
			alignCaptionsId,	(void*) trueId,
			nullStringId);

		make_screen(popup, &main_area, &button_area);

		i = 0;
		make_button(button_area, gettext("Done"), X_DONE, select_a_device_done,
			    popup, i++,0,centerId);
		make_button(button_area, gettext("Help..."), X_HELP,help_select_a_device,
			    NULL, i++,0,centerId);

		label = CUI_compileString(gettext("Select the device you want to add:"));
		caption = CUI_vaCreateWidget(unique_name(), CUI_CAPTION_ID,
			main_area,
			colId,			(void*) 1,
			rowId,			(void*) 0,
			labelId,		(void*) label,
			positionId,		(void*) topId,
			alignmentId, 		(void*) leftId,
			nullStringId);

		list = CUI_vaCreateWidget(unique_name(), CUI_LIST_ID, caption,
			/* This should extend to the end of the window: */
			heightId,		(void*) 6,
			toggleId,		(void *) trueId,
			helpId,			help_key(X_SEL_DEV),
			footerMessageId,	footer_key(X_SEL_DEV),
			nullStringId);

		set_cat_idx(idx);

		/* Make the list of devices in this category. */
		while ( (item_string = next_cat_dev_title()) != NULL ) {
			item_string = normalize(item_string, litem_size);
			item = CUI_vaCreateWidget(unique_name(), CUI_LITEM_ID, list,
				labelId, (void*) CUI_compileString(item_string),
				nullStringId);

			CUI_addCallback(item, CUI_SELECT_CALLBACK, add_new_device, list);
			xfree(item_string);
		}

		/* Save this window handle. */
		set_cat_ui_info((void*)popup, idx);
	}

	/* Reset category */
	set_cat_idx(idx);

	CUI_popup(popup, CUI_GRAB_EXCLUSIVE);
	push_popup(popup);

	return 0;
}


/*ARGSUSED*/
static int
select_a_category(CUI_Widget w, void* client_data, void* call_data)
{
	static CUI_Widget	popup;

	if ( popup == NULL ) {
		CUI_Widget	main_area;
		CUI_Widget	button_area;
		CUI_Widget	caption;
		CUI_Widget	list;
		CUI_Widget	item;

		int		label;
		int		row = 0;
		int		i;

		char*		item_string;

		label = CUI_compileString(gettext("Device categories"));

		popup = CUI_vaCreateWidget(unique_name(), CUI_POPWIN_ID, NULL,
			widthId,		(void*) CUI_screenCols(),
			heightId,		(void*) CUI_screenRows(),
			hPadId,			(void*) 2,
			vPadId,			(void*) 0,
			hSpaceId,		(void*) hspace,
			vSpaceId,		(void*) -1,
			titleId,		(void*) label,
			alignCaptionsId,	(void*) trueId,
			nullStringId);

		make_screen(popup, &main_area, &button_area);

		i = 0;
		make_button(button_area, gettext("Done"), X_DONE,
			    select_a_category_done, popup, i++,0,centerId);
		make_button(button_area, gettext("Help..."), X_HELP,
			    help_select_a_category, NULL, i++,0,centerId);

		label = CUI_compileString(gettext("Select the type of device you want to add:"));
		caption = CUI_vaCreateWidget(unique_name(), CUI_CAPTION_ID,
			main_area,
			colId,			(void*) 1,
			rowId,			(void*) row,
			labelId,		(void*) label,
			positionId,		(void*) topId,
			alignmentId,		(void*) leftId,
			nullStringId);

		list = CUI_vaCreateWidget(unique_name(), CUI_LIST_ID, caption,
			/* This should extend to the end of the window: */
			heightId,		(void*) 10,
                        toggleId,               (void*) trueId,
			helpId,			help_key(X_SEL_CAT),
			footerMessageId,	footer_key(X_SEL_CAT),
			nullStringId);
		++row;

		/* Make the list of categories. */
		while ( (item_string = next_cat_title()) != NULL ) {
			item_string = normalize(item_string, litem_size);
			item = CUI_vaCreateWidget(unique_name(), CUI_LITEM_ID, list,
				labelId, (void*) CUI_compileString(item_string),
				nullStringId);

			CUI_addCallback(item, CUI_SELECT_CALLBACK, select_a_device, list);
			xfree(item_string);
		}
		push_popup(popup);
	}

	CUI_popup(popup, CUI_GRAB_EXCLUSIVE);
	push_popup(popup);

	return 0;
}

static void
make_overview_popup()
{
	CUI_Widget main;
	int row;

	overview = CUI_vaCreateWidget("OVERVIEW", CUI_POPWIN_ID, NULL,
		rowId,			CUI_RESOURCE(1),
		widthId,		CUI_RESOURCE( CUI_screenCols()),
		heightId,		CUI_RESOURCE( CUI_screenRows()-5),
		hPadId,			(void*) 1,
		vPadId,			(void*) 1,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) 0,0,
		titleId,		CUI_STR_RESOURCE( MSG(OVRV_TITLE) ),
		adjustId,	(void*) centerId,
		nullStringId);

	row = 0;
	CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, overview,
		stringId,		CUI_STR_RESOURCE( MSG(MSG0) ),
		colId,			(void*) 1,
		rowId,			(void*) row,
		adjustId,		(void*) centerId,
		widthId,		(void*) (CUI_screenCols() - 10),
		nullStringId);
	++row;

	CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, overview,
		stringId,		CUI_STR_RESOURCE( MSG(MSG1) ),
		colId,			(void*) 1,
		rowId,			(void*) row,
		adjustId,		(void*) centerId,
		widthId,		(void*) (CUI_screenCols() - 10),
		nullStringId);
	++row;


    CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, overview,
        stringId,               CUI_STR_RESOURCE( MSG(MSG2) ),
        colId,                  (void*) 1,
        rowId,                  (void*) row,
        adjustId,               (void*) centerId,
        widthId,                (void*) (CUI_screenCols() - 10),
        nullStringId);
    ++row;


    CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, overview,
        stringId,               CUI_STR_RESOURCE( MSG(MSG3) ),
        colId,                  (void*) 1,
        rowId,                  (void*) row,
        adjustId,               (void*) centerId,
        widthId,                (void*) (CUI_screenCols() - 10),
        nullStringId);
    ++row;

	CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, overview,
		stringId,		CUI_STR_RESOURCE( "   " ),
		colId,			(void*) 1,
		rowId,			(void*) row,
		adjustId,		(void*) centerId,
		widthId,		(void*) (CUI_screenCols() - 10),
		nullStringId);
	++row;


		CUI_vaCreateWidget(unique_name(), CUI_OBLONGBUTTON_ID, 
		overview,
        rowId,                  (void*) row,
		adjustId,	CUI_RESOURCE(centerId),
		defaultId,	CUI_RESOURCE(trueId),
		labelId,	CUI_STR_RESOURCE( MSG(OKMSG) ),
		popdownOnSelectId, CUI_STR_RESOURCE("OVERVIEW"),
		nullStringId);

}

static ButtonSpec main_bspec[] = {
{ "dummy", X_CONT_INST, done, 				0 },
{ "dummy", X_CANCEL, 	done_no_save, 		0 },
{ "dummy", X_ADD, 		select_a_category,	NULL },
{ "dummy", X_HELP, 		help_summary, 		NULL },
};

static void
make_summary_window(CUI_Widget parent)
{
	CUI_Widget	button_area;
	CUI_Widget	item;
	CUI_Widget	list_menu;
	CUI_Widget	main_area;

	int		label;
	int		row = 1;

	make_overview_popup();

	make_screen(parent, &main_area, &button_area);

	main_bspec[0].name = MSG(CONTINUE);
	main_bspec[1].name = MSG(CANCEL);
	main_bspec[2].name = MSG(ADD);
	main_bspec[3].name = MSG(HELP);
	make_buttons(button_area, CUI_screenCols()-10, main_bspec, 4);

	CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, main_area,
		rowId, 		CUI_RESOURCE(row),
		colId,		CUI_RESOURCE(0),
		stringId,	CUI_STR_RESOURCE(MSG(OVER_LABEL)),
		adjustId,	CUI_RESOURCE(leftId),
		nullStringId);

	CUI_vaCreateWidget(unique_name(), CUI_OBLONGBUTTON_ID, 
		main_area,
		rowId,		CUI_RESOURCE(row),
		colId,		CUI_RESOURCE(1),
		labelId,	CUI_STR_RESOURCE((MSG(OVERVIEW))),
		helpId,			help_key(X_OVERVIEW),
		footerMessageId,	footer_key(X_OVERVIEW),
		popupOnSelectId, CUI_STR_RESOURCE("OVERVIEW"),
		nullStringId);

		
	row++;

	/* This is an invisible separator. */

	CUI_vaCreateWidget(unique_name(), CUI_STATIC_TEXT_ID, main_area,
		stringId,	CUI_STR_RESOURCE(" "),
		rowId,		CUI_RESOURCE(row),
		nullStringId);
	row++;

	label = CUI_compileString( MSG(CFGDEVS) );

	list_size = CUI_screenCols() - 9;
	litem_size = list_size - 5;
	summary_list = CUI_vaCreateWidget(unique_name(), CUI_LIST_ID, main_area,
		heightId,		(void*) 10,
		widthId,		(void*) list_size,
		rowId,			(void*) row,
		titleId,		(void*) label,
		helpId,			help_key(X_MAIN),
		footerMessageId,	footer_key(X_MAIN),
		nullStringId);
	++row;

	(void)populate_list(summary_list, next_dev_title);
		/* XXX  Add bogus item to list if retval == 0 ? */

	list_menu = CUI_vaCreateWidget(unique_name(), CUI_MENU_ID, summary_list,
		nullStringId);

	label = CUI_compileString( MSG(SHOWATTR) );
	item = CUI_vaCreateWidget(unique_name(), CUI_MITEM_ID, list_menu,
		labelId,		(void*) label,
		footerMessageId,	footer_key(X_SHOW_ATTR),
		nullStringId);
        CUI_addCallback(item, CUI_SELECT_CALLBACK, show_attributes, summary_list);

	label = CUI_compileString( MSG(UNCFG) );
	item = CUI_vaCreateWidget(unique_name(), CUI_MITEM_ID, list_menu,
		labelId,		(void*) label,
		footerMessageId,	footer_key(X_UNCONF),
		nullStringId);
        CUI_addCallback(item, CUI_SELECT_CALLBACK, unconfigure, summary_list);

}

static void
initialize(int ac, char* av[])
{
	int i;

	for ( i=1; i<ac; ++i ) {
		if ( *av[i] == '-' ) {
			char opt = *(av[i]+1);
			switch ( opt  ) {
			case 'v':
				++dvc_verbose;
				break;
				
			case 'f':
				++dvc_fake_data;
				/*FALLTHRU*/

			case 'x':
				++dvc_tmp_root;
				break;
				
			default:
				ui_error_exit(gettext("Usage: devconfig [-v] [-f]"));
			}
		}
	}

	if ( getuid() && !dvc_fake_data )
		ui_error_exit(gettext("You must run this program as root."));
}


int
main(int ac, char* av[])
{
	int		label;
	char 	*cui_home = getenv("CUIHOME");

	if(!cui_home)
		cui_home = dvc_home();
	putenv(strcats("CUIHOME=", cui_home, NULL));
	setlocale(LC_ALL, "");
	textdomain("SUNW_INSTALL_DEVCFG");

	label = CUI_compileString( MSG(DEVCFG) );

	toplevel = CUI_vaInitialize("devconfig", &ac, av,
		hPadId,			(void*) 2,
		vPadId,			(void*) 0,
		hSpaceId,		(void*) hspace,
		vSpaceId,		(void*) -1,
		titleId,		(void*) label,
		alignCaptionsId,	(void*) trueId,
		nullStringId);

	CUI_vaSetValues(toplevel,
		widthId,		(void*) CUI_screenCols(),
		heightId,		(void*) CUI_screenRows(),
		nullStringId);

	initialize(ac, av);

	fetch_cat_info();

	make_summary_window(toplevel);

	CUI_realizeWidget(toplevel);
	++realized;

	CUI_popup( overview, CUI_GRAB_EXCLUSIVE);
	CUI_mainLoop();

	return 1;
}
