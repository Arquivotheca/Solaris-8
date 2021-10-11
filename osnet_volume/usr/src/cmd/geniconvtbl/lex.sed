# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# "@(#)lex.sed 1.2     99/05/25 SMI"

s;\("Input string too long, limit %d\\n"\);gettext(\1);g
s;\("Cannot realloc yytext\\n"\);gettext(\1);g
