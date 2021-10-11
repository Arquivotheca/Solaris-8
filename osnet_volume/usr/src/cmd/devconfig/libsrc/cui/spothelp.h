#pragma ident "@(#)spothelp.h   1.3     92/11/25 SMI"

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
//	SpotHelp class definitions
//
//	$RCSfile: spothelp.h $ $Revision: 1.14 $ $Date: 1992/09/12 15:16:40 $
//=============================================================================

#ifndef  _CUI_SPOTHELP_H
#define  _CUI_SPOTHELP_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_SYMTAB_H
#include "symtab.h"
#endif
#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif

#endif // PRE_COMPILED_HEADERS


class SpotHelp
{
    protected:

		static	bool	checkMode;
		static	bool	processingLinks;
		static	int 	lineNumber;
		static	bool	writeIndex;
		static	char  **links;
		static	int 	arraySize;
		static	int 	numLinks;
				Symtab *symtab;
				FILE   *fd;
				bool	hyperMode;

		FILE *SpotHelp::openHelpFile(char *file);
		FILE *SpotHelp::openIndex(char *file);
		void processIndexFile(FILE *indexFd);
		void processHelpFile(FILE *indexFd);

    public:

		SpotHelp(char *file, bool checkIt = FALSE);
		~SpotHelp(void);
		bool   status(void) 	{ return(fd != NULL); }
		bool   hypertext(void)	{ return(hyperMode);  }
		char   **lookup(char *key);
		bool   haveText(char *key);
        char   *lookup(char *key, char **title);
		char   *target(char *text);
		void   saveLinks(char *text);
        void   validateLinks(void);
};

#endif // _CUI_SPOTHELP_H

