/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)usage.c	1.10	93/10/21 SMI"	/* SVr4.0 1.9	*/

#include "lp.h"
#include "printers.h"
#include <locale.h>

/**
 ** usage() - PRINT COMMAND USAGE
 **/

void			usage ()
{
#if	defined(CAN_DO_MODULES)
	(void) printf (gettext(
"usage:\n"
"\n"
"  (add printer)\n\n"
"    lpadmin -p printer {-v device | -U dial-info | -s system[!printer]} [options]\n"
"	[-s system[!printer]]			(remote system/printer name)\n"
"	[-v device]				(printer port name)\n"
"	[-U dial-info]				(phone # or sys. name)\n"
"	[-T type-list]				(printer types)\n"
"	[-c class | -r class]			(add to/del from class)\n"
"	[-A mail|write|quiet|showfault|cmd [-W interval]]\n"
"						(alert definition)\n"
"	[-A none]				(no alerts)\n"
"	[-A list]				(examine alert)\n"
"	[-D comment]				(printer description)\n"
"	[-e printer | -i interface | -m model]	(interface program)\n"
"	[-l | -h]				(is/isn't login tty)\n"
"	[-f allow:forms-list | deny:forms-list]	(forms allowed)\n"
"	[-u allow:user-list | deny:user-list]	(who's allowed to use)\n"
"	[-S char-set-maps | print-wheels]	(list of avail. fonts)\n"
"	[-I content-type-list]			(file types accepted\n"
"	[-F beginning|continue|wait]		(fault recovery)\n"
"	[-o stty='stty-options']		(port characteristics)\n"
"	[-o cpi=scaled-number]			(character pitch)\n"
"	[-o lpi=scaled-number]			(line pitch)\n"
"	[-o width=scaled-number]		(page width)\n"
"	[-o length=scaled-number]		(page length)\n"
"	[-o nobanner]				(allow no banner)\n\n"
"	[-P paper-list]				(add paper type)\n"
"	[-P ~paper-list]			(remove paper type)\n"
"	[-t number-of-trays]			(number of paper trays)\n"
"	[-H module,...|keep|default|none]	(STREAMS modules to push)\n\n"
"  (delete printer or class)\n"
"    lpadmin -x printer-or-class\n\n"
"  (define default destination)\n"
"    lpadmin -d printer-or-class\n\n"
"  (mount form, printwheel)\n"
"    lpadmin -p printer -M {options}\n"
"	[-f form [-a [-o filebreak]] [-t tray-number]]\n"
"						(mount (align) form (on tray))\n"
"	[-S print-wheel]			(mount print wheel)\n\n"
"  (define print-wheel mount alert)\n"
"    lpadmin -S print-wheel {options}\n"
"	[-A mail|write|quiet|cmd [-W interval] [-Q queue-size]]\n"
"	[-A none]				(no alerts)\n"
"	[-A list]				(examine alert)\n "));
#else
	(void) printf (gettext(
"usage:\n"
"\n"
"  (add printer)\n\n"
"    lpadmin -p printer {-v device | -U dial-info | -s system[!printer]} [options]\n"
"	[-s system[!printer]]			(remote system/printer name)\n"
"	[-v device]				(printer port name)\n"
"	[-U dial-info]				(phone # or sys. name)\n"
"	[-T type-list]				(printer types)\n"
"	[-c class | -r class]			(add to/del from class)\n"
"	[-A mail|write|quiet|showfault|cmd [-W interval]]\n"
"						(alert definition)\n"
"	[-A none]				(no alerts)\n"
"	[-A list]				(examine alert)\n"
"	[-D comment]				(printer description)\n"
"	[-e printer | -i interface | -m model]	(interface program)\n"
"	[-l | -h]				(is/isn't login tty)\n"
"	[-f allow:forms-list | deny:forms-list]	(forms allowed)\n"
"	[-u allow:user-list | deny:user-list]	(who's allowed to use)\n"
"	[-S char-set-maps | print-wheels]	(list of avail. fonts)\n"
"	[-I content-type-list]			(file types accepted\n"
"	[-F beginning|continue|wait]		(fault recovery)\n"
"	[-o stty='stty-options']		(port characteristics)\n"
"	[-o cpi=scaled-number]			(character pitch)\n"
"	[-o lpi=scaled-number]			(line pitch)\n"
"	[-o width=scaled-number]		(page width)\n"
"	[-o length=scaled-number]		(page length)\n"
"	[-o nobanner]				(allow no banner)\n\n"
"	[-P paper-list]				(add paper type)\n"
"	[-P ~paper-list]			(remove paper type)\n"
"	[-t number-of-trays]			(number of paper trays)\n"
"  (delete printer or class)\n"
"    lpadmin -x printer-or-class\n\n"
"  (define default destination)\n"
"    lpadmin -d printer-or-class\n\n"
"  (mount form, printwheel)\n"
"    lpadmin -p printer -M {options}\n"
"	[-f form [-a [-o filebreak]] [-t tray-number]]\n"
"						(mount (align) form (on tray))\n"
"	[-S print-wheel]			(mount print wheel)\n\n"
"  (define print-wheel mount alert)\n"
"    lpadmin -S print-wheel {options}\n"
"	[-A mail|write|quiet|cmd [-W interval] [-Q queue-size]]\n"
"	[-A none]				(no alerts)\n"
"	[-A list]				(examine alert)\n "));
#endif

	return;
}
