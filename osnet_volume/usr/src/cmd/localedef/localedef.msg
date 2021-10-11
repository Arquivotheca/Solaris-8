$
$ COPYRIGHT NOTICE
$ 
$ This source code is designated as Restricted Confidential Information
$ and is subject to special restrictions in a confidential disclosure
$ agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
$ this source code outside your company without OSF's specific written
$ approval.  This source code, and all copies and derivative works
$ thereof, must be returned or destroyed at request. You must retain
$ this notice on any copies which you make.
$ 
$ (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
$ ALL RIGHTS RESERVED 
$
$
$ OSF/1 1.2
$

$ @(#)$RCSfile: localedef.msg,v $ $Revision: 1.4.6.2 $ (OSF) $Date: 1992/08/10 14:44:31 $
$
$ COMPONENT_NAME: (CMDNLS)
$
$
$ (C) COPYRIGHT International Business Machines Corp. 1989
$ All Rights Reserved
$
$ US Government Users Restricted Rights - Use, duplication or
$ disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
$
$ 1.7  com/cmd/nls/localedef.msg, cmdnls, bos320, 9132320m 8/11/91 14:21:50
$
$quote "
$set LOCALEDEF

ERR_WRONG_SYM_TYPE   "The symbol '%s' is not the correct type.\n"

ERR_OPEN_READ        "Could not open '%s' for read.\n"

ERR_INTERNAL         "Internal error. [file %s - line %d].\n"

ERR_N_SARGS          "Syntax Error: expected %d arguments and received\
 %d arguments.\n"

ERR_ILL_RANGE_SPEC   "Illegal limit in range specification.\n"

ERR_MEM_ALLOC_FAIL   "Memory allocation failure: [line %d -- module %s].\n"

ERR_WRT_PERM         "Could not open temporary file '%s' for write.\n"

ERR_CHAR_TOO_LONG    "The '%s' character is longer than <mb_cur_max>.\n"

ERR_SYM_UNDEF        "The '%s' character is undefined.\n"

ERR_INVAL_COLL_RANGE "The start of the range must be numerically less\
 than the\nend of the range.\n"

ERR_INVALID_SYM_RNG  "The symbol range containing %s and %s is\
 incorrectly formatted.\n"

ERR_BAD_STR_FMT      "Illegal character reference or escape sequence\
 in '%s'.\n"

ERR_PC_COLLISION     "You have specified different names for the\
 same character '%d'.\n"

ERR_DUP_CHR_SYMBOL   "The character symbol '%s', has already been\
 specified.\n" 

ERR_NO_UNDEFINED     "There are characters in the codeset which are\
 unspecified in the collation order.\n"

ERR_ILL_DEC_CONST    "Illegal decimal constant '%s'.\n"

ERR_ILL_OCT_CONST    "Illegal octal constant '%s'.\n"

ERR_ILL_HEX_CONST    "Illegal hexadecimal constant '%s'.\n"

ERR_MISSING_QUOTE    "Missing closing quote in string '%s'.\n"

ERR_ILL_CHAR         "Illegal character, '%c', in input file.  Character\
 will be ignored.\n"

ERR_ESC_CHAR_MISSING "Character for escape_char statement missing.\
 Statement ignored.\n"

ERR_COM_CHAR_MISSING "Character for <comment_char> statement missing.\
 Statement ignored.\n"

ERR_CHAR_NOT_PCS     "'%c' is not a POSIX Portable Character. Statement\
 ignored.\n"

ERR_ILL_CHAR_SYM     "The character symbol'%s' is missing the closing '>'.\
 The '>' will be added.\n"

ERR_UNKNOWN_KWD      "Unrecognized keyword, '%s', statement ignored.\n"

ERR_UNDEF_RANGE_SYM  "The character symbol '%s' is undefined in the range\
 '%s...%s'.\n The symbol will be ignored.\n"

ERR_UNSUP_ENC        "The encoding specified for the '%s' character is\
 unsupported.\n"

ERR_DUP_COLL_SPEC    "The character, '%s', has already been assigned a\
 weight.\n Specification ignored.\n"

ERR_DUP_COLL_RNG_SPEC "The character %s in range %s...%s already has a\
 collation weight. Range ignored.\n"

ERR_TOUPPER_NOT_OPT  "No toupper section defined for this locale\
 sourcefile.\n"

ERR_CODESET_DEP	     "The use of the \"...\" keyword assumes that the\
 codeset is contiguous\n between the two range endpoints specified.\n"

ERR_ILL_COLL_SUBS    "The collation substitution,  %s%s, contains a\
 symbol which is not\n a character\n.",

ERR_FORWARD_REF      "The symbol, %s, referenced has not yet been\
 specified in the\n collation order.\n"

ERR_USAGE            "usage: localedef [-csdvw] [-x extfile] [-f charmap] [-i locsrc]\
 \\\n\t[-C cc opts] [-L ld opts] [-P tool path] locname\n"

ERR_ERROR            "localedef [ERROR]: FILE: %s, LINE: %d, CHAR: %d\n"

ERR_WARNING          "localedef [WARNING]: FILE: %s, LINE: %d, CHAR: %d\n"

ERR_SYNTAX           "localedef [ERROR]: FILE: %s, LINE: %d, CHAR: %d\n\
 Syntax Error.\n"

ERR_TOO_MANY_ORDERS  "Specific collation weight assignment is not valid\
 when no sort\n keywords have been specified.\n"

ERR_BAD_CHDR         "Required header files sys/localedef.h and sys/lc_core.h\
 are the wrong version or are missing.\n"

ERR_INV_MB_CUR_MIN   "The <mb_cur_min> keyword must be defined as 1, you have\
 defined it \nas %d.\n"

ERR_INV_CODE_SET_NAME "The <code_set_name> must contain only characters from\
 the portable \ncharacter set, %s is not valid.\n"

ERR_FORWARD_BACKWARD "The collation directives forward and backward are\
 mutually \nexclusive.\n"

ERR_TOO_MANY_ARGS "Received too many arguments, only expected %d.\n"

ERR_DUP_CATEGORY "The %s category has already been defined.\n"

ERR_EMPTY_CAT "The %s category is empty.\n"

ERR_UNDEF_CAT "Unrecognized category %s is not processed by localedef.\n"

ERR_USER_DEF "The POSIX defined categories must appear before any unrecognized\
 categories.\n"

ERR_DIGIT_FC_BAD "The file code for the digit %s is not one greater than\
 the \nfile code for %s.\n"

ERR_DIGIT_PC_BAD "The process code for the digit %s is not one greater than\
 the \nprocess code for %s.\n"

ERR_DUP_COLL_SYM "The symbol %s has already been defined. Ignoring definition\
 as a \ncollating-symbol.\n"

ERR_INVALID_UPPER "Locale does not conform to POSIX specifications for the\
 LC_CTYPE upper \nkeyword.\n"

ERR_INVALID_LOWER "Locale does not conform to POSIX specifications for the\
 LC_CTYPE lower \nkeyword.\n"

ERR_INVALID_ALPHA "Locale does not conform to POSIX specifications for the\
 LC_CTYPE alpha \nkeyword.\n"

ERR_INVALID_SPACE "Locale does not conform to POSIX specifications for the\
 LC_CTYPE space \nkeyword.\n"

ERR_INVALID_CNTRL "Locale does not conform to POSIX specifications for the\
 LC_CTYPE cntrl \nkeyword.\n"

ERR_INVALID_PUNCT "Locale does not conform to POSIX specifications for the\
 LC_CTYPE punct \nkeyword.\n"

ERR_INVALID_GRAPH "Locale does not conform to POSIX specifications for the\
 LC_CTYPE graph \nkeyword.\n"

ERR_INVALID_PRINT "Locale does not conform to POSIX specifications for the\
 LC_CTYPE print \nkeyword.\n"

ERR_MIN_UPPER "Locale does not specify the minimum required for the LC_CTYPE\
 upper keyword.\n"

ERR_MIN_LOWER "Locale does not specify the minimum required for the LC_CTYPE\
 lower keyword.\n"

ERR_MIN_SPACE "Locale does not specify the minimum required for the LC_CTYPE\
 space keyword.\n"

ERR_INV_DIGIT "Locale does not specify only '0', '1', '2', '3', '4', '5',\
 '6', '7', \n'8', and '9' for LC_CTYPE digit keyword.\n"

ERR_INV_XDIGIT "Locale does not specify only '0', '1', '2', '3', '4', '5',\
 '6', '7', \n'8', '9', 'a' through 'f', and 'A' through 'F' for LC_CTYPE \
 xdigit keyword.\n"

ERR_COLL_WEIGHTS "The number of operands to LC_COLLATE order exceeds\
 COLL_WEIGHTS_MAX.\n"

ERR_NOSHELL	     "Unable to exec /usr/bin/sh to process localedef\
intermediate files.\n"

ERR_LOAD_FAIL	"Unable to load extension method for %s from file %s\n"

ERR_METHOD_REQUIRED	"%s method must be defined explicitly.\n"

CANT_LOAD_LOCALE	"Unable to load locale \"%s\" for copy directive.\n"

NAME_TOO_LONG		"Locale name longer than PATH_MAX (%d).\n"

ERR_MB_LEN_MAX_TOO_BIG	"cswidth specification: Maximum character length can be no bigger than %d.\n"

ERR_TOO_MANY_CODESETS	"Maximum number of codesets is %d.\n"
