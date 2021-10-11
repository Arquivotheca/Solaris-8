#ident	"@(#)Menu.mod	1.2	92/07/14 SMI"	/* SVr4.0 1.1	*/

menu="Modify Global MS-DOS Programs"

help=open TEXT $INTFBASE/Text.itemhelp 'menu:F1'

framemsg="Move to an item with arrow keys and press ENTER to select the item."

`fmlgrep '^vmsys:' /etc/passwd | fmlcut -f6 -d: | set -e VMSYS;
$VMSYS/bin/listserv -p -m VMSYS`
