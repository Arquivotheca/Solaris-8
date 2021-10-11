#pragma ident "@(#)api.cc   1.15     96/09/11 SMI"

/*
 * Copyright (c) 1996 Sun Microsystems, Inc.  
 * All Rights Reserved. 
 *
 * Sun considers its source code as an unpublished, proprietary trade secret, 
 * and it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 * 
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

//=============================================================================
//	CUI Toolkit API routines
//
//	$RCSfile: api.cc $ $Revision: 1.48 $ $Date: 1992/09/23 00:52:18 $
//=============================================================================

#include "precomp.h"

#ifndef  PRE_COMPILED_HEADERS

#include "cuiall.h"

#endif	// PRE_COMPILED_HEADERS


// make sure we only initialize this once!

static int count = 0;

// version info

static int majorVersion = 1;
static int minorVersion = 2;

// system StringTable

extern StringTable *CUI_Stringtab;
extern void CUI_createStringTable(void);


//
//	return version we're running
//

void CUI_version(int *major, int *minor)
{
	*major = majorVersion;
	*minor = minorVersion;
}


//
//	initialization
//

CUI_Widget CUI_initialize(char *name, CUI_Resource *resources,
						  int *, char **)
{
	if(count++)
		CUI_fatal(dgettext( CUI_MESSAGES, 
			"CUI_initialize called more than once"));

	// create and realize Application widget
	// we must upper-case name; reallocate it in case it's a constant string

	MEMHINT();
	name = CUI_newString(name);
	name[0] = toupper(name[0]);
    MEMHINT();
    Application *application = new Application(name);

	// create BaseWindow (should extract resources from command-line!)

	MEMHINT();
    BaseWindow *baseWin = new BaseWindow(name, NULL, resources);
	baseWin->appInit(application);

	// get rid of the reallocated name

	MEMHINT();
	CUI_free(name);

	// now we have a BaseWindow, create widgets from resource file

	CUI_Restab->load(TRUE);

    // return BaseWindow to caller

	return((CUI_Widget)baseWin);
}


//
//	cleanup
//

void CUI_exit(CUI_Widget handle)
{
	// make sure we're a BaseWindow, and we have an Application to delete

	BaseWindow *baseWin;
	baseWin = (BaseWindow *)Widget::verifyIsA((Widget *)handle, CUI_BASEWIN_ID);
	Application *application = baseWin->getApp();
	if(!application)
		CUI_fatal(dgettext(CUI_MESSAGES, 
			"BaseWindow argument to CUI_exit() was not created by CUI_init()"));

	// delete the BaseWindow and the Application

	MEMHINT();
    delete(baseWin);
	MEMHINT();
    delete(application);
}


//
//	create Widget
//

CUI_Widget CUI_createWidget(char *name, CUI_WidgetId type,
							CUI_Widget parentHandle, CUI_Resource * resources)
{
	Widget *parent = NULL;
	Widget *widget = NULL;

	if(parentHandle)
		parent = Widget::verify((Widget *)parentHandle);

	CUI_errno = 0;
	switch(type)
	{
		case CUI_WIDGET_ID:
			MEMHINT();
			widget = new Widget(name, parent, resources);
			break;
		case CUI_COMPOSITE_ID:
			MEMHINT();
            widget = new Composite(name, parent, resources);
			break;
		case CUI_CAPTION_ID:
			MEMHINT();
            widget = new Caption(name, parent, resources);
			break;
		case CUI_WINDOW_ID:
			MEMHINT();
            widget = new Window(name, parent, resources);
			break;
		case CUI_CONTROL_AREA_ID:
			MEMHINT();
            widget = new ControlArea(name, parent, resources);
			break;
		case CUI_CONTROL_ID:
			MEMHINT();
            widget = new Control(name, parent, resources);
			break;
		case CUI_BUTTON_ID:
			MEMHINT();
            widget = new Button(name, parent, resources);
			break;
		case CUI_SHELL_ID:
			MEMHINT();
            widget = new Shell(name, parent, resources);
			break;
		case CUI_BASEWIN_ID:
			MEMHINT();
            widget = new BaseWindow(name, parent, resources);
			break;
		case CUI_POPWIN_ID:
			MEMHINT();
            widget = new PopupWindow(name, parent, resources);
			break;
        case CUI_MENU_ID:
			MEMHINT();
            widget = new Menu(name, parent, resources);
			break;
		case CUI_MITEM_ID:
			MEMHINT();
            widget = new MenuItem(name, parent, resources);
			break;
		case CUI_MENUBUTTON_ID:
			MEMHINT();
            widget = new MenuButton(name, parent, resources);
			break;
		case CUI_OBLONGBUTTON_ID:
			MEMHINT();
            widget = new OblongButton(name, parent, resources);
			break;
		case CUI_RECTBUTTON_ID:
			MEMHINT();
            widget = new RectButton(name, parent, resources);
			break;
		case CUI_CHECKBOX_ID:
			MEMHINT();
            widget = new CheckBox(name, parent, resources);
			break;
		case CUI_TEXTEDIT_ID:
			MEMHINT();
            widget = new TextEdit(name, parent, resources);
			break;
		case CUI_STATIC_TEXT_ID:
			MEMHINT();
            widget = new StaticText(name, parent, resources);
			break;
        case CUI_TEXTFIELD_ID:
			MEMHINT();
            widget = new TextField(name, parent, resources);
			break;
		case CUI_NUMFIELD_ID:
			MEMHINT();
            widget = new NumericField(name, parent, resources);
			break;
		case CUI_EXCLUSIVES_ID:
			MEMHINT();
            widget = new Exclusives(name, parent, resources);
			break;
		case CUI_NONEXCLUSIVES_ID:
			MEMHINT();
            widget = new NonExclusives(name, parent, resources);
			break;
		case CUI_NOTICE_ID:
			MEMHINT();
            widget = new Notice(name, parent, resources);
			break;
		case CUI_LIST_ID:
			MEMHINT();
            widget = new ScrollingList(name, parent, resources);
			break;
		case CUI_LITEM_ID:
			MEMHINT();
            widget = new ListItem(name, parent, resources);
			break;
		case CUI_ABBREV_ID:
			MEMHINT();
            widget = new AbbrevMenuButton(name, parent, resources);
			break;
		case CUI_FOOTER_ID:
			MEMHINT();
            widget = new FooterPanel(name, parent, resources);
			break;
		case CUI_SEPARATOR_ID:
			MEMHINT();
            widget = new Separator(name, parent, resources);
			break;
		case CUI_TEXTPANEL_ID:
			MEMHINT();
            widget = new TextPanel(name, parent, resources);
			break;
		case CUI_GAUGE_ID:
			MEMHINT();
            widget = new Gauge(name, parent, resources);
			break;
		case CUI_SLIDER_ID:
			MEMHINT();
            widget = new Slider(name, parent, resources);
			break;
		case CUI_HELPWIN_ID:
			MEMHINT();
			widget = new HelpWindow(name, parent, resources);
			break;
		case CUI_HYPERPANEL_ID:
			MEMHINT();
			widget = new HypertextPanel(name, parent, resources);
			break;
		case CUI_COLOR_ID:
			MEMHINT();
			widget = new Color(name, parent, resources);
			break;
        default:
            // by default, widget is NULL
			break;
	}
	if(!CUI_errno)
		return((CUI_Widget)widget);
	else
		return(NULL);
}

 
//
//      delete widget
//      (should hide/unmap before deleting...)
//
 
static int _delete_widget( Widget *widget)
{
        if(!widget)
                return(-1);
        else
        {
                MEMHINT();
        delete(widget);
                return(0);
        }
}
 
int CUI_deleteWidgetByHandle(CUI_Widget handle)
{
        Widget *widget = Widget::verify((Widget *)handle);
        return(_delete_widget(widget));
}

int CUI_deleteWidget(char *name)
{
        Widget *widget = Widget::lookup(name);
        return(_delete_widget(widget));
}


//
//	realize Widget
//

int CUI_realizeWidget(CUI_Widget handle)
{
	Widget *widget = Widget::verify((Widget *)handle);
	return(widget->realize());
}


//
//	main event loop
//

void CUI_mainLoop(void)
{
	CUI_Emanager->mainLoop();
}


//
//	set/get Widget resource values
//

int CUI_setValues(CUI_Widget handle, CUI_Resource* resources)
{
	Widget *widget = Widget::verify((Widget *)handle);
	return(widget->setValues(resources));
}

int CUI_getValues(CUI_Widget handle, CUI_Resource* resources)
{
    Widget *widget = Widget::verify((Widget *)handle);
	return(widget->getValues(resources));
}


//
//	add callback to Widget
//

int CUI_addCallback(CUI_Widget handle, CUI_CallbackType type,
					CUI_CallbackProc callback, void *data)
{
	Widget *widget = Widget::verify((Widget *)handle);
	return(widget->addCallback(callback, type, data));
}


//
//	compile string
//

int CUI_compileString(char *string)
{

    // if we don't yet have a StringTable, create it,
	// and ensure that our 'known strings' are first

    if(!CUI_Stringtab)
		CUI_createStringTable();
    return(CUI_Stringtab->compile(string));
}


//
//	decompile string
//

char *CUI_lookupString(int id)
{
    return(CUI_Stringtab->value(id));
}


//
//	info message
//

void CUI_infoMessage(char *format, ...)
{
	// format the error message

	va_list argptr;
	va_start(argptr, format);
	vsprintf(CUI_buffer, format, argptr);
	va_end(argptr);
	CUI_display->infoMessage("%s", CUI_buffer);
}


//
//	popup/down a PopupWindow or Menu
//	(these should be virtualized so we don't test type here!)
//

int CUI_popup(CUI_Widget handle, CUI_GrabMode mode)
{
	Widget *widget = Widget::verify((Widget *)handle);
	if(widget->isKindOf(CUI_POPWIN_ID))
	{
		PopupWindow *window = (PopupWindow *)widget;
		return(window->popUp(mode));
	}
	if(widget->isKindOf(CUI_MENU_ID))
    {
		Menu *menu = (Menu *)widget;
		return(menu->popUp(mode));
	}
	return(-1);
}

int CUI_popdown(CUI_Widget handle)
{
	Widget *widget = Widget::verify((Widget *)handle);
	if(widget->isKindOf(CUI_POPWIN_ID))
	{
		PopupWindow *window = (PopupWindow *)widget;
		window->popDown();
		return(0);
    }
	if(widget->isKindOf(CUI_MENU_ID))
	{
		Menu *menu = (Menu *)widget;
		menu->popDown();
		return(0);
	}
	return(-1);
}


//
//	lookup a widget by name
//

CUI_Widget CUI_lookupWidget(char *name)
{
	return(Widget::lookup(name));
}


//
//	return a widget's name
//

char *CUI_widgetName(CUI_Widget handle)
{
    Widget *widget = Widget::verify((Widget *)handle);
    return(widget->getName());
}


//
//	return handle of widget's parent
//

CUI_Widget CUI_parent(CUI_Widget handle)
{
	Widget *widget = Widget::verify((Widget *)handle);
	return(widget->getParent());
}


//
//	insert a key-filter
//

CUI_KeyFilter CUI_setKeyFilter(CUI_KeyFilter filter)
{
	return(Emanager::filterKeys(filter));
}


//
//	disable/enable display updating
//

bool CUI_updateDisplay(bool mode)
{
	int oldMode;
	if(mode)
	{
		oldMode = CUI_display->update(TRUE);
		CUI_display->refresh(FALSE);
	}
	else
		oldMode = CUI_display->update(FALSE);
	return(oldMode != 0);
}


//
//	return current display update mode (without changing)
//

bool CUI_updateMode(void)
{
	return(CUI_display->updateMode());
}


//
//	refresh display (force redraw if flag set)
//

void CUI_refreshDisplay(bool redraw)
{
	CUI_display->refresh(redraw);
}


//
//	save/restore display
//

int CUI_saveDisplay(void)
{
	return(CUI_display->save());
}

int CUI_restoreDisplay(void)
{
	return(CUI_display->restore());
}


//=============================================================================
//	varargs routines
//=============================================================================


#ifdef VOID_MALLOC
#define MALLOC_TYPE	void *
#define FREE_ARG_TYPE	void *
#else
#define MALLOC_TYPE	malloc_t
#define FREE_ARG_TYPE	char *
#endif /* VOID_MALLOC */


//
//	Support routine, takes a varargs list of resource id/value
//	pairs, terminated by a nullStringId resource name, and returns
//	an array of CUI_Resource structures with the ids and values
//	filled in.
//

static CUI_Resource *
va_get_resources(va_list ap)
{

	unsigned short		cnt = 0;
	const unsigned short	num = 10;
	CUI_Resource		*res;
	size_t			alloc_size;


	alloc_size = num * sizeof (CUI_Resource);

	res = (CUI_Resource *)malloc(alloc_size);

	if (res == (CUI_Resource *)NULL) {
		return NULL;
	}

	while ((res[cnt].id = va_arg(ap, CUI_StringId)) != nullStringId) {

		res[cnt].value = va_arg(ap, void *);

		if (++cnt % num == 0) {

			alloc_size = (int)cnt / (int)(num + 1) * num *
			    sizeof (CUI_Resource);

			res = (CUI_Resource *)realloc((MALLOC_TYPE)res,
						      alloc_size);

			if (res == (CUI_Resource *)NULL) {
				return NULL;
			}
		}
	}

	return(res);
}


//
//	Support routine, frees an array returned by va_get_resources().
//

static void
va_free_resources(CUI_Resource *res)
{
	if (res)
		free((FREE_ARG_TYPE)res);
}


//
//	API extensions.  Provides varargs interfaces for CUI_initialize(),
//	CUI_createWidget(), CUI_getValues(), and CUI_setValues(), and a
//	public interface to va_free_resources() for users of vaGeValues.
//

void
CUI_vaFreeResources(CUI_Resource *res)
{
	va_free_resources(res);
}


CUI_Widget
CUI_vaInitialize(char *name, int *argc, char *argv[], ...)
{
	va_list 	ap;
	CUI_Resource	*res;
	CUI_Widget	retval;

	va_start(ap, argv);
	if (! (res = va_get_resources(ap))) {
		return NULL;
	}
	va_end(ap);

	retval = CUI_initialize(name, res, argc, argv);
	va_free_resources(res);
	return(retval);
}


CUI_Widget
CUI_vaCreateWidget(char *name, CUI_WidgetId type, CUI_Widget parentHandle, ...)
{
	va_list 	ap;
	CUI_Resource	*res;
	CUI_Widget	retval;

	va_start(ap, parentHandle);
	if (! (res = va_get_resources(ap))) {
		return NULL;
	}
	va_end(ap);

	retval = CUI_createWidget(name, type, parentHandle, res);
	va_free_resources(res);
	return(retval);
}


int
CUI_vaSetValues(CUI_Widget handle, ...)
{
	va_list 	ap;
	CUI_Resource	*res;
	int 		retval;

	va_start(ap, handle);
	if (! (res = va_get_resources(ap))) {
		return NULL;
	}
	va_end(ap);

	retval = CUI_setValues(handle, res);
	va_free_resources(res);
	return(retval);
}


CUI_Resource *
CUI_vaGetValues(CUI_Widget handle, ...)
{
	va_list 	ap;
	CUI_Resource	*res;

	va_start(ap, handle);
	if (! (res = va_get_resources(ap))) {
		return NULL;
	}
	va_end(ap);

	(void) CUI_getValues(handle, res);

	return(res);
}


//=============================================================================
//  TextField utility routines
//=============================================================================

//
//	get buffer associated with TextEdit
//

static char *getBuffer(CUI_Widget widget)
{
	static bool validating = FALSE;

	TextEdit *edit = (TextEdit *)Widget::verifyIsKindOf((Widget *)widget,
					 CUI_TEXTEDIT_ID);
	FIELD *field = edit->ETIfield();
	if(!field)
		return(NULL);

	// ensure buffer is flushed (but beware infinite recursion!)

	if(!validating)
	{
		validating = TRUE;
		FORM *form = edit->ETIform();
		if(form)
			form_driver(form, REQ_VALIDATION);
		validating = FALSE;
	}

	// fetch the buffer

	char *data = CUI_rtrim(field_buffer(field, CUI_INIT_BUFF));
	if(CUI_strempty(data))
		return(NULL);
	return(data);
}


//
//	copy TextField data to caller's buffer (see OLTextFieldCopyString)
//

int CUI_textFieldCopyString(CUI_Widget widget, char *string)
{
	char *data = getBuffer(widget);
	int len = 0;
	if(data)
	{
		strcpy(string, data);
		len = strlen(data);
	}
	return(len);
}


//
//	return newly-allocated copy of TextField data (see OLTextFieldGetString)
//

char *CUI_textFieldGetString(CUI_Widget widget, int *size)
{
	char *data = getBuffer(widget);
	int len = 0;
	char *copy = NULL;
	if(data)
	{
		MEMHINT();
        copy = CUI_newString(data);
		len = strlen(data);
	}
	if(size)
		*size = len;
	return(copy);
}


//=============================================================================
//	Menu/ScrollingList routines
//=============================================================================


//
//	add an Item to Menu or ScrollingList
//

int CUI_addItem(CUI_Widget menuOrList, CUI_Widget menuOrListItem, int index)
{
	Menu *menu;
	Item *item;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	item = (Item *)Widget::verifyIsKindOf((Widget *)menuOrListItem, CUI_ITEM_ID);
	return(menu->addItem(index, item));
}


//
//	delete an Item from Menu or ScrollingList
//

int CUI_deleteItem(CUI_Widget menuOrList, int index)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	return(menu->deleteItem(index));
}


//
//	lookup Item in Menu or ScrollingList
//

CUI_Widget CUI_lookupItem(CUI_Widget menuOrList, int index)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	return(menu->lookupItem(index));
}


//
//	make Item current in Menu or ScrollingList
//

int CUI_setCurrentItem(CUI_Widget menuOrList, int index)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	return(menu->setCurrentItem(index));
}


//
//	return index of current item in Menu or ScrollingList
//

int CUI_getCurrentItem(CUI_Widget menuOrList)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	return(menu->getCurrentItem());
}


//
//  return the number of items in the list
//

int CUI_countListItems(CUI_Widget menuOrList)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	return(menu->countItems());
}


//
//	refresh Menu/List after adding/removing items
//

int CUI_refreshItems(CUI_Widget menuOrList)
{
	Menu *menu;
	menu = (Menu *)Widget::verifyIsKindOf((Widget *)menuOrList, CUI_MENU_ID);
	menu->show();
	return(0);
}


//
//	return number of screen rows/cols
//

int CUI_screenRows(void)
{
	if(CUI_display)
		return(CUI_display->rows());
	else
		return(-1);
}

int CUI_screenCols(void)
{
	if(CUI_display)
		return(CUI_display->cols());
	else
		return(-1);
}

bool CUI_hasColor(void)
{
	if(CUI_display)
		return(CUI_display->hasColor());
	else
		return(FALSE);
}


//=============================================================================
//	TextPanel routines
//=============================================================================


void CUI_textPanelSetcur(CUI_Widget handle, int row, int col)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->setcur(row, col);
}

void CUI_textPanelClear(CUI_Widget handle)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->clear();
}

void CUI_textPanelClearRow(CUI_Widget handle, int row)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->clearRow(row);
}

void CUI_textPanelScroll(CUI_Widget handle, int count)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->scroll(count);
}

void CUI_textPanelPrint(CUI_Widget handle, char *text)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->print(text);
}

void CUI_textPanelPrintLine(CUI_Widget handle, int row, char *text)
{
	TextPanel *panel;
	panel = (TextPanel *)Widget::verifyIsA((Widget *)handle, CUI_TEXTPANEL_ID);
	panel->printLine(row, text);
}


//=============================================================================
//	Help routines
//=============================================================================

int CUI_showHelp(char *topic)
{
	if(topic && CUI_helpWindow)
		return(((HelpWindow *)CUI_helpWindow)->showHelp(topic));
	else
		return(-1);
}

int CUI_showHelpIndex(void)
{
	if(CUI_helpWindow)
		return(((HelpWindow *)CUI_helpWindow)->showHelp(HELP_INDEX_KEY));
	else
		return(-1);
}


//=============================================================================
//	Cursor routines
//=============================================================================

void CUI_getCursor(short *row, short *col)
{
	CUI_display->getcur(*row, *col);
}

void CUI_setCursor(short row, short col)
{
	CUI_display->setcur(row, col);
}

