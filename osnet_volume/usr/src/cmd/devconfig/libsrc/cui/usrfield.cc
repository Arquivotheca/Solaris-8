#pragma ident "@(#)usrfield.cc   1.3     92/11/25 SMI"

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
//	user-defined ETI fields
//
//	$RCSfile: usrfield.cc $ $Revision: 1.6 $ $Date: 1992/09/12 15:23:59 $
//
//
//	User-defined fields are implemented in order to allow us to invoke our
//	own field-level validation routines (verification callbacks) whenever
//	the user attempts to leave a modified field.
//
//	We define a CUI_GENERIC field type that performs no validation (must do
//	this since the default ETI field type is not actually a type at all, and
//	has no validation associated with it).
//
//	We then define a CUI_VALIDATION type, whose sole purpose is to invoke
//	the user-defined verification callbacks.
//
//	Finally we create the types we'll actually use by linking a 'primary
//	type' (CUI_GENERIC or TYPE_INTEGER) with CUI_VALIDATION.  The primary
//	type's validation routines will be applied first, and then (if these
//	pass), our validateFcheck() routine will be called.
//
//	We could extend this by defining additional primary types (eg, HEX),
//	and linking these with CUI_VALIDATE.
//
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifndef  _CUI_SYSCURSES_H
#include "syscurses.h"
#endif
#ifndef  _CUI_WIDGET_H
#include "widget.h"
#endif

#endif	// PRE_COMPILED_HEADERS


FIELDTYPE *CUI_GENERIC;
FIELDTYPE *CUI_VALIDATE;
FIELDTYPE *CUI_STRING;
FIELDTYPE *CUI_INTEGER;

extern "C"
{
	int genericFcheck(FIELD *, char *);
	int validateFcheck(FIELD *, char *);
};


//
//	define all our field types
//

int initUserfields(void)
{
	CUI_GENERIC  = new_fieldtype((PTF_int)genericFcheck, (PTF_int)NULL);
	CUI_VALIDATE = new_fieldtype((PTF_int)validateFcheck, (PTF_int)NULL);
	CUI_STRING	 = link_fieldtype(CUI_GENERIC, CUI_VALIDATE);
	CUI_INTEGER  = link_fieldtype(TYPE_INTEGER, CUI_VALIDATE);
    return(0);
}
 

int genericFcheck(FIELD *, char *)
{
	// nothing to do...

	return(TRUE);
}


//
//	This routine will be called recursively as we tell the form-driver
//	to sync buffers in CUI_textFieldGetString (the only way to sync
//	buffers is to ask for validation).	Avoid invoking the callback
//	twice (and also the extra processing) by means of a static flag.
//

int validateFcheck(FIELD *field, char *)
{
	static int validating = FALSE;

	// if we're already validating, nothing to do

	if(validating)
		return(TRUE);

    CUI_TextFieldVerify verifyData;

	// extract Widget pointer from ETI field

	Widget *control = (Widget *)field_userptr(field);

    // get current field value and store in Verify structure
	// (we may be called recursively as buffer is flushed;
	// set validating flag to warn against infinite recursion)

	validating = TRUE;
	char *data = CUI_textFieldGetString(control, NULL);
	validating = FALSE;
    verifyData.string = data;
	verifyData.ok	  = TRUE;

	// validate, and save the 'ok' value from verifyData

	control->doCallback(CUI_VERIFY_CALLBACK, &verifyData);

	// free up the copied data and return 'ok'

	MEMHINT();
    CUI_free(data);
	return(verifyData.ok);
}


