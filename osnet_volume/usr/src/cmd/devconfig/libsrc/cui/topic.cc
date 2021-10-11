#pragma ident "@(#)topic.cc   1.4     93/07/23 SMI"

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
//	Hypertext Link and Topic class implementations
//
//	$RCSfile: topic.cc $ $Revision: 1.9 $ $Date: 1992/09/12 17:01:29 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_TOPIC_H
#include "topic.h"
#endif
#ifndef  _CUI_SPOTHELP_H
#include "spothelp.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// link start and end chars

#define LINK_START '['
#define LINK_END   ']'


// grow links array by this number of elements

#define BUMP 6

// the global SpotHelp object (this provides our text)

extern SpotHelp *CUI_spotHelp;


// messages

static char *noStart = dgettext( CUI_MESSAGES, "Link end without start in line %d:\n    %s\n");
static char *noEnd	 = dgettext( CUI_MESSAGES, "Link start without end in line %d:\n    %s\n");


//
//	constructor (from help key)
//

HypertextTopic::HypertextTopic(char *key)
{
	keyText 	= NULL;
	titleText	= NULL;
	topicText	= NULL;
	linksArray	= NULL;
	linkIndex	= 0;
	arraySize	= 0;
	lines		= 0;

	// if we have a SpotHelp object...

	if(CUI_spotHelp)
	{
		// save the key

		keyText = CUI_newString(key);

		// load the text

        topicText = CUI_spotHelp->lookup(key);
		if(topicText)
		{
			// if we have a title (1st line is not blank,
			// 2nd line is blank, and we have a 3rd line)...

			foundTitle = FALSE;
			if(CUI_arrayCount(topicText) >= 3 &&
			   *topicText[0] && CUI_strempty(topicText[1]))
			{
				// save title, bump start of array, set flag

				titleText = topicText[0];
				topicText += 2;
				foundTitle = TRUE;
            }

			// if we don't have a title, use key

			if(!titleText)
				titleText = keyText;

			// remember number of lines, and if we have any, find links

			lines = CUI_arrayCount(topicText);
			if(lines)
				findLinks();
        }
	}
}


HypertextTopic::~HypertextTopic(void)
{
	MEMHINT();
    CUI_free(keyText);

	// if we found a title in the text, bump pointer back to array start

	if(foundTitle)
		topicText -= 2;

	// delete the text array

	MEMHINT();
    CUI_deleteArray(topicText);

	// if we have any links, get rid of them, and their array

	if(linkIndex)
	{
		for(int i = 0; i < linkIndex; i++)
		{
			MEMHINT();
            delete(linksArray[i]);
		}
	}
	MEMHINT();
    delete(linksArray);
}


//
//	find all the links in our text and add them to our array
//

void HypertextTopic::findLinks(void)
{
	for(int i = 0; i < lines; i++)
	{
		char *text = topicText[i];
		char *ptr  = text;
		while(1)
		{
			short start = -1;
			short end	= -1;

			bool found = findLink(ptr, start, end);
			if(found)
			{
				// make start and end relative to start of line

				short adjust = (short)(ptr - text);
				start += adjust;
				end   += adjust;

				// create new link and add to our array

				int len = end - start + 1;
				strncpy(CUI_buffer, text + start, len);
				CUI_buffer[len] = 0;
				HypertextLink *link = new HypertextLink(i, start,
										  end, CUI_buffer);
				addLink(link);

				// bump pointer past end and try again (unless end of string)

				ptr += end + 1;
				if(!(*ptr))
					break;
			}
			else // didn't find a link - we're done
			{
				break;
			}
		} // look for next link in line

	} // do next line
}


//
//	add a Link to our array
//

int HypertextTopic::addLink(HypertextLink *link)
{
	// allocate array if necessary

	if(!linksArray)
	{
		arraySize  = BUMP;
		int size   = arraySize * sizeof(HypertextLink *);
		MEMHINT();
        linksArray = (HypertextLink **) CUI_malloc(size);
	}

	// else grow array if necessary

	else if(linkIndex == arraySize - 1)  // keep final NULL!
	{
		MEMHINT();
        linksArray = (HypertextLink **) CUI_arrayGrow((char **)linksArray, BUMP);
		arraySize += BUMP;
    }

	// save topic

	linksArray[linkIndex++] = link;

	return(0);
}


//
//	given a string, find first hypertext link in it (if any)
//	(returns TRUE if found, setting indexes of start and end of link)
//
//	We may optionally be passed a line-number parameter for diagnostics
//	(if we're not, we silently ignore malformed entries in the file)

bool HypertextTopic::findLink(char *text, short &start, short &end, int line)
{
	start = end = -1;

	for(int i = 0; i < strlen(text); i++)
	{
		switch(text[i])
		{
			case '\\':
			{
				// step over this char (we'll step over next at end of loop)

				i++;
				break;
			}
			case LINK_START:
			{
				start = (short)i;
				break;
			}
			case LINK_END:
			{
				if(start != -1)
				{
					end = (short)i;
					return(TRUE);
				}
				else
				{
					if(line)
						printf(noEnd, line, text);
                    return(FALSE);
				}
			}
			default:
				break;
		}

	} // do next char in line

	if(start != -1)
	{
		if(line)
			printf(noStart, line, text);
    }
	return(FALSE);
}

