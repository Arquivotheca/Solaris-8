/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)qreason.c	1.7	93/03/09 SMI"	/* SVr4.0 1.2	*/

#include <locale.h>
#include <libintl.h>

#define	MSG_UNKREQ	"qreason(): unrecognized message request."

#define	MSG_RE_SUC	"Processing of request script was successful."
#define	MSG_IN_SUC0	"Installation of <%s> was successful."
#define	MSG_IN_SUC1	"\nInstallation of %s on %s as package instance <%s> " \
			"was successful."
#define	MSG_RM_SUC0	"Removal of <%s> was successful."
#define	MSG_RM_SUC1	"\nRemoval of <%s> package instance on %s was " \
			"successful."

#define	MSG_RE_FAIL	"Processing of request script failed."
#define	MSG_IN_FAIL0	"Installation of <%s> failed."
#define	MSG_IN_FAIL1	"\nInstallation of %s on %s as package instance <%s> " \
			"failed."
#define	MSG_RM_FAIL0	"Removal of <%s> failed."
#define	MSG_RM_FAIL1	"\nRemoval of <%s> package instance on %s failed."

#define	MSG_RE_PARFAIL	"Processing of request script partially failed."
#define	MSG_IN_PARFAIL0	"Installation of <%s> partially failed."
#define	MSG_IN_PARFAIL1	"\nInstallation of %s on %s as package instance <%s> " \
			"partially failed."
#define	MSG_RM_PARFAIL0	"Removal of <%s> partially failed."
#define	MSG_RM_PARFAIL1	"\nRemoval of <%s> package instance on %s partially " \
			"failed."

#define	MSG_RE_USER	"Processing of request script was terminated due to " \
			"user request."
#define	MSG_IN_USER0	"Installation of <%s> was terminated due to user " \
			"request."
#define	MSG_IN_USER1	"\nInstallation of %s on %s as package instance <%s> " \
			"was terminated due to user request."
#define	MSG_RM_USER0	"Removal of <%s> was terminated due to user request."
#define	MSG_RM_USER1	"\nRemoval of <%s> package instance on %s was " \
			"terminated due to user request."

#define	MSG_RE_SUA	"Processing of request script was suspended " \
			"(administration)."
#define	MSG_IN_SUA0	"Installation of <%s> was suspended (administration)."
#define	MSG_IN_SUA1	"\nInstallation of %s on %s as package instance <%s> " \
			"was suspended (administration)."
#define	MSG_RM_SUA0	"Removal of <%s> was suspended (administration)."
#define	MSG_RM_SUA1	"\nRemoval of <%s> package instance on %s was " \
			"suspended (administration)."

#define	MSG_RE_SUI	"Processing of request script was suspended " \
			"(interaction required)."
#define	MSG_IN_SUI0	"Installation of <%s> was suspended (interaction " \
			"required)."
#define	MSG_IN_SUI1	"\nInstallation of %s on %s as package instance <%s> " \
			"was suspended (interaction required)."
#define	MSG_RM_SUI0	"Removal of <%s> was suspended (interaction required)."
#define	MSG_RM_SUI1	"\nRemoval of <%s> package instance on %s was " \
			"suspended (interaction required)."

#define	MSG_RE_IEPI	"Processing of request script failed (internal " \
			"error) - package partially installed."
#define	MSG_IN_IEPI0	"Installation of <%s> failed (internal error) " \
			"- package partially installed."
#define	MSG_IN_IEPI1	"\nInstallation of %s on %s as package instance <%s> " \
			"failed (internal error) - package partially installed."
#define	MSG_RM_IEPI0	"Removal of <%s> failed (internal error) - package " \
			"partially installed."
#define	MSG_RM_IEPI1	"\nRemoval of <%s> package instance on %s failed " \
			"(internal error) - package partially installed."

#define	MSG_RE_IE	"Processing of request script failed (internal error)."
#define	MSG_IN_IE0	"Installation of <%s> failed (internal error)."
#define	MSG_IN_IE1	"\nInstallation of %s on %s as package instance <%s> " \
			"failed (internal error)."
#define	MSG_RM_IE0	"Removal of <%s> failed (internal error)."
#define	MSG_RM_IE1	"\nRemoval of <%s> package instance on %s failed " \
			"(internal error)."


#define	MSG_RE_UNK	"Processing of request script failed with an " \
			"unrecognized error code."
#define	MSG_IN_UNK0	"Installation of <%s> failed with an unrecognized " \
			"error code."
#define	MSG_IN_UNK1	"\nInstallation of %s on %s as package instance <%s> " \
			"failed with an unrecognized error code."
#define	MSG_RM_UNK0	"Removal of <%s> failed with an unrecognized error " \
			"code."
#define	MSG_RM_UNK1	"\nRemoval of <%s> package instance on %s failed " \
			"with an unrecognized error code."

char *
qreason(int caller, int retcode, int started)
{
	char	*status;

	switch (retcode) {
	    case  0:
	    case 10:
	    case 20:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_SUC);
			break;
		    case 1:
			status = gettext(MSG_IN_SUC0);
			break;
		    case 2:
			status = gettext(MSG_IN_SUC1);
			break;
		    case 3:
			status = gettext(MSG_RM_SUC0);
			break;
		    case 4:
			status = gettext(MSG_RM_SUC1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case  1:
	    case 11:
	    case 21:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_FAIL);
			break;
		    case 1:
			status = gettext(MSG_IN_FAIL0);
			break;
		    case 2:
			status = gettext(MSG_IN_FAIL1);
			break;
		    case 3:
			status = gettext(MSG_RM_FAIL0);
			break;
		    case 4:
			status = gettext(MSG_RM_FAIL1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case  2:
	    case 12:
	    case 22:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_PARFAIL);
			break;
		    case 1:
			status = gettext(MSG_IN_PARFAIL0);
			break;
		    case 2:
			status = gettext(MSG_IN_PARFAIL1);
			break;
		    case 3:
			status = gettext(MSG_RM_PARFAIL0);
			break;
		    case 4:
			status = gettext(MSG_RM_PARFAIL1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case  3:
	    case 13:
	    case 23:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_USER);
			break;
		    case 1:
			status = gettext(MSG_IN_USER0);
			break;
		    case 2:
			status = gettext(MSG_IN_USER1);
			break;
		    case 3:
			status = gettext(MSG_RM_USER0);
			break;
		    case 4:
			status = gettext(MSG_RM_USER1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case  4:
	    case 14:
	    case 24:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_SUA);
			break;
		    case 1:
			status = gettext(MSG_IN_SUA0);
			break;
		    case 2:
			status = gettext(MSG_IN_SUA1);
			break;
		    case 3:
			status = gettext(MSG_RM_SUA0);
			break;
		    case 4:
			status = gettext(MSG_RM_SUA1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case  5:
	    case 15:
	    case 25:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_SUI);
			break;
		    case 1:
			status = gettext(MSG_IN_SUI0);
			break;
		    case 2:
			status = gettext(MSG_IN_SUI1);
			break;
		    case 3:
			status = gettext(MSG_RM_SUI0);
			break;
		    case 4:
			status = gettext(MSG_RM_SUI1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;

	    case 99:
		if (started)
			switch (caller) {
			    case 0:
				status = gettext(MSG_RE_IEPI);
				break;
			    case 1:
				status = gettext(MSG_IN_IEPI0);
				break;
			    case 2:
				status = gettext(MSG_IN_IEPI1);
				break;
			    case 3:
				status = gettext(MSG_RM_IEPI0);
				break;
			    case 4:
				status = gettext(MSG_RM_IEPI1);
				break;
			    default:
				status = gettext(MSG_UNKREQ);
				break;
			}
		else
			switch (caller) {
			    case 0:
				status = gettext(MSG_RE_IE);
				break;
			    case 1:
				status = gettext(MSG_IN_IE0);
				break;
			    case 2:
				status = gettext(MSG_IN_IE1);
				break;
			    case 3:
				status = gettext(MSG_RM_IE0);
				break;
			    case 4:
				status = gettext(MSG_RM_IE1);
				break;
			    default:
				status = gettext(MSG_UNKREQ);
				break;
			}
		break;

	    default:
		switch (caller) {
		    case 0:
			status = gettext(MSG_RE_UNK);
			break;
		    case 1:
			status = gettext(MSG_IN_UNK0);
			break;
		    case 2:
			status = gettext(MSG_IN_UNK1);
			break;
		    case 3:
			status = gettext(MSG_RM_UNK0);
			break;
		    case 4:
			status = gettext(MSG_RM_UNK1);
			break;
		    default:
			status = gettext(MSG_UNKREQ);
			break;
		}
		break;
	}

	return (status);
}
