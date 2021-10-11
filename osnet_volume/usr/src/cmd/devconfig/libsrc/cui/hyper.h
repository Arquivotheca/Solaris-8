#pragma ident "@(#)hyper.h   1.5     93/01/08 SMI" 

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
//	HypertextPanel class definition
//
//	$RCSfile: hyper.h $ $Revision: 1.2 $ $Date: 1992/12/31 00:27:20 $
//=============================================================================


#ifndef _CUI_HYPERPANEL_H
#define _CUI_HYPERPANEL_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_TEXTPANEL_H
#include "txtpanel.h"
#endif
#ifndef  _CUI_TOPIC_H
#include "topic.h"
#endif

#endif // PRE_COMPILED_HEADERS



//
//	TopicInfo class is used to store info about visited topics,
//	so we can retrace our steps
//

class TopicInfo
{
	protected:

        char  *topicKey;        // key for topic
		int   firstLine;		// first line number in panel
		int   linkLine; 		// line number of link
		int   linkCol;			// column number of line

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		TopicInfo(char *key, int fLine = 0, int lLine = -1, int lCol = 0)
		{
			topicKey  = CUI_newString(key);
			firstLine = fLine;
			linkLine  = lLine;
			linkCol   = lCol;
		}

		~TopicInfo(void)
		{
			CUI_free(topicKey);
		}

		// class-specific routines

		int  &first(void)					{ return(firstLine);			}
		int  &line(void)					{ return(linkLine); 			}
		int  &col(void) 					{ return(linkCol);				}
		char *key(void) 					{ return(topicKey); 			}
};


//
//	the HypertextPanel does all the hypertext management and display
//	(most of the basic text-display logic is inherited from TextPanel)
//

class HypertextPanel : public TextPanel
{
	protected:

		TopicInfo **topics; 				// array of visited topics
		int 	  topicIndex;				// index into array
		int 	  arraySize;				// size of array
		HypertextTopic *topic;				// current topic
		HypertextLink  *link;				// current link
		static HypertextLink *savedLink;	// for link save/restore

		bool  inLink(void);
		int   nextLink(bool showFocus);
		int   prevLink(void);
        int   showLink(bool showFocus);
		int   showAllLinks(void);
        void  leaveLink(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		HypertextPanel(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					   CUI_WidgetId id = CUI_HYPERPANEL_ID);
		virtual ~HypertextPanel(void);

        // utility routines

		virtual int isKindOf(CUI_WidgetId type)
		  { return(type == CUI_HYPERPANEL_ID || TextPanel::isKindOf(type)); }
#ifdef WAS
		virtual void locateCursor(void) 		{ syncCursor(); 			}
#else
		virtual void locateCursor(void) 		{ CUI_doRefresh = FALSE;	}
#endif

		// access methods for elements of current TopicInfo

		int &topicFirstLine(void)
								{ return(topics[topicIndex - 1]->first());	}
		int &topicLine(void)
								{ return(topics[topicIndex - 1]->line());	}
		int &topicColumn(void)
								{ return(topics[topicIndex - 1]->col());	}
		char *topicKey(void)
		  { return(topicIndex > 0 ? topics[topicIndex - 1]->key() : NULL);	}

		// class-specific routines

		int  showTopic(char *key, bool push = TRUE);
		int  pushTopic(char *key);
		int  popTopic(void);
		int  followLink(void);
		int  prevTopic(void);
        int  showFirstLink(void);
		void saveLink(void) 					{ savedLink = link; 		}
		void restoreLink(void);
		void focusIfInLink(void);
        void exitHelp();

		// message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual int focus(bool reset = TRUE);

		// show is needed! (we bypass some of TextPanel's logic)

		virtual int show(void)				 { return(ControlArea::show()); }
};

#endif // _CUI_HYPERPANEL_H

