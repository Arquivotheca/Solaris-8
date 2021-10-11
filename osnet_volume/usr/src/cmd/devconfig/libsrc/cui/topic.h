#pragma ident "@(#)topic.h   1.3     92/11/25 SMI"

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
//	Hypertext Link and Topic class definitions
//
//	$RCSfile: topic.h $ $Revision: 1.8 $ $Date: 1992/09/12 15:18:22 $
//=============================================================================

#ifndef _CUI_TOPIC_H
#define _CUI_TOPIC_H


class HypertextLink
{
	protected:

		int   linkLine; 		// line number in file
		short startCol; 		// starting column number in line
		short endCol;			// ending column number in line
		char  *linkText;		// copy of current link text

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		HypertextLink(int l, short s, short e, char *t)
		{
			linkLine = l;
			startCol = s;
			endCol	 = e;
			linkText = CUI_newString(t);
		}

		~HypertextLink(void)
		{
			CUI_free(linkText);
		}

		// class-specific routines

        int   line(void)                    { return(linkLine);             }
        short start(void)                   { return(startCol);             }
		short end(void) 					{ return(endCol);				}
		char  *text(void)					{ return(linkText); 			}
};

class HypertextTopic
{
	protected:

		char		   *keyText;			// text of this topic's key
        char           *titleText;          // text of this topic's title
		char		   **topicText; 		// text of the topic itself
		HypertextLink  **linksArray;		// array of HypertextLinks
		int 		   linkIndex;			// index into array
		int 		   arraySize;			// size of array
		bool		   foundTitle;			// did we find a title?
		int 		   lines;				// number of lines in text

		void		   findLinks(void);
        int            addLink(HypertextLink *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		HypertextTopic(char *key);
		~HypertextTopic(void);

		// class-specific routines

		static bool findLink(char *text, short &start, short &end, int line = 0);

		bool		  ok(void)				{ return(lines != 0);			}
		char		  *title(void)			{ return(titleText);			}
        char          **text(void)          { return(topicText);            }
		HypertextLink **links(void) 		{ return(linksArray);			}
		int 		  numLinks(void)		{ return(linkIndex);			}
		int 		  numLines(void)		{ return(lines);				}
};

#endif // _CUI_TOPIC_H

