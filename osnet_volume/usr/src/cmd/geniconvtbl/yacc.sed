# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# "@(#)yacc.sed 1.2     99/05/25 SMI"
#
s;\("syntax error - cannot backup"\);gettext(\1);g
s;\("yacc stack overflow"\);gettext(\1);g
s;\("syntax error"\);gettext(\1);g
