\ Copyright (c) 1995-1999 by Sun Microsystems, Inc.
\ All rights reserved.
\
\ "@(#)data32.fth	1.2	99/09/15 SMI"

hex

only forth also definitions
vocabulary kdbg-words
also kdbg-words definitions

defer p@
defer p!
['] l@ is p@
['] l! is p!

4 constant ptrsize

d# 18 constant nbitsminor
h# 3ffff constant maxmin
