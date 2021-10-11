#pragma ident "@(#)cuicomp.cc   1.8     93/07/23 SMI"

/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
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

//===========================================================================
//	CUI resource-file compiler (generates 'app.cc' for application 'app')
//
//	$RCSfile: cuicomp.cc $ $Revision: 1.14 $ $Date: 1992/09/13 00:23:50 $
//===========================================================================

#include  "precomp.h"

#ifndef  PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif

#include "cuilib.h"

#endif	// PRE_COMPILED_HEADERS


//
//	define string equivalents of widget types
//	(keep this up-to-date with cuitypes.h!)
//

static char *typeNames[] =
{
	"CUI_NULL_ID",
	"CUI_WIDGET_ID",
	"CUI_COMPOSITE_ID",
	"CUI_CONTROL_AREA_ID",
	"CUI_CAPTION_ID",
	"CUI_CONTROL_ID",
	"CUI_BUTTON_ID",
	"CUI_OBLONGBUTTON_ID",
	"CUI_RECTBUTTON_ID",
	"CUI_MENUBUTTON_ID",
	"CUI_CHECKBOX_ID",
	"CUI_TEXTEDIT_ID",
	"CUI_TEXTFIELD_ID",
	"CUI_STATIC_TEXT_ID",
	"CUI_NUMFIELD_ID",
	"CUI_ITEM_ID",
	"CUI_MENU_ID",
	"CUI_MITEM_ID",
	"CUI_WINDOW_ID",
	"CUI_SCROLLBAR_ID",
	"CUI_SHELL_ID",
	"CUI_BASEWIN_ID",
	"CUI_VCONTROL_ID",
	"CUI_EXCLUSIVES_ID",
	"CUI_NONEXCLUSIVES_ID",
	"CUI_LIST_ID",
	"CUI_LITEM_ID",
	"CUI_NOTICE_ID",
	"CUI_POPWIN_ID",
	"CUI_ABBREV_ID",
	"CUI_FOOTER_ID",
	"CUI_SEPARATOR_ID",
	"CUI_SLIDER_CONTROL_ID",
	"CUI_GAUGE_ID",
	"CUI_SLIDER_ID",
	"CUI_TEXTPANEL_ID"
};


//
//	prototypes
//

static void blockComment(char *format,...);
static void comment(char *format,...);
static void parseFile(char *file);
static void declareWidgets(void);
static void defineWidgets(void);
static void assignResources(void);
static char *nextLine(char *file);
static bool definesWidget(char *line);
static bool assignsResource(char *line);


//
//	typdefs and structure definitions
//

#define BUMP 32

typedef struct
{
	CUI_WidgetId type;
	char		 *name;
	char		 *parent;
} WidgetDef;

typedef struct
{
	char		 *widget;
	char		 *resource;
	char		 *value;
} ResourceDef;


//
//	statics/globals
//

static char appName[80];
static FILE *outFd = NULL;
static FILE *inFd = NULL;
static WidgetDef **widgets = NULL;
static int widgetArraySize = 0;
static int numWidgets = 0;
static ResourceDef **resources = NULL;
static int resourceArraySize = 0;
static int numResources = 0;


static char *commentLine =
"//===========================================================================";

//
//	text we will output for our definitions
//

static char *defsText = "\
#include \"cui.h\"\n\n\
extern bool CUI_noResfile;\n\
extern int	appInitialize(int, char *[]);\n\
extern int	appStartup(void);\n\
extern int	appRealize(void);\n\
extern int	appCleanup(void);\n\
static void defineWidgets(void);\n\
static void assignResources(void);\n\n\n";


//
//	text we will output for the main routine
//

static char *mainText = "\
int main(int argc, char *argv[])\n\
{\n\
	// say we don't want to load a resource file\n\n\
	CUI_noResfile = TRUE;\n\n\
    // initialize CUI library\n\n\
	CUI_Widget_%s = CUI_initialize(\"%s\", NULL, &argc, argv);\n\
	if(!CUI_Widget_%s)\n\
	{\n\
		fprintf(stderr, \"Can't initialize %s\\n\");\n\
		exit(1);\n\
	}\n\n\
	// call application-specific intialization routine\n\n\
	appInitialize(argc, argv);\n\n\
    // define widgets and assign resources to them\n\n\
	defineWidgets();\n\
	assignResources();\n\n\
	// call application-specific startup routine\n\n\
	appStartup();\n\n\
	// realize BaseWindow and process events\n\n\
    CUI_realizeWidget(CUI_Widget_%s);\n\
	// post-realize hook\n\
	appRealize();\n\n\
	CUI_mainLoop();\n\n\
	// call application-specific cleanup routine and exit\n\n\
	appCleanup();\n\
	CUI_exit(CUI_Widget_%s);\n\
	return(0);\n\
}\n\n\n";


//
//	do it
//

int main(int argc, char *argv[])
{
	char buffer[1024];

    // check args

	if(argc < 3)
		CUI_fatal(dgettext( CUI_MESSAGES,"Usage: %s appName resfile [resfile...]"), argv[0]);

	// open the file we'll write our generated source to

	sprintf(buffer, "%s.cc", argv[1]);
	buffer[0] = tolower(buffer[0]);
	outFd = fopen(buffer, "w");
	if(!outFd)
		CUI_fatal(dgettext( CUI_MESSAGES,"Can't open output file"));

	// allocate arrays of WidgetDefs and ResourceDefs

	widgetArraySize = BUMP;
	numWidgets		= 0;
	widgets = (WidgetDef**)CUI_malloc(widgetArraySize * sizeof(WidgetDef *));
	resourceArraySize = BUMP;
	numResources = 0;
	resources = (ResourceDef**)CUI_malloc(resourceArraySize * sizeof(ResourceDef *));

	// generate application name

    strcpy(appName, argv[1]);
	appName[0] = toupper(appName[0]);

    // parse resource files

	for(int i = 2; i < argc; i++)
		parseFile(argv[i]);

	// write header declarations

	sprintf(buffer, "%s.cc: CUI application main file (generated by cuicomp)",
					 argv[1]);
	buffer[0] = tolower(buffer[0]);
	blockComment(buffer);
	fprintf(outFd, defsText, appName);

	// write declarations of all defined widgets

	declareWidgets();

	// write main routine

    blockComment("main routine");
	fprintf(outFd, mainText,
			appName, appName, appName, appName, appName, appName);

	// write function to define widgets

	blockComment("define widgets");
	fprintf(outFd, "static void defineWidgets(void)\n{\n");
    defineWidgets();
	fprintf(outFd, "}\n\n\n");

	// write function to assign resources

	blockComment("assign resources");
	fprintf(outFd, "static void assignResources(void)\n{\n");
    assignResources();
	fprintf(outFd, "}\n\n");

    // close output file and return success

	fclose(outFd);
	return(0);
}


//
//	parse a resource file
//

static void parseFile(char *file)
{
	inFd = fopen(file, "r");
	if(!inFd)
		CUI_fatal(dgettext( CUI_MESSAGES,"Can't open resource file '%s'"), file);

	printf(dgettext( CUI_MESSAGES,"processing %s\n"), file);

	// scan file for widget definitions

	char *line;
	while(line = nextLine(file))
	{
		if(!definesWidget(line))
			assignsResource(line);
	}
    fclose(inFd);
}


//
//	declare globals for all defined widgets
//

static void declareWidgets(void)
{
	blockComment("global Widget handles");

	fprintf(outFd,
		"CUI_Widget CUI_Widget_%s;  // application's base window\n\n",
		appName);
    for(int i = 0; i < numWidgets; i++)
	{
		fprintf(outFd, "CUI_Widget CUI_Widget_%s;\n", widgets[i]->name);
	}
	fprintf(outFd, "\n\n");
}


//
//	define widgets
//

static void defineWidgets(void)
{
	char buffer[1024];

    for(int i = 0; i < numWidgets; i++)
	{
		char *typeName = typeNames[widgets[i]->type - CUI_BUMP];
		char *parent = "NULL";
		if(widgets[i]->parent)
		{
			sprintf(buffer, "CUI_Widget_%s", widgets[i]->parent);
			parent = buffer;
		}
		fprintf(outFd,
				"\tCUI_Widget_%s = CUI_createWidget(\"%s\", %s, %s, NULL);\n",
				widgets[i]->name,
                widgets[i]->name,
				typeName,
				parent);
	}
}


//
//	assign resources to defined widgets
//

static void assignResources(void)
{
	char buffer[1024];

    for(int i = 0; i < numResources; i++)
	{
		// have we defined this widget, or is it external?

		bool external = TRUE;
		char *widgetName = resources[i]->widget;
		if(strcmp(widgetName, appName) == 0)
			external = FALSE;
		else
		{
			for(int j = 0; j < numWidgets; j++)
			{
				if(strcmp(widgetName, widgets[j]->name) == 0)
				{
					external = FALSE;
					break;
				}
			}
		}

		// generate a call to CUI_vaSetValues for all consecutive
		// resource assignments to the same widget

		if(external)
			fprintf(outFd, "\tCUI_vaSetValues(%s,\n", widgetName);
        else
			fprintf(outFd, "\tCUI_vaSetValues(CUI_Widget_%s,\n", widgetName);

        while(strcmp(resources[i]->widget, widgetName) == 0)
		{
			char *value = resources[i]->value;

			// if value is a number, we can use directly

			int num;
			if(sscanf(value, "%d", &num) == 1)
			{
				sprintf(buffer, "CUI_RESOURCE(%s)", value);
			}

			// if it's a quoted string, we must compile it

			else if(value[0] == '"')
			{
			/* dvb 6/18
			 * For these resource names, add the gettext so it can be
			 * localized, and included in a message file.
			 */
				if ((!strcmp(resources[i]->resource, "label" ) ) ||
				    (!strcmp(resources[i]->resource, "string") ) ||
					(!strcmp(resources[i]->resource, "title" ) ) )
					sprintf(buffer, "CUI_STR_RESOURCE(gettext(%s))", value);
				else
					sprintf(buffer, "CUI_STR_RESOURCE(%s)", value);
			}

			// else assume it's a known string - suffix with 'Id'

			else
			{
				sprintf(buffer, "CUI_RESOURCE(%sId)", value);
            }

			// generate next line of varargs list

			fprintf(outFd, "\t\t%sId, %s,\n", resources[i]->resource, buffer);
			if(++i == numResources)
				break;
		}
		i--;	// we've looked-ahead to next assignment

		// terminate varargs list with 'nullStringId'

        fprintf(outFd, "\t\tnullStringId);\n");
	}
}


//===========================================================================
//	helper routines
//===========================================================================


//
//	write a block comment
//

static void blockComment(char *format, ...)
{
	char buffer[1024];

    // format the comment

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	// output it prettily

	fprintf(outFd, commentLine);
	fprintf(outFd, "\n//  %s\n", buffer);
	fprintf(outFd, "%s\n\n", commentLine);
}


//
//	write a regular comment
//

static void comment(char *format, ...)
{
	char buffer[1024];

    // format the comment

	va_list argptr;
	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	// output it prettily

	fprintf(outFd, "// %s\n\n", buffer);
}


//
//	read line from input file, skipping blank lines and comments
//

static char *nextLine(char *file)
{
	static char inBuff[1024];
	while(TRUE)
	{
		if(!fgets(inBuff, 1024, inFd))
		{
			if(feof(inFd))
				break;
			else
				CUI_fatal(dgettext( CUI_MESSAGES,"Fatal error reading resource file '%s'"),
						  file);
		}
		int len = strlen(inBuff);
		if(inBuff[0] == '!')
			continue;
		if(CUI_strempty(inBuff))
			continue;
		if(inBuff[len - 1] == '\n')
			inBuff[len - 1] = 0;
        return(inBuff);
	}
	return(NULL);
}


//
//	look for a widget definition in line
//

static bool definesWidget(char *line)
{
	char type[1024];
	char def[1024];
	CUI_WidgetId id = CUI_NULL_ID;
	char *name = NULL;
	char *parent = NULL;

	if(sscanf(line, "%s %s", type, def) == 2)
	{
		int len = strlen(type);
		if(type[len - 1] == ':')
		{
			type[len - 1] = 0;
			id = Widget::lookupName(type);
			if(id != CUI_NULL_ID)
			{
				name = def;
                char *ptr = strchr(def, ':');
				if(ptr)
				{
					*ptr++ = 0;
					parent = ptr;
				}

				// bump widgets array if necessary

				if(numWidgets == widgetArraySize)
				{
					widgets = (WidgetDef **)CUI_arrayGrow((char **)widgets, BUMP);
					widgetArraySize += BUMP;
				}

				// add a new element to the widgets array

				widgets[numWidgets] = (WidgetDef *)CUI_malloc(sizeof(WidgetDef));
				widgets[numWidgets]->type	= id;
				widgets[numWidgets]->name	= CUI_newString(name);
				widgets[numWidgets]->parent = CUI_newString(parent);
				numWidgets++;
            }
        }
	}
	return(id != CUI_NULL_ID);
}


//
//	look for a resource assignment in line
//

static bool assignsResource(char *line)
{
	char def[1024];
	char *value    = NULL;
	char *widget   = NULL;
	char *resource = NULL;

	if(sscanf(line, "%s", def) == 1)
	{
		// zap trailing ':' in definition

		int len = strlen(def);
		if(def[len - 1] == ':')
			def[len - 1] = 0;

		// scan backwards for resource name

		char *ptr = &(def[len - 2]);
		while(ptr >= def && *ptr != '.')
			ptr--;
		if(*ptr == '.')
			*ptr = 0;
		resource = ptr + 1;

		// scan backwards for widget name

		ptr--;
		while(ptr >= def && *ptr != '.' && *ptr != '*')
			ptr--;
		if(*ptr == '.')
			*ptr = 0;
		widget = ptr + 1;

		// scan for resource value

		ptr = strchr(line, ':');
		if(!ptr)
			return(FALSE);
		ptr++;
		while(*ptr && (*ptr == '\t' || *ptr == ' '))
			ptr++;
		value = ptr;

        // bump resource array if necessary

		if(numResources == resourceArraySize)
		{
			resources = (ResourceDef **)CUI_arrayGrow((char **)resources, BUMP);
			resourceArraySize += BUMP;
		}

		// add a new element to the resource array

		resources[numResources] = (ResourceDef *)CUI_malloc(sizeof(ResourceDef));
		resources[numResources]->widget   = CUI_newString(widget);
		resources[numResources]->resource = CUI_newString(resource);
		resources[numResources]->value	  = CUI_newString(value);
		numResources++;

		return(TRUE);
	}
	return(FALSE);
}

