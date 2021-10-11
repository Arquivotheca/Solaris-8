\
\ Copyright (c) 1995-1999 by Sun Microsystems, Inc.
\ All rights reserved.
\

h# 7ff constant v9bias

\ enable forthdebug when entering interpreter
' kdbg-words is debugger-vocabulary-hook

: next-word ( alf voc-acf -- false | alf' true )
   over  if  drop  else  nip >threads  then
   another-link?  if  >link true  else  false  then
;

\ another? that allows nesting
: another? ( alf voc-acf -- false | alf' voc-acf anf true )
   dup >r next-word  if         ( alf' ) ( r: voc-acf )
      r> over l>name true       ( alf' voc-acf anf true )
   else                         ( ) ( r: voc-acf )
      r> drop false             ( false )
   then
;


create err-no-sym ," symbol not found"

\ guard against bad symbols
: $symbol ( adr,len -- x )
   $handle-literal? 0= if  err-no-sym throw  then
;

\ Compile the value of the symbol if known,
\ otherwise arrange to look it up later at run time.
: symbol ( -- n ) \ symbol-name
   parse-word 2dup 2>r $handle-literal?  if   ( r: sym$ )
      2r> 2drop
   else
      +level
      2r> compile (") ", compile $symbol
      -level
   then
; immediate


\ print in octal
: .o ( n -- ) base @ >r octal . r> base ! ;

\ redefine type macro to support 64 bit addresses
: type ( adr len -- ) bounds ?do i c@ emit loop ;


\ print at most cnt characters of a string
: .nstr ( str cnt -- )
   over if
      over cscount nip min
      bounds ?do i c@ dup 20 80 within if emit else drop then loop
   else
      ." NULL " 2drop
   then
;

\ print string
: .str ( str -- )
   ?dup  if
      cscount type
   else
      ." NULL"
   then
;

\ new actions
: print 2 perform-action ;
: index 3 perform-action ;
: sizeof 1 perform-action ;

\ indent control
-8 value plevel
: +plevel ( -- ) plevel 8 + to plevel ;
: -plevel ( -- ) plevel 8 - to plevel ;
: 0plevel ( -- ) -8 to plevel ;

\ new print words
: name-print ( apf -- apf ) plevel spaces dup body> .name ." = " ;
: voc-print ( addr acf -- )
   ??cr +plevel
   0 swap                         ( addr 0 acf )
   begin  another?  while         ( addr alf acf anf )
      3 pick swap name> print     ( addr alf acf )
      exit?  if                   ( addr alf acf )
         0plevel true throw       ( )
      then                        ( addr alf acf )
   repeat                         ( addr )
   drop -plevel                   ( )
;


3 actions ( offset print-acf )
action: ( addr apf -- x )       @ + x@ ;        \ get
action: ( addr x apf -- )       @ rot + x! ;    \ set
action: ( addr apf -- )
   name-print
   dup @ rot + x@ swap         ( x apf )
   na1+ @ execute cr ;                          \ print

: ext-field ( acf offset -- ) create , , use-actions ;


3 actions ( offset print-acf )
action: ( addr apf -- l )       @ + l@ ;        \ get
action: ( addr l apf -- )       @ rot + l! ;    \ set
action: ( addr apf -- )
   name-print
   dup @ rot + l@ swap          ( l apf )
   na1+ @ execute cr ;                          \ print

: long-field ( acf offset -- ) create , , use-actions ;


3 actions ( offset print-acf )
action: ( addr apf -- w )       @ + w@ ;        \ get
action: ( addr w apf -- )       @ rot + w! ;    \ set
action: ( addr apf -- )
   name-print
   dup @ rot + w@ swap          ( w apf )
   na1+ @ execute cr ;                          \ print

: short-field ( acf offset -- ) create , , use-actions ;


3 actions ( offset print-acf )
action: ( addr apf -- c )       @ + c@ ;        \ get
action: ( addr c apf -- )       @ rot + c! ;    \ set
action: ( addr apf -- )
   name-print
   dup @ rot + c@ swap          ( c apf )
   na1+ @ execute cr ;                          \ print

: byte-field ( acf offset -- ) create , , use-actions ;


3 actions ( offset print-acf )
action: ( addr apf -- ptr )     @ + p@ ;        \ get
action: ( addr l apf -- )       @ rot + p! ;    \ set
action: ( addr apf -- )
   name-print
   dup @ rot + p@ ?dup  if     ( apf ptr )
      swap na1+ @ execute      ( )
   else                        ( apf )
      drop ." NULL"            ( )
   then                        ( )
   cr ;                                         \ print
 
: ptr-field ( acf offset -- ) create , , use-actions ;


3 actions ( offset print-acf )
action: ( addr apf -- saddr )   @ + ;           \ get
action: ( -- )                  quit ;          \ error
action: ( addr apf -- )
   name-print
   dup @ rot + swap             ( saddr apf )
   na1+ @ execute ??cr ;                       \ print
 
: struct-field ( acf offset -- ) create , , use-actions ;


4 actions ( offset inc limit print-acf fetch-acf )
action: ( addr apf -- araddr )  @ + ;           \ get
action: ( -- )                  quit ;          \ set
action: ( addr apf -- )
   name-print
   dup @ rot + swap         ( base apf )
   na1+ dup @ -rot          ( inc base apf' )
   na1+ dup @ swap          ( inc base limit apf' )
   na1+ dup @ swap          ( inc base limit p-acf apf' )
   na1+ @ 2swap             ( inc p-acf f-acf base limit )
   bounds  do               ( inc p-acf f-acf )
      3dup                  ( inc p-acf f-acf inc p-acf f-acf )
      i swap execute        ( inc f-acf p-acf inc p-acf n )
      swap execute          ( inc f-acf p-acf inc )
   +loop                    ( inc f-acf p-acf )
   3drop ??cr ;                                 \ print
action: ( addr index apf -- ith-item )
   rot swap                 ( index addr apf )
   dup @ rot + swap         ( index base apf )
   na1+ dup @ 3 roll *      ( base apf' ioff )
   rot + swap 3 na+ @       ( iaddr f-acf )
   execute ;                                    \ index

: array-field ( f-acf p-acf limit inc offset -- ) create , , , , , use-actions ;


3 actions ( offset mask shift print-acf )
action: ( addr apf -- bits )
   dup @ rot + l@ swap         ( b-word apf )
   na1+ dup @ rot and swap     ( b-masked apf' )
   na1+ @ >> ;                               \ get
action: ( addr bits apf -- )
   rot over @ + dup l@ 2swap   ( b-addr b-word nbits apf )
   na1+ dup @ -rot             ( b-addr b-word mask nbits apf' )
   na1+ @ << over and          ( b-addr b-word mask nb-masked )
   -rot invert and or swap l! ;              \ set
action: ( addr apf -- )
   name-print
   dup @ rot + l@ swap         ( b-word apf )
   na1+ dup @ rot and swap     ( b-mask apf' )
   na1+ dup @ rot swap >> swap ( bits apf' )
   na1+ @ execute cr ;                       \ print

: bits-field ( acf shift mask offset -- ) create , , , , use-actions ;


2 actions ( voc-acf size )
action: ( apf -- )              @ voc-print ;   \ print vocabulary
action: ( apf -- size )         na1+ @ ;        \ sizeof

: c-struct ( size acf -- ) create , , use-actions ;

: c-enum ( {str value}+ n-values -- )
   create   ( n-values {value str}+ )
      dup 2* 1+ 0  do  ,  loop
   does>    ( enum apf -- )
      dup @ 0  do                     ( enum apf' )
         na1+ 2dup @ =  if            ( enum apf' )
            na1+ @ .str               ( enum )
            drop unloop exit          ( )
         then                         ( enum apf' )
         na1+                         ( enum apf' )
      loop                            ( enum apf' )
      drop .d cr                      ( )
;

\ end kdbg section


\ start dlx section

h# 10 constant #dlx-nodes
list: active-dlx

h# 10 constant /modname
h# 40 constant /interp

listnode
   /modname field >modname
   /interp field >interp
   /n field >mlen
   /n field >ilen
nodetype: dlx-node

#dlx-nodes dlx-node more-nodes


: same-modname? ( mod$ node -- mod$ flag )
   dup 2over 2swap          ( mod$ mod$ node node )
   >modname  swap  >mlen @  ( mod$ mod$ node-mod$ )
   $=
;

: find-dlx  ( mod$ list -- mod$ prev this )
   ['] same-modname?  find-node
;

: +onld  \ modname interp  ( -- )
   parse-word                     ( mod$ )
   -1 parse                       ( mod$ interp$ )
   dlx-node allocate-node         ( mod$ interp$ node )
   ?dup 0=  if                    ( mod$ interp$ )
      ." out of memory"           ( mod$ interp$ )
      2drop 2drop  exit           (  )
   then                           ( mod$ interp$ node )
   >r                             ( mod$ interp$  r: node )
   r@ 2dup  >ilen !               ( mod$ interp$ node  r: node )
   >interp  swap move             ( mod$  r: node )
   r@ 2dup  >mlen !               ( mod$ node  r: node )
   >modname  swap move            ( r: node )
   r> active-dlx insert-after     (  )
;

: -onld  \ modname  ( -- )
   parse-word                   ( mod$ )
   active-dlx find-dlx          ( mod$ prev this )
   0<>  if                      ( mod$ prev )
      delete-after              ( mod$ node )
      dlx-node free-node        ( mod$ )
      2drop                     (  )
   else                         ( mod$ tail )
      drop  type  space         (  )
      ." not found"             (  )
   then                         (  )
;

: .dlx-node  ( node -- )
   dup >modname  over >mlen @ type   ( node )
   /modname 5 + to-column            ( node )
   dup >interp  swap >ilen @  type   (  )
   cr
;

: dlx-print-list  ( node -- false )
    .dlx-node  false
;

: .onld  ( -- )
   ??cr ." module" /modname 5 + to-column
   ." interpret" cr
   active-dlx ['] dlx-print-list  find-node
   2drop
;

defer modload-hook
' noop is modload-hook

: load-notify  ( -- )
   parse-word                     ( mod$ )
   modload-hook                   ( mod$ )
   active-dlx                     ( mod$ list )
   begin                          ( mod$ head )
      find-dlx                    ( mod$ prev this )
      dup 0<>  if                 ( mod$ prev this )
         nip  dup                 ( mod$ this this )
         >interp  over >ilen @    ( mod$ this interp$ )
	 ['] interpret-string     ( mod$ this interp$ acf )
         catch  drop              ( mod$ this )
	 >next-node  false        ( mod$ next false )
      else                        ( mod$ tail 0 )
	 drop  true               ( mod$ tail true )
      then                        ( mod$ head done? )
   until                          ( mod$ head )
   3drop                          (  )
;

\ end dlx section
