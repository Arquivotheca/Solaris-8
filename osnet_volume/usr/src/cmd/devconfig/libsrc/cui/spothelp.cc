#pragma ident "@(#)spothelp.cc   1.9     93/09/15 SMI"

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

//============================== -*-Mode: c++;-*- =============================
//	SpotHelp class implementation
//
//	$RCSfile: spothelp.cc $ $Revision: 1.30 $ $Date: 1992/09/12 18:56:25 $
//=============================================================================

#include "precomp.h"

#ifndef  PRE_COMPILED_HEADERS

#include <stdlib.h>
#include <sys/stat.h>
#ifndef  _CUI_SPOTHELP_H
#include "spothelp.h"
#endif
#ifndef  _CUI_TOPIC_H
#include "topic.h"
#endif

#endif	// PRE_COMPILED_HEADERS


bool   SpotHelp::processingLinks = FALSE;
bool   SpotHelp::checkMode	= FALSE;
bool   SpotHelp::writeIndex = FALSE;
int    SpotHelp::lineNumber = 0;
char **SpotHelp::links = NULL;
int    SpotHelp::arraySize;
int    SpotHelp::numLinks;


#define BUMP 64 	// grow check-array by this number of elements


static FILE *indexFd = NULL;
static FILE *openIndex(char *path);


// messages

static char *checking =
		dgettext( CUI_MESSAGES, "\n======== checking link formats ========\n\n");

static char *reportTopic =
		dgettext( CUI_MESSAGES, "Checking topic '%s'...\n");

static char *noTarget1 =
	   dgettext( CUI_MESSAGES, "No target in index entry at line %d:\n     %s\n");

static char *noTarget2 =
	   dgettext( CUI_MESSAGES, "No target specified for link %s\n");

static char *noTopic =
	   dgettext( CUI_MESSAGES, "No topic found for target key '%s'\n");


//
//	constructor (in checkMode we build up an array of all the links
//	we find in the text for later validation, and also write an index)
//

SpotHelp::SpotHelp(char *file, bool checkIt)
{
	// initialize instance variables

	checkMode = checkIt;
	if(checkMode)
	{
		numLinks = 0;
		arraySize = BUMP;
		MEMHINT();
		links = (char **)CUI_malloc(arraySize * sizeof(char *));
	}
	fd = NULL;
	symtab = NULL;
	hyperMode = FALSE;

	// try to open the file(s)

	FILE *indexFd = openHelpFile(file);
	if(!fd)
		return;

    // create symbol table

	MEMHINT();
    symtab = new Symtab(257);

	// process index or help file appropriately

	if(!indexFd || checkMode || writeIndex)
	{
		if(checkMode)
			printf(checking, file);
		processHelpFile(indexFd);
	}
	else
		processIndexFile(indexFd);

	// close the index file (whether we're reading or writing, we're done)

	fclose(indexFd);
}


//
//	process index file
//	(even though the index file should be well-formed, we sanity-check
//	that we have valid pointers (though we don't report any errors)
//

void SpotHelp::processIndexFile(FILE *indexFd)
{
	//	read lines till EOF

	while(fgets(CUI_buffer, 200, indexFd))
    {
		// strip trailing newline, if any

		int len = strlen(CUI_buffer);
		if(CUI_buffer[len - 1] == '\n')
			CUI_buffer[len - 1] = 0;

		char firstCh = CUI_buffer[0];

		// if first char is '!' we're in hypertext mode

		if(firstCh == '!')
		{
			hyperMode = TRUE;
			continue;
		}

        // if first char is '[' we have a link-target specification

		if(firstCh == '[')
		{
			// find and skip past the closing ']'

			char *ptr = strchr(CUI_buffer, ']');
			if(!ptr)
				continue;

			// terminate this part of the string, and bump pointer

			*++ptr = 0;
			ptr++;

			// now the buffer contains the link, and ptr => target
			// insert into symtab

			if(!*ptr)
				continue;
			MEMHINT();
			char *target = CUI_newString(ptr);
			symtab->insert(CUI_buffer, (void *)target);
		}
		else // we have a topic specification
		{
			// find the space separating the index from the key
			// separate the two and bump pointer to the key

			char *ptr = strchr(CUI_buffer, ' ');
			if(!ptr)
				continue;
			*ptr++ = 0;
			if(!*ptr)
				continue;

			// insert into symtab

			long index = atol(CUI_buffer);
			symtab->insert(ptr, (void *)index);
        }
    }
}


//
//	process help file (we didn't find an index, or it was out of date)
//		(if we're passed an index file-descriptor,
//		we're in checkMode; write an index file)
//

void SpotHelp::processHelpFile(FILE *indexFd)
{
	static char dummy[201];

    //  read lines till EOF

	while(fgets(CUI_buffer, 200, fd))
    {
		lineNumber++;
		if(CUI_buffer[0] == '#') // comment
			continue;

		if(checkMode)
		{
			int len = strlen(CUI_buffer);
			if(CUI_buffer[len - 1] == '\n')
				CUI_buffer[len - 1] = 0;
		}

		// keys begin ':'

		if(CUI_buffer[0] == ':')
		{
			// we're no longer processing links (if indeed we were)

			processingLinks = FALSE;

#ifdef CHECKHELP_ONLY
			// skip the next line, since we know it is a title (no links)
			fgets( dummy, 200, fd);
#endif

			char *ptr = CUI_buffer + 1;
			if(!ptr)
				continue;	// nothing there

			// strip leading/trailing whitespace and convert to upper-case

			ptr = CUI_ltrim(ptr);
			CUI_rtrim(ptr);

			// anything left?

			if(CUI_strempty(ptr))
				continue;

			// in checkMode report what we're doing

			if(checkMode)
				printf(reportTopic, ptr);

            // convert to upper-case

            CUI_uppercase(ptr);

			// "!LINKS" is special-case

			if(strcmp(ptr, "!LINKS") == 0)
			{
				// switch into link-processing mode
				// (and set flag to say we have hypertext mode)

				processingLinks = TRUE;
				hyperMode = TRUE;

				// if we're writing an index file, save a token
				// so we'll know we're in hypertext mode

				if(indexFd)
					fprintf(indexFd, "!\n");
                continue;
			}

			// else it's a regular topic - save key and index in symtab
			// and write index if we have a file-descriptor

			else
			{
				long index = ftell(fd);
				symtab->insert(ptr, (void *)index);
				if(indexFd)
					fprintf(indexFd, "%ld %s\n", index, ptr);
			}
		}

		else // not a Topic - process links if we're in that mode

		{
			if(processingLinks)
			{
				// strip leading and trailing whitespace

				char *ptr = CUI_ltrim(CUI_buffer);
				CUI_rtrim(ptr);
				char *linkText = NULL;
				char *target = NULL;

				// check whether line contains a link

				short start = -1;
				short end	= -1;
				if(HypertextTopic::findLink(CUI_buffer, start, end, lineNumber))
				{
					// save and terminate linkText pointer

					linkText = CUI_buffer + start;
					CUI_buffer[end + 1] = 0;

					// strip leading whitespace from target
					// (skip the inserted 0)

					target = CUI_ltrim(CUI_buffer + end + 2);

					// if we have linkText and target, insert in symtab
					// and write index if we have a file-descriptor

					if(linkText && *target)
					{
						CUI_uppercase(linkText);

						// !!! string targets aren't freed !!!
						// !!! when we free the symtab	   !!!

						MEMHINT();
						target = CUI_newString(CUI_uppercase(target));
                        symtab->insert(linkText, (void *)target);
						if(indexFd)
							fprintf(indexFd, "%s %s\n", linkText, target);
					}
					else // no target
					{
						if(checkMode)
							printf(noTarget1, lineNumber, CUI_buffer);
					}
				}
			}
			else // not processing links - in checkMode, save our links
			{
				if(checkMode)
					saveLinks(CUI_buffer);
			}
		}
    }
}


//
// destructor
//

SpotHelp::~SpotHelp(void)
{
	if(fd)
		fclose(fd);
	if(symtab)
	{
		symtab->removeAll();
		MEMHINT();
		delete(symtab);
	}
	for(int i = 0; i < numLinks; i++)
		CUI_free(links[i]);
	CUI_free(links);
}


//
//	lookup help text by key and return in newly-allocated array
//

char **SpotHelp::lookup(char *key)
{
	char buffer[120];
	strcpy(buffer, key);

	// lookup key in symtab

	long index = (long)symtab->lookup(CUI_uppercase(buffer));
	if(!index)
		return(NULL);

	// seek to index and read help text into array

	fseek(fd, index, 0);
	return(CUI_helpToArray(fd));
}


//
//	lookup help text by key and return TRUE if found, else FALSE
//

bool SpotHelp::haveText(char *key)
{
	char buffer[120];
	strcpy(buffer, key);

	// lookup key in symtab

	long index = (long)symtab->lookup(CUI_uppercase(buffer));
	if(!index)
		return(FALSE);
	else
		return(TRUE);
}


//
//	get target of specified linkText
//

char *SpotHelp::target(char *text)
{
	char buffer[120];
	strcpy(buffer, text);

    // lookup text in symtab

	char *target = (char *)symtab->lookup(CUI_uppercase(buffer));
	return(target);
}


//
//	lookup help text by key and return it and title in newly-allocated buffer
//

char *SpotHelp::lookup(char *key, char**title)
{
	char **array = lookup(key);
	if(!array)
		return(NULL);

	// if we have a title
	// (1st line is not blank, 2nd line  is blank, and we have a 3rd line)
	// copy first line to newly-allocated string

	char *titlePtr = NULL;
	int  firstLine = 0;
	if(CUI_arrayCount(array) >= 3 && *array[0] && CUI_strempty(array[1]))
	{	
		MEMHINT();
        titlePtr = CUI_newString(array[0]);
		firstLine = 2;
	}

	// calculate size of required buffer, and copy text into it

	int len = 0;
	for(int i = firstLine; array[i]; i++)
		len += strlen(array[i]) + 1;

	// allocate a buffer and copy text into it

	MEMHINT();
	char *buffer = (char *)CUI_malloc(len);
	char *ptr = buffer;
	for(i = firstLine; array[i]; i++)
	{
		sprintf(ptr, "%s\n", array[i]);
		ptr += strlen(array[i]) + 1;
	}

	// delete the array and return the buffer and title

	CUI_deleteArray(array);
	if(title)
		*title = titlePtr;
	return(buffer);
}


//
//	open SpotHelp file for application (taking locale into account)
//		sets instance variable 'fd' to file descriptor for help file
//		if we succeed, and returns file descriptor for index if we
//		find one and it's newer than the help file, or if we're in
//		checkMode (in which case we're writing the index)
//

#include <locale.h>
FILE *SpotHelp::openHelpFile(char *file)
{
	char *dirName  = (char *)CUI_malloc(257);

    char *home = getenv("CUIHOME");
	if(!home)
		home = CUI_DEFAULT_HOME;
    char *locale = getenv("LANG");
	if (!locale) locale="C";
	sprintf(dirName, "%s/lib/locale/%s/help", home, locale);

	// first look in $CUIHOME/lib/locale/<localename>/help

	sprintf(CUI_buffer, "%s/%s", dirName, file);
	CUI_free(dirName);
    FILE *indexFd = NULL;
    indexFd = openIndex(CUI_buffer);
	if(fd)
		return(indexFd);

    // if that didn't work, try again with locale 'C'
	// (not all locales will have help files)
 
    sprintf(CUI_buffer, "%s/lib/locale/C/help/%s", home, file);
    indexFd = openIndex(CUI_buffer);
    if(fd)
        return(indexFd);

	// that didn't work - try current directory

	indexFd = openIndex(file);
	if(fd)
		return(indexFd);

	// we don't have a help file...

	return(NULL);
}


//
//	try to open index or help file
//		the instance variable 'fd' is set to file descriptor for help file
//		if we find it, and the returned file descriptor is for the index
//		file if we find one and it's newer than the help file, or we're
//		in checkMode (in which case we're writing the index)
//

FILE *SpotHelp::openIndex(char *file)
{
	struct stat statbuff;
	char buffer[256];
	long indexTime = -1L;
	long helpTime  = -1L;
	FILE *indexFd  = NULL;

	// first we stat the help file:
	//	  return failure immediately if not found,
	//	  else open it and save modification time

	sprintf(buffer, "%s.info", file);
	if(stat(buffer, &statbuff) != 0)
		return(NULL);
	else
	{
		fd = fopen(buffer, "r");
		if(!fd)
			return(NULL);
        helpTime = statbuff.st_mtime;
	}

	// construct index file name
	// stat the file and save its modification time

    sprintf(buffer, "%s.ndx", file);
	if(stat(buffer, &statbuff) == 0)
		indexTime = statbuff.st_mtime;

	// if index file is newer than help file, open for read and return fd

	writeIndex = FALSE;
    if(indexTime > helpTime)
	{
		indexFd = fopen(buffer, "r");
		return(indexFd);
	}

	// we didn't find an index file, or it's out of date;
	// open it for write and return its fd

	indexFd = fopen(buffer, "w");
	if(indexFd)
		writeIndex = TRUE;
    return(indexFd);
}


//
//	look for links in the text, and make copies in our array (checkMode only)
//

void SpotHelp::saveLinks(char *text)
{
	char buffer[200];

	if(!checkMode)
		return;

	short start, end;
    while(HypertextTopic::findLink(text, start, end, lineNumber))
	{
		// copy the text of the link

        int len = end - start + 1;
		strncpy(buffer, text + start, len);
		buffer[len] = 0;

		// have we saved this link already?

		bool saved = FALSE;
		for(int i = 0; i < numLinks; i++)
		{
			if(strcmp(buffer, links[i]) == 0)
			{
				saved = TRUE;
				break;
			}
		}
		if(!saved)
		{
			// grow strings array if necessary

			if(numLinks == arraySize - 1)  // keep final NULL!
			{
				links = CUI_arrayGrow(links, BUMP);
				arraySize += BUMP;
			}

			// insert copy of string into array

			links[numLinks++] = CUI_newString(buffer);
		}

		// bump pointer past this link and check for another

        text += (end + 1);
		if(!*text)
			break;
	}
}


//
//	validate links to check that they all have targets
//	(in checkMode only)
//

void SpotHelp::validateLinks(void)
{
	if(!checkMode)
		return;

	for(int i = 0; i < numLinks; i++)
	{
		char *targetKey = target(links[i]);
		if(!targetKey)
			printf(noTarget2, links[i]);
		else
		{
			if(!haveText(targetKey))
				printf(noTopic, targetKey);
		}
	}
}

