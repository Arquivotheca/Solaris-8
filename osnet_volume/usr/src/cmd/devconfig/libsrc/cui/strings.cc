#pragma ident "@(#)strings.cc   1.3     92/11/25 SMI"

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
//	string routines
//
//	$RCSfile: strings.cc $ $Revision: 1.6 $ $Date: 1992/09/12 15:23:50 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#define isWhite(c)	(c == ' ' || c == '\t' || c == '\n')



//
//	 convert strings to upper and lower case (in place)
//

char *CUI_uppercase(char *string)
{
	for(char *ptr = string; *ptr; ptr++)
	{
		if(isascii(*ptr))
			*ptr = toupper(*ptr);
	}
	return(string);
}

char *CUI_lowercase(char *string)
{
	char *ptr;
	for(ptr = string; *ptr; ptr++)
	{
		if(isascii(*ptr))
			*ptr = tolower(*ptr);
	}
	return(string);
}


//
//	find substring in a string
//

char *CUI_substr(char *string, char *sub)
{
	// check for null strings

    if(*string == 0 || *sub == 0)
		return(NULL);

	// search for 1st char of substring

	char *p1 = string;
	char *save = p1;

    while(1)
    {
        p1 = save;
		char ch = *sub | 0x20;
		while( (ch = *p1++ | 0x20) != ch )
        {
            if (*p1 == 0)
            return(NULL);
        }

        save = p1--;

		// see if they match

		char *p2 = sub;
		while( (ch = *p1++ | 0x20) == (ch = *p2++ | 0x20) )
            if (*p2 == 0)
        return(save);
    }
}


//
//	determine whether a string is empty (NULL, zero-length, or all blank)
//	returns 0 if not empty, 1 if empty
//

int CUI_strempty(char *string)
{
    if(string == NULL)
		return(1);
    if(*string == 0)
		return(1);
    while(*string)
    {
		if(!isWhite(*string))
			return(0);
		string++;
    }
    return(1);
}


//
//	trim leading/trailing/leading & trailing whitespace from string (in place)
//

char *CUI_ltrim(char *string)
{
	while(isWhite(*string))
		string++;
	return(string);
}

char *CUI_rtrim(char *string)
{
	int len = strlen(string) -1;
	char *tmp = string + len;
	while(isWhite(*tmp) && tmp >= string)
		*tmp-- = 0;
    return(string);
}

char *CUI_lrtrim(char *string)
{
	return(CUI_ltrim(CUI_rtrim(string)));
}


