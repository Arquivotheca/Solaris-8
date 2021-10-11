\ Copyright (c) 1995-1999 by Sun Microsystems, Inc.
\ All rights reserved.
\
\ "@(#)data64.fth	1.2	99/09/15 SMI"

hex

only forth also definitions
vocabulary kdbg-words
also kdbg-words definitions

defer p@
defer p!
['] x@ is p@
['] x! is p!

8 constant ptrsize

d# 32 constant nbitsminor
h# ffffffff constant maxmin
