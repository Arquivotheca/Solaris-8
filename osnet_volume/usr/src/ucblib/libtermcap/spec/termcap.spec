#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)termcap.spec 1.1	99/09/21 SMI"
#

function	tgetent
include		<curses.h>
include		<term.h>
declaration	int tgetent(char *bp, char *name)
version		SUNW_1.1
end

function	tgetflag
declaration	int tgetflag(char *id)
version		SUNW_1.1
end

function	tgetnum
declaration	int tgetnum(char *id)
version		SUNW_1.1
end

function	tgetstr
declaration	char *tgetstr(char *id, char **area)
version		SUNW_1.1
end

function	tgoto
declaration	char *tgoto(char *CM, int destcol, int destline)
version		SUNW_1.1
end

function	tputs
declaration	int tputs(char *cp, int affcnt, int (*)(char))
version		SUNW_1.1
end


data		PC
version		SUNW_1.1
end

data		UP
version		SUNW_1.1
end

data		BC
version		SUNW_1.1
end

data		ospeed
version		SUNW_1.1
end
