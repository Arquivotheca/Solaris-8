#pragma ident "@(#)cui.cc   1.5     93/08/03 SMI"

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

/*=============================================================================
 *	generic application driver
 *
 *	$RCSfile: cui.cc $ $Revision: 1.5 $ $Date: 1992/09/12 15:22:42 $
 *===========================================================================*/


#include "cui.h"
#include <locale.h>


/* BaseWindow pointer */

CUI_Widget window = NULL;


int main(int argc, char *argv[])
{
	/* check args */

	setlocale ( LC_ALL, "" );
	textdomain("SUNW_INSTALL_CUITEST");
	if(argc != 2)
		CUI_fatal(gettext( "Usage: cui appName"));

	/* print version */

	int major, minor;
	CUI_version(&major, &minor);
	printf(gettext( "cui driver program - version %d.%d\n"),
		   major, minor);

	/* create and realize BaseWindow */

	window = CUI_initialize(argv[1], NULL, &argc, argv);
	if(!window)
		CUI_fatal(gettext( "Can't initialize application"));

#if 0
	CUI_Widget x;
	x = CUI_createWidget("name", CUI_CAPTION_ID, window, NULL);
	CUI_createWidget("nameField", CUI_TEXTFIELD_ID, x, NULL);

	x = CUI_createWidget("age", CUI_CAPTION_ID, window, NULL);
	CUI_createWidget("areField", CUI_NUMFIELD_ID, x, NULL);

	CUI_createWidget("exit", CUI_OBLONGBUTTON_ID, window, NULL);
#endif

	CUI_realizeWidget(window);
	if(CUI_errno == E_OPEN)
		CUI_fatal(gettext( "Can't open resource file for application %s"), argv[1]);

	/* process events... */

	CUI_mainLoop();

	/* clean up and exit */

	CUI_exit(window);
    return(0);
}

