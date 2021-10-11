	: \ a word drop drop ; imm
	: ( 29 word drop drop ; imm

\ []--------------------------------------------------------------------[]
\  | Forth Utilities							|
\  | Author: Rick McNeal						|
\  | Date  : 2-June-1995						|
\  | 									|
\  | File is split into different sections				|
\  | #1 : Basic forth words that are not provided as built in words	|
\  |      to the interpreter.						|
\  | #2 : Common utility type words which are nice to have around	|
\  | #3 : Screen display utilities for VT100 screens. The beginnings	|
\  |      of an editor.							|
\  | #4 : test/play stuff						|
\ []--------------------------------------------------------------------[]


\ []--------------------------------------------------------------------[]
\  | SECTION 1 : Basic Forth Words					|
\ []--------------------------------------------------------------------[]

\ Turn compile state off
	: [ ( -- ) 0 state ! ; imm

\ Turn compile state on
	: ] ( -- ) -1 state ! ; imm

\ exit the intrepreter
	: bye ( -- ) 1 leave ! ;

\ return the current directory pointer
	: here ( -- adr ) (here) @ ;

\ allocate some number of bytes from the dictionary and return pointer
\ to the allocated space
	: alloc ( size -- adr ) here >r here + (here) ! r> ;

\ find the address of the next word in the dictionary
	: ' ( -- adr ) 20 word drop find drop ; imm

\ test for not equal to zero
	: !0 ( val -- t/f ) 0 = ;

\ compile a ?branch into a word
	: if ' (?branch) literal , here 0 , ; imm

\ compile a else into a word
	: else ( loc -- loc ) 
	  ' (jmp) literal ,		\ compile (jmp) into stream
	  here 0 , >r 			\ save cur. dp on rp, store 0 after jmp
	  here swap ! 			\ store cur dp loc for (?branch)
	  r> 				\ leave loc on stack
	; imm

\ complete the 'if' statement
	: endif here swap ! ; imm

\ setup a loop
	: do here ; imm

\ finish off loop
	: loop ' dup literal , ' !0 literal , ' (?branch) literal , , ; imm

\ print a space to the console
	: space 20 emit ;

\ []--------------------------------------------------------------------[]
\  | SECTION 2 : Basic Utility Words					|
\ []--------------------------------------------------------------------[]

\ copy data from source to dest for some number of bytes  
\ NOTE: Doesn't handle case where d is inside of s[0] to s[c]
	: bcopy ( s d c -- )
	  do				\ ... s, d, cc --
	    >r				\ ... s, d -- cc
	    1 pick c@			\ ... s, d, v -- cc
	    1 pick c!			\ ... s, d -- cc
	    >r 1 + r> 1 +		\ ... s++, d++ -- cc
	    r> 1 -			\ ... s, d, cc-- --
	  loop drop drop drop
	;

\ create a word which has a variable type
	: variable ( -- )
	  20 word 1 + dup alloc		\ ... tib, cc, dp --
	  dup >r swap			\ ... tib, dp, cc -- dp
	  bcopy				\ ... -- dp
	  here				\ ... addr -- dp
	  last @ ,			\ lay down prev pointer ... addr -- dp
	  3 ,				\ type == variable ... addr -- dp
	  r> ,				\ name string pointer ... addr --
	  0 ,				\ initial value ... addr --
	  last !			\ store this pointer as last ... --
	;

\ execute the given pointer/size pair that's on the stack. If it can be
\ found in the dictionary type execution. else it should be a word and
\ convert the string to a number on the stack.
	: 'execute ( ptr size -- ? )
	  drop find if
	    exec
	  else
	    number
	  endif
	;

\ some common functions for changing the output. The first four here
\ actually change the base in which numbers are interpreted.
	: hex 10 base ! ;
	: decimal a base ! ;
	: octal 8 base ! ;
	: binary 2 base ! ;

\ Common function which changes the base only for the next input word. This
\ can be a word or function.
	: tbase ( newbase -- )
	  base @ >r			\ save the current base on rp
	  base !			\ set to decimal output
	  20 word			\ get next word in list
	  'execute			\ doit
	  r> base !			\ restore the base
	;
	
\ Here's the actual definitions which use tbase.
\ CAUTION: Since these are immediate functions, if used in a :/; pair the
\ next word will be exectued during the compile not when the word is
\ executed.
	: b# 2 tbase ; imm
	: o# 8 tbase ; imm
	: d# a tbase ; imm
	: h# 10 tbase ; imm

	: type ( buf count -- )
	  do
	    swap dup c@ emit		\ print out first character
	    1 + swap			\ add 1 to buf and swap args back
	    1 - 			\ subtract from count
	  loop drop drop
	;

\ Some variables that are needed for the next couple of words
	variable thePad 40 alloc drop
	variable thePP

\ Initialized the Pad buffer
	: <# ( -- ) thePad 10 + thePP ! ;

\ Divide down the value on the stack and stick the % value into the Pad
\ This routine is used for converting a number to it's string value. Not
\ normally used by itself. Along the the lines of <# # # # # #> or <# #s #>
	: # ( val -- val )
	  dup base @ / swap base @ mod	\ divide down tos and leave mod 
	  dup 9 > if			\ is value greater than 9?
	    57 +			\ change val to character 'abc..' etc.
	  else
	    30 +			\ change val to character '0123..' etc.
	  endif
	  thePP @ 1 - thePP !		\ decrement addr by one and save
	  thePP @ c! 			\ store the character in the buffer
	;

\ Call '#' until the stack value is zero
	: #s ( val -- ) do # loop drop ;

\ Leave the address of the pad pointer and number of character on stack
	: #> ( -- addr cc )
	  thePP @			\ buffer address first
	  thePad 10 + thePP @ -		\ number of character in buffer
	;

\ Print routine for numbers
	: . ( val -- ) <# #s #> type space ;

\ print a string out to the console. NOTE: Need to make this work as a
\ compiled entry.
	: ." 22 word type a emit ;

\ print a string to the screen which must be null terminated
	: puts ( adr -- )
	  dup c@			\ ... adr c --
	  do
	    emit 1 +			\ emit character of string ... adr --
	    dup c@			\ test of eof ... adr c --
	  loop				\ cont. until 0 ... adr --
	  drop drop			\ drop character & address
	;

\ display the words from a single dictionary.
	: singlelist ( dict -- )
	  do
	    dup 4 + @ puts		\ dump out this nodes name 
	    				\ ... adr namep --
	    space
	    @				\ fetch prev ptr. ... adr adr ---
	  loop				\ loop while non-nil ... adr --
	  drop				\ drop last adr
	;

\ display the words in both dictionaries that are currently used.
	: words ( -- )
	  ' find literal		\ get the addr of the last entry 
	  				\ ... adr --
	  singlelist
	  last @			\ now print out compiled words ... addr
	  singlelist
	;

\ []--------------------------------------------------------------------[]
\  | SECTION 3 : Screen Display						|
\ []--------------------------------------------------------------------[]

\ []--------------------------------------------------------------------[]
\  | SECTION 4 : Test & Play code					|
\ []--------------------------------------------------------------------[]

\ couple of varibles need for the twrite/topen funcs below
	variable wblock
	variable wname 20 alloc drop
	variable wfd

\ test write. after opening a file for write via topen this function
\ writes a decrementing value into the file. This was used to debug
\ the dos write code in the stand alone environment.
	: twrite ( cc -- )
	  do				\ ... index --
	    dup wblock !		\ store index in wblock ... index --
	    wfd @ wblock 4		\ setup for write 
	    				\ ... index fd, buf, cc --
	    write drop			\ do write ... index --
	    1 -				\ decrement index ... index index --
	  loop
	  drop
	;

\ open a file with the given mode which is on the stack.
\ Open mode values from sys/fcntl.h for Solaris 2.x
\ O_RDONLY	0
\ O_WRONLY	1
\ O_RDWR	2
\    or these in ...
\ O_NDELAY	4
\ O_APPEND	8
\ O_CREAT	0x100
\ O_TRUNC	0x200
\ O_EXCL	0x400
	: topen ( mode -- )
	  20 word drop swap		\ args for open ... name, flag --
	  open				\ do open ... fd --
	  wfd !				\ store fd for later ... --
	;

	." Standalone Forth "
