#pragma ident "@(#)restab.cc   1.4     93/07/23 SMI"

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

//=============================================================================
//	Resource Table class implementation
//
//	$RCSfile: restab.cc $ $Revision: 1.19 $ $Date: 1992/09/12 15:26:15 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#include <stdlib.h>
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_RESTAB_H
#include "restab.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#define BUMP		16
#define MAX_MATCHES 100
#define isSeparator(c)	(c == '.' || c == '*')

bool CUI_noResfile = FALSE;   // set to TRUE in cuicomp-generated code


//=============================================================================
//	static utility routines
//=============================================================================


//
//	skip past next component of resource string (ptr points into string)
//

static char *skip(char *ptr)
{
	// if we're pointing at a separator, just bump by one to go past it
	// adjacent separators cause 'undefined results'

	if(isSeparator(*ptr))
		return(++ptr);

	// advance till next separator or EOS

	while(*ptr && !isSeparator(*ptr))
		ptr++;

	// if we're at EOS return NULL

	if(*ptr == 0)
		return(NULL);

	// we're at a separator...

	// if it's '*', return it as is,
	// else advance past it (sanity check we don't run off the end)

	switch(*ptr)
	{
		case '*':
			return(ptr);
		case ' ':
			return(NULL);
		default:
			return(++ptr);
	}
}


//
//	compare resource specification (possibly containing wildcards)
//	with fully-qualified class or instance hierarchy specification
//
//	returns >= 0 for success, -1 for failure
//	(retval is element number at which we first matched element, so we
//	can apply precedence rules; 0 = pure wildcard, 1 = match 1st element,
//	2 = matched 2nd element, etc.)
//

static int compare(char *rSpec, char *hSpec)
{
	int matchedAt = 0;
	int element   = 1;

    // point to beginning of each string

	char *rPtr = rSpec;
	char *hPtr = hSpec;

	// away we go...

	while(1)
	{
		// anything more to match?

		// if no more in hSpec...

		if(!hPtr)
		{
			// if rSpec is now empty, or just contains '*'
			// we have a match, else we've failed

			if(!rPtr || (*rPtr == '*' && *(rPtr + 1) == 0))
				return(matchedAt);
			else
				return(-1);
		}

		// if no more in rSpec...

		if(!rPtr)
		{
			// if hSpec is now empty, we have a match, else we've failed

			if(!hPtr)
				return(matchedAt);
			else
				return(-1);
		}

		// if we have a wildcard match...

		if(*rPtr == '*')
		{
			// check what follows the '*' against next component of hSpec

			// figure out how much to compare

			char *nextH = skip(hPtr);
			int len;
			if(!nextH)
				len = strlen(hPtr);
			else
				len = nextH - hPtr - 1; 	// allow for separator

			// if comparison succeeds...

			if(strncmp(rPtr + 1, hPtr, len) == 0)
			{
				// check for a false match (eg, "foo" against "fooBar")

				char *next = rPtr + len + 1;
				if(!*next || isSeparator(*next) || *next == ':')
				{
					// match is real; if it's first match, save element number

					if(!matchedAt)
						matchedAt = element;

					// strip '*' and next entry from rSpec

					rPtr = skip(rPtr + 1);
				}
			}

			// unconditionally skip to next entry of hSpec
			// bump element number

			hPtr = skip(hPtr);
			element++;
		}
		else // next component of each string must match
		{
			// figure out how much to compare

			char *nextH = skip(hPtr);
			int len;
			if(!nextH)
				len = strlen(hPtr);
			else
				len = nextH - hPtr - 1; 	// allow for separator

			// if comparison succeeds...

			if(strncmp(rPtr, hPtr, len) == 0)
			{
				// bump each string to next component

				hPtr = nextH;
				rPtr = skip(rPtr);

                // if this is first match, save element number

				if(!matchedAt)
					matchedAt = element++;
			}

			// else return failure

			else
				return(-1);
		}
	}
}


//
//	split a resource string into resource and name parts
//	(reallocates/compiles the pieces and returns by reference)
//

static void split(char *string, char * &resource, CUI_StringId &name)
{
	// point beyond end of the string

	char *ptr = string + strlen(string) - 1;

	// search backwards for '.' or '*' (we know there's at least one,
	// since we sanity-checked before we called this routine)

	for( ; !isSeparator(*ptr) ; ptr--)
		; // nothing

	// bump past the separator to isolate the name portion, and compile it

	name = (CUI_StringId)CUI_compileString(ptr + 1);

	// if separator is '*', keep it, else zap it;
	// copy what's left of string into resource

	if(*ptr == '.')
		*ptr = 0;
	else
		*(++ptr) = 0;
	MEMHINT();
    resource = CUI_newString(string);
}


//
//	check two resource-specification strings to see which has precedence
//	(returns TRUE if first has precedence over second, else FALSE)
//

static int hasPrecedence(char *first, char *second)
{
	NO_REF(first);
	NO_REF(second);
	return(TRUE);
}


//
//	check a resource definition for a widget-definition, using our
//	extension to standard X syntax:
//
//		widgetType: widgetName[:parentName]
//
//	returns 1 if we find a definition, 0 if we don't, -1 on error
//
//  The 'createWidgets' flag tells us whether or not we should create widgets
//	on this pass (we're called twice, first with this flag FALSE, and then
//	TRUE).	We must do this since the resources that apply to objects must
//	all exist when the objects themselves are created.
//
//	We make an exception for Color objects, creating these on the first
//	pass, since they must exist before all other objects so that they can
//	be applied to them.
//
//  I know, this is ugly, but believe me, the sequencing is important!
//

static int defineWidget(char *buffer, int line, char *file, int createWidgets)
{
	int retcode = 0;
	char tmpBuffer[1024];

	// trim the buffer

	buffer = CUI_lrtrim(buffer);

    // if we have a wildcard spec, return 0 (no match)

	if(*buffer == '*')
		return(0);

	// extract first part of specification into tmpBuffer

	char *from, *to;
	for(from = buffer, to = tmpBuffer;
		*from && *from != '.' && *from != ':';
		*to++ = *from++)
	{
		if(!*from)
		{
			ResourceTable::formatError(line, file);
			retcode = -1;
		}
	}
	*to = 0;
	from++; // skip past the '.' or ':'

	// if first part is Widget typeName...

	CUI_WidgetId id = Widget::lookupName(tmpBuffer);
	if(id != CUI_NULL_ID)
	{
		// if separator is not ':', it's not a widget definition; tell caller

		if(*(from - 1) != ':')
			return(0);

		// it is a widget definition - handle Color objects as exception

		if(id == CUI_COLOR_ID)
		{
			// if 'createWidgets' is TRUE, we've already created
			// else fall through to create on this pass

			if(createWidgets)
				return(1);
		}
		else
		{
			// for all other types, don't create if flag is FALSE

			if(!createWidgets)
				return(1);
		}

		// we're defining a widget - get name (remainder of line)

		strcpy(tmpBuffer, CUI_ltrim(from));

		char *widgetName = tmpBuffer;

		for ( char* parentName = widgetName; *parentName; ++parentName )
			if ( *parentName == ':' ) {
				*parentName++ = 0;
				break;
			}

		// create widget as specified and check for failure

		CUI_Widget parent = Widget::lookup(parentName);
		if(!CUI_createWidget(widgetName, id, parent, NULL))
			CUI_fatal(dgettext( CUI_MESSAGES, "Can't create '%s' widget"),
					  Widget::typeName(id));

		return(1); // success
    }

	// no match - return retcode

	return(retcode);
}


//
//	determine whether resource expects numeric value
//

static bool isNumeric(CUI_StringId resource)
{
	switch(resource)
	{
        case borderWidthId:
        case colId:
        case granularityId:
		case hPadId:
		case hSpaceId:
		case heightId:
		case maxId:
		case measureId:
		case minId:
		case rowId:
		case sliderMaxId:
		case sliderMinId:
        case sliderValueId:
		case spaceId:
		case ticksId:
        case vPadId:
		case vSpaceId:
		case widthId:
		case xId:
		case yId:
		case tabKeyId:
		case stabKeyId:
		case leftKeyId:
		case rightKeyId:
		case upKeyId:
		case downKeyId:
		case pgUpKeyId:
		case pgDownKeyId:
		case homeKeyId:
		case endKeyId:
		case helpKeyId:
		case insKeyId:
		case delKeyId:
		case cancelKeyId:
		case refreshKeyId:
            return(TRUE);
		default:
			return(FALSE);
    }
}


//=============================================================================
//	Resource Table Entry
//=============================================================================


//
//	does specified resource match this entry?
//		rName = simple resource name			(eg 'label')
//		iName = fully-qualified instance name	(eg 'myapp.mybox.quit')
//		cName = fully-qualified class name		(eg 'Myapp.Box.Pushbutton')
//
//	returns precedence value of match: < 0 = no match, else element at
//	which we first matched (0 == pure wildcard...)
//

int RestabEntry::match(CUI_StringId rName, char *cName, char *iName)
{
	// if name portion of resource string doesn't match, fail immediately

	if((int)rName != name)
		return(-1);

	// else compare against cName and iName

	int cPrecedence = compare(resource, cName);
	int iPrecedence = compare(resource, iName);

	if(cPrecedence > iPrecedence)
		return(cPrecedence);
	else
		return(iPrecedence);
}


#ifdef TEST

//
//	for debugging, print entry
//

void RestabEntry::printOn(FILE *fd)
{
	char *stringName  = CUI_lookupString(name);
	char *stringValue = CUI_lookupString(value);
	fprintf(fd, "%s\t%s\t%s\n", resource, stringName, stringValue);
}

#endif // TEST


//=============================================================================
//	Resource Table
//=============================================================================


//
// constructor & destructor
//

ResourceTable::ResourceTable(char *name)
{
	// initialize the table

	MEMHINT();
    entries   = (RestabEntry **)CUI_malloc(BUMP * sizeof(RestabEntry *));
	arraySize = BUMP;
	used	  = 0;

	// save application name

	MEMHINT();
	appName = CUI_newString(name);
}

ResourceTable::~ResourceTable(void)
{
	MEMHINT();
    CUI_free(appName);
	for(int i = 0; i < used; i++)
	{
		MEMHINT();
        delete(entries[i]);
	}
	MEMHINT();
    CUI_free(entries);
}


//
//	load resources from resource files
//		load $CUIHOME/lib/cui.cui (if that fails, try ./cui.cui)
//		and then (if CUI_noResFile is FALSE)
//		$CUIHOME/lib/appName.cui (or ./appName.cui)
//	(we're called exactly twice; once not to create widgets, and then
//	to do so)
//
//	!!! should add a mechanism for a local resource !!!
//	!!! file that supplements the global ones		!!!
//

int ResourceTable::load(int createWidgets)
{
	char *dirName  = (char *)CUI_malloc(257);
	char *fileName = (char *)CUI_malloc(257);

	int ret = 0;
	char *home = getenv("CUIHOME");
	if(!home)
		home = CUI_DEFAULT_HOME;
	sprintf(dirName, "%s/lib", home);

	// if we're not creating widgets, load the global cui.cui file,
	// (first try in the 'official location', and if that fails,
	// try the current directory)

	if(!createWidgets)
	{
		sprintf(fileName, "%s/%s", dirName, "cui.cui");
		if(load(fileName, FALSE) != 0)
			load("cui.cui", FALSE);
	}

	// next load the application-specific file, passing on the
	// createWidgets flag (first try the 'official location',
	// and if that fails, try the current directory)

	if(appName && !CUI_noResfile)
    {
		char *appFileName = (char *)CUI_malloc(128);
		sprintf(appFileName, "%s.cui", appName);
		appFileName[0] = tolower(appFileName[0]);

		sprintf(fileName, "%s/%s", dirName, appFileName);
		ret = load(fileName, createWidgets);
		if(ret != 0)
			ret = load(appFileName, createWidgets);
		CUI_free(appFileName);
    }
	CUI_free(fileName);
	CUI_free(dirName);
	return(ret);
}


//
//  load entries from 'file' that apply to this application
//

int ResourceTable::load(char *file, int createWidgets)
{
	// open the file and handle error

	int retcode = 0;
	CUI_errno	= 0;
    FILE *fin = fopen(file, "r");
	if(!fin)
	{
		CUI_errno = E_OPEN;
		retcode = -1;
	}
	else
	{
		// read and process lines from the file

		for(int line = 1; ; line++)
		{
			char *ptr = fgets(CUI_buffer, TMP_BUFF_LEN, fin);
			if(ptr == NULL)
			{
				if(feof(fin))
					break;
				else
				{
					CUI_errno = E_READ;
					return(-1);
				}
			}
			int ret = processLine(CUI_buffer, line, file, createWidgets);
			if(ret)
				retcode = ret;
		}
		fclose(fin);
	}
	return(retcode);
}


//
//	process line from resource file
//

int ResourceTable::processLine(char *buffer, int line, char *file, int createWidgets)
{
	// anything to do?

	if(*buffer == '!' || CUI_strempty(buffer))
		return(0);

	// check for a Widget definition (CUI extension to X resource-file syntax)

	switch(defineWidget(buffer, line, file, createWidgets))
	{
		case -1:
			return(-1); // error
		case 1:
			return(0);	// we found a Widget definition - return success
	}

	// We are creating widgets on this pass, so don't load
	//   resources, we assume that this has been done.

	if ( createWidgets )
		return 0;

	// break line into resource and value parts (separated by ':')

    char *colon = strchr(buffer, ':');
	if(!colon)
		return(formatError(line, file));
	*colon++ = 0;

	int  value		   = 0;
	CUI_StringId name  = nullStringId;
	char *resource	   = CUI_lrtrim(buffer);
	char *strValue	   = CUI_lrtrim(colon);

	// strip any quotes from the value part

	if(strValue[0] == '"')
		strValue++;
	int len = strlen(strValue);
	if(strValue[len - 1] == '"')
		strValue[len - 1] = 0;

	// sanity check for at least one separator char in resource part

	if(strchr(resource, '.') == NULL && strchr(resource, '*') == NULL)
		return(formatError(line, file));

	// split resource string into resource and name parts (name is compiled)

	split(resource, resource, name);

	// if resource is numeric, convert strValue to number, else compile

	if(isNumeric(name))
	{
		if(sscanf(strValue, "%d", &value) == 0)
			CUI_fatal(dgettext( CUI_MESSAGES, "Numeric value expected at line %d in file %s"),
					  line, file);
	}
	else
	{
		// certain string resources must be translated...

#ifdef INTRINSIC_TRANSLATION
// The translation is being left entirely up to the application, except 
// for explicit message sgenerated by the library.
// dvb 7/22
        switch(name)
		{
			case labelId:
			case minLabelId:
			case maxLabelId:
			case titleId:
			{
				strValue = dgettext( CUI_MESSAGES, strValue);
				break;
			}
		}
#endif
		value = CUI_compileString(strValue);
	}

    // if resource is already in table, replace its value

    for(int i = 0; i < used; i++)
	{
		if(strcmp(resource, entries[i]->resource) == 0 &&
		   name == entries[i]->name)
		{
			entries[i]->value = value;
			return(0);
		}
	}

	// else we must append a new entry (grow array if necessary)

	if(used == arraySize - 1)
	{
		entries   = (RestabEntry **)CUI_arrayGrow((char **)entries, BUMP);
		arraySize += BUMP;
	}
	MEMHINT();
    entries[used++] = new RestabEntry(resource, name, value);
	return(0);
}


#ifdef TEST

//
//	for debugging, print out contents of table
//

void ResourceTable::printOn(FILE *fd)
{
	for(int i = 0; i < used; i++)
		entries[i]->printOn(fd);
}

#endif // TEST


//
//	report error in resource file
//

int ResourceTable::formatError(int line, char *file)
{
	CUI_warning(dgettext( CUI_MESSAGES, "Bad format in line %d of resource file %s\r\n"),
				line, file);
	return(-1);
}


//
//	return value for specified resource, or NULL if no matching entry
//
//	we weed out returned values based on precedences, so we end up
//	saving only items with equal precedence based on the 1st precedence
//	rule (element number at which we first matched)
//

int ResourceTable::lookup(CUI_StringId rName, char *cName, char *iName,
						  int &retval)
{
	int matches[MAX_MATCHES];
	int found = 0;
	int i;
	int precedence = -1;

	for(i = 0; i < used; i++)
	{
		// if no more room in array

		if(found == MAX_MATCHES)
			CUI_fatal(dgettext( CUI_MESSAGES, "Overflow in resource-precedence table"));

		// call the match routine, and save returned precedence

		int tmp = entries[i]->match(rName, cName, iName);

		// if returned precedence is >= 0, we have a match

		if(tmp >= 0)
		{
			// if precedence of this match is equal to highest precedence
			// we've found so far, add this entry to table of saved matches

			if(tmp == precedence)
				matches[found++] = i;

			// else if precedence is higher than previous highest precedence,
			// throw away all previous entries and save this one

			else if(tmp > precedence)
			{
				precedence = tmp;
				matches[0] = i;
				found = 1;
			}
			// else precedence is lower than previous matches; do nothing
		}
    }

	// if any matches, apply remaining precedence rules to select a single value

    if(found)
	{
		// arbitrarily assume match[0] has highest precedence

		int highest = 0;

		// compare highest precedence element with all the other matches

		for(i = 1; i < found; i++)
		{
			// pre-strip everything up to the level at which we matched...

			// reset 'highest' if current item has higher precedence

			if(hasPrecedence(entries[matches[i]]->resource,
							 entries[highest]->resource))
			{
				highest = i;
			}
		}
		retval = entries[matches[highest]]->value;
		return(0);
	}
	return(-1); // not found
}

