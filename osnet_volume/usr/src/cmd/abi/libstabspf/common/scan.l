%{
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scan.l	1.1	99/05/14 SMI"

#include <stdio.h>
#include "test.h"

%}

DOT_STABS	^\t\.stabs\t
OTHER		^.
TYPE_SPEC	[a-zA-Z_][a-zA-Z_0-9 ]*
NUM		[-0-9][0-9]*
HNUM		0x[0-9A-Fa-f]+
ENDSTR		\"\,
ENDSTRC		\\\\{ENDSTR}

%X P S X T N D V
%%
{DOT_STABS}	{
		  BEGIN(P);
		  return (K_STAB);
		}

{OTHER}		BEGIN(X);

<P>\"		BEGIN(S);

<S>[^\"\\]*	{ /* '\\' are special */
		  yymore();
		}
<S>\\[^\\] 	{ /* continue if not '\\\\' */
		  yymore();
		}
<S>{ENDSTR} 	{
		  BEGIN(T);
		  return (K_STR);
		}
<S>{ENDSTRC} 	{
		  BEGIN(T);
		  return (K_STRC);
		}
<T>{NUM}	{
		  return (K_TYPE);
		}
<T>{HNUM}	{
		  return (K_TYPE);
		}
<T>\,		BEGIN(N);

<N>{NUM}	{
		  return (K_NUM);
		}
<N>\,		BEGIN(D);

<D>{NUM}	{
		  return (K_DESC);
		}
<D>\,		BEGIN(V);

<V>{NUM}	{
		  return (K_VAL);
		}
<V>\n		{ BEGIN(INITIAL);
		  ++stab_line;
		  return (K_END);
		}
<X>.*\n		{ BEGIN(INITIAL);
		  ++stab_line;
		}

\n 		++stab_line;
%%
