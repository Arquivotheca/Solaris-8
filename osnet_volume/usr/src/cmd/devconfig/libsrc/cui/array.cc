#pragma ident "@(#)array.cc   1.4     92/11/25 SMI"

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
//	dynamic array routines
//
//	$RCSfile: array.cc $ $Revision: 1.8 $ $Date: 1992/09/12 15:21:07 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// under CenterLine C++, unlink is in sysent.h - copy the prototype here

extern "C"
{
int unlink(const char*);
}


#define BUMP	  32	// bump arrays by this number of elements
#define FILE_MODE 1 	// read from regular file
#define HELP_MODE 2 	// read from help file


char CUI_buffer[TMP_BUFF_LEN + 1];


//
//	local function prototypes
//

static int strLength(char *string);


//
//	read file (or help text) into null-terminated array
//	(file parameter is filename or help-file handle,
//	mode parameter is FILE_MODE or HELP_MODE)
//

static char **newArray(void *file, int mode)
{
	int  i = 0;
	int  lines;
    char *ptr;
    FILE *fin;

	if(mode == HELP_MODE)
    {
        fin = (FILE *)file;
    }
	else // FILE_MODE
    {
		if((fin = fopen((char *)file, "r")) == 0)
        {
            CUI_errno = E_OPEN;
            return(NULL);
        }
    }

	// initialize the array

	MEMHINT();
	char **array = (char **)CUI_malloc(BUMP * sizeof(char *));
	for(i = 0; i < BUMP; i++)
        array[i] = NULL;
	lines = BUMP;
    i = 0;

	// read in data from file

    while(1)
    {
		char newBuffer[256];

		// grow array if necessary 

		if(i == lines - 1)	  // leave a NULL on end, so we know how big
        {
			array = CUI_arrayGrow(array, BUMP);
            if(array == NULL)
                return(NULL);
            else
				lines = lines + BUMP;
        }

		// read next line

		ptr = fgets(CUI_buffer, TMP_BUFF_LEN, fin);
        if(ptr == NULL)
        {
            if(feof(fin))
            {
				array[i] = NULL;	// mark end of array
                break;
            }
            else
            {
				CUI_errno = E_READ;
				CUI_deleteArray(array);
                return(NULL);
            }
        }

		// in help mode we can ignore some lines right now

		if(mode == HELP_MODE)
        {
			if(CUI_buffer[0] == ':') // next topic...
			{
				array[i] = NULL;		// mark end of array
				break;
			}
			if(CUI_buffer[0] == '#')     // skip comment...
				continue;
        }

		// expand tabs

		int col = 0;
		char *from = CUI_buffer;
		char *to   = newBuffer;

		while(*from && col < 250)
		{
			switch(*from)
			{
				case '\t':
				{
					int nextTab = (col + 8) - (col % 8);
					while(col < nextTab)
					{
						*to++ = ' ';
						col++;
					}
					break;
				}
				default:
				{
					*to++ = *from;
					col++;
					break;
				}
			}
			from++;
		}
		*to = 0;

		// strip trailing spaces (and newline)

		CUI_rtrim(newBuffer);

		// malloc the buffer and copy line to it

		MEMHINT();
		array[i] = (char *)CUI_malloc(strlen(newBuffer) + 1);
		strcpy(array[i++], newBuffer);
    }

	if(mode != HELP_MODE)
        fclose(fin);

	// zap trailing blank lines (leading blank lines aren't stripped!)

    if(i)
    {
		for(--i; ; i--)
        {
			if(strlen(array[i]) == 0)
			{
				MEMHINT();
                CUI_free(array[i]);
				array[i] = NULL;
			}
			else
				break;
        }
    }
    else
		i = -1;

	// anything left?

    if(i < 0)
    {
		MEMHINT();
        CUI_free(array);
		CUI_errno = E_NULL;
        return(NULL);
    }
    else
        return(array);
}


//
//	construct array from file
// 

char **CUI_fileToArray(char *file)
{
	return(newArray((void *)file, FILE_MODE));
}


//
//	construct array from help text
//

char **CUI_helpToArray(FILE *file)
{
	return(newArray((void *)file, HELP_MODE));
}


//
//	 construct array from buffer
//	 (lazy! we write the buffer to a temp file and read it back)
//

char **CUI_buffToArray(char *buffer)
{
	char   **array;
	FILE   *fd;
	char   *tmpfile = tmpnam(NULL);

    fd = fopen(tmpfile, "w");
    if(fd == NULL)
	{
		CUI_errno = E_OPEN;
        return(NULL);
	}
	fprintf(fd, buffer);
    fclose(fd);
	array = CUI_fileToArray(tmpfile);
    unlink(tmpfile);
    return(array);
}


//
//	delete array
//

void CUI_deleteArray(char **array)
{
    if(array == NULL)
		return;
	for(int i = 0; array[i] != NULL; i++)
    {
		MEMHINT();
        CUI_free(array[i]);
        array[i] = NULL;
    }
	MEMHINT();
    CUI_free(array);
}


//
//	grow array by 'bump' elements
//

char **CUI_arrayGrow(char **array, int bump)
{
    int count, newcount, i;
	char **nextArray;

	count	  = CUI_arrayCount(array);
	newcount  = count + bump + 1;	/* count the NULL on the end... */
	MEMHINT();
    nextArray = (char **)CUI_malloc(newcount * sizeof(char *));
	for(i = 0; i < count; i++)
		nextArray[i] = array[i];
	for( ; i < newcount; i++)
		nextArray[i] = NULL;
	MEMHINT();
    CUI_free(array);
	return(nextArray);
}


//
//	count number of elements in array
//

int CUI_arrayCount(char **array)
{
	for(int i = 0; array[i] != NULL; i++)
		;
    return(i);
}


//
//	return maximum width of elements in array
//

int CUI_arrayWidth(char **array)
{
    int width, tmp;

    width = tmp = 0;
	for(int i = 0; array[i] != NULL; i++)
    {
		tmp = strLength(array[i]);	   // allow for embedded attributes
        if(tmp > width)
            width = tmp;
    }
    return(width);
}


//
//	calculate length of string, allowing for embedded attribute-change codes
//

static int strLength(char *string)
{
    char *next = string;
    int count = 0;
    int code;

    while(1)
    {
		next = strchr(next, ATTRIB_CHANGE);
		if(next == NULL)
			break;
		next++;
		code = *next++;
		if(code >= '0' && code <= '9')
			count++;
		else
			next--;
    }
    return(strlen(string) - (2 * count));
}

