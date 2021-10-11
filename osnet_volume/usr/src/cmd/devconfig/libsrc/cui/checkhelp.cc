#pragma ident "@(#)checkhelp.cc   1.4     93/07/22 SMI"

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

//
//	check a Help file for consistency
//

#include "spothelp.h"

static char *validating =
		dgettext(CUI_MESSAGES, 
		"\n======== validating link targets ========\n\n");

int main(int argc, char *argv[])
{
	// check args

	if(argc != 2)
		CUI_fatal(dgettext(CUI_MESSAGES, "Usage: checkhelp helpfile-name"));

	// construct a SpotHelp object from our file in check mode

	SpotHelp *spotHelp = new SpotHelp(argv[1], TRUE);
	if(!spotHelp->status())
		CUI_fatal(dgettext(CUI_MESSAGES, "Can't open helpfile '%s'"), argv[1]);

	// tell the SpotHelp object to validate links

	printf(validating);
	spotHelp->validateLinks();

	// delete it

	delete(spotHelp);
	return(0);
}


