%{
/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_grammar.y	1.2	99/09/06 SMI"

#include <mdb/mdb_types.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_shell.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_io.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb.h>

/*
 * Utility routines to fetch values from the target's virtual address space
 * and object file, respectively.  These are called from the handlers for
 * the * /.../ and % /.../ code below.
 */

static void
vfetch(void *buf, size_t nbytes, uintptr_t addr)
{
	if (mdb_tgt_vread(mdb.m_target, buf, nbytes, addr) != nbytes)
		yyperror("failed to read from address %p", addr);
}

static void
ffetch(void *buf, size_t nbytes, uintptr_t addr)
{
	if (mdb_tgt_fread(mdb.m_target, buf, nbytes, addr) != nbytes)
		yyperror("failed to read from address %p", addr);
}

%}

%union {
	char *l_string;
	char l_char;
	uintmax_t l_immediate;
	mdb_var_t *l_var;
	mdb_idcmd_t *l_dcmd;
}

%token	<l_string>	MDB_TOK_SYMBOL
%token	<l_string>	MDB_TOK_STRING
%token	<l_char>	MDB_TOK_CHAR
%token	<l_immediate>	MDB_TOK_IMMEDIATE
%token	<l_dcmd>	MDB_TOK_DCMD
%token	<l_var>		MDB_TOK_VAR_REF
%token	<l_immediate>	MDB_TOK_LEXPR
%token	<l_immediate>	MDB_TOK_REXPR
%token	<l_immediate>	MDB_TOK_COR1_DEREF
%token	<l_immediate>	MDB_TOK_COR2_DEREF
%token	<l_immediate>	MDB_TOK_COR4_DEREF
%token	<l_immediate>	MDB_TOK_COR8_DEREF
%token	<l_immediate>	MDB_TOK_OBJ1_DEREF
%token	<l_immediate>	MDB_TOK_OBJ2_DEREF
%token	<l_immediate>	MDB_TOK_OBJ4_DEREF
%token	<l_immediate>	MDB_TOK_OBJ8_DEREF

%left	'|'
%left	'^'
%left	'&'
%left	MDB_TOK_EQUAL MDB_TOK_NOTEQUAL
%left	MDB_TOK_LSHIFT MDB_TOK_RSHIFT
%left	'-' '+'
%left	'*' '%' '#'

%right	MDB_COR_VALUE
%right	MDB_OBJ_VALUE
%right	MDB_INT_NEGATE
%right	MDB_BIT_COMPLEMENT
%right	MDB_LOG_NEGATE
%right	MDB_VAR_REFERENCE

%type	<l_immediate>	expression
%type	<l_dcmd>	command

%%
statement_list:	/* Empty */
	|	statement_list statement { return (0); }
	;

terminator:	'\n'
	|	';'

statement:	pipeline shell_pipe terminator {
			if (!mdb_call(mdb_nv_get_value(mdb.m_dot), 1, 0))
				return (0);
		}

	|	expression pipeline shell_pipe terminator {
			if (!mdb_call($1, 1, DCMD_ADDRSPEC))
				return (0);
		}

	|	expression ',' expression pipeline shell_pipe terminator {
			if (!mdb_call($1, $3, DCMD_ADDRSPEC | DCMD_LOOP))
				return (0);
		}

	|	',' expression pipeline shell_pipe terminator {
			if (!mdb_call(mdb_nv_get_value(mdb.m_dot), $2,
			    DCMD_LOOP))
				return (0);
		}

	|	expression terminator {
			mdb_frame_t *pfp = mdb.m_frame->f_prev;
			/*
			 * The handling of naked expressions is slightly tricky:
			 * in a string context, we want to just set dot to the
			 * expression value.  In a pipe context, we also set
			 * dot but need to record the address in the right-
			 * hand command's addrv and update any vcbs that are
			 * active.  Otherwise, on the command-line, we have to
			 * support this as an alias for executing the previous
			 * command with the new value of dot.  Sigh.
			 */
			if (mdb_iob_isastr(mdb.m_in)) {
				mdb_nv_set_value(mdb.m_dot, $1);
				mdb.m_incr = 0;
			} else if (pfp != NULL && pfp->f_pcmd != NULL) {
				mdb_addrvec_unshift(&pfp->f_pcmd->c_addrv,
				    (uintptr_t)$1);
				mdb_vcb_update(pfp, (uintptr_t)$1);
				mdb_nv_set_value(mdb.m_dot, $1);
			} else {
				mdb_list_move(&mdb.m_lastc,
				    &mdb.m_frame->f_cmds);
				if (!mdb_call($1, 1, DCMD_ADDRSPEC))
					return (0);
			}
		}

	|	expression ',' expression shell_pipe terminator {
			mdb_list_move(&mdb.m_lastc, &mdb.m_frame->f_cmds);
			if (!mdb_call($1, $3, DCMD_ADDRSPEC | DCMD_LOOP))
				return (0);
		}

	|	',' expression shell_pipe terminator {
			uintmax_t dot = mdb_dot_incr(",");
			mdb_list_move(&mdb.m_lastc, &mdb.m_frame->f_cmds);
			if (!mdb_call(dot, $2, DCMD_LOOP))
				return (0);
		}

	|	'!' MDB_TOK_STRING terminator {
			if (mdb_iob_isapipe(mdb.m_in))
				yyerror("syntax error");
			mdb_shell_exec($2);
		}

	|	terminator {
			if ((mdb.m_flags & MDB_FL_REPLAST) &&
			    !mdb_iob_isastr(mdb.m_in)) {
				uintmax_t dot = mdb_dot_incr("\\n");
				/*
				 * If a bare terminator is encountered, execute
				 * the previous command if -o repeatlast is set
				 * and stdin is not an mdb_eval() string.
				 */
				mdb_list_move(&mdb.m_lastc,
				    &mdb.m_frame->f_cmds);
				if (!mdb_call(dot, 1, 0))
					return (0);
			}
		}
	;

pipeline:	pipeline '|' command { mdb_cmd_create($3, &yyargv); }
	|	command { mdb_cmd_create($1, &yyargv); }
	;

command:	'?' format_list { $$ = mdb_dcmd_lookup("?"); }
	|	'/' format_list	{ $$ = mdb_dcmd_lookup("/"); }
	|	'\\' format_list { $$ = mdb_dcmd_lookup("\\"); }
	|	'=' format_list { $$ = mdb_dcmd_lookup("="); }
	|	MDB_TOK_DCMD argument_list { $$ = $1; }
	|	'$' { $$ = mdb_dcmd_lookup("$?"); }
	;

shell_pipe:	/* Empty */
	|	'!' MDB_TOK_STRING { mdb_shell_pipe($2); }
	;

format_list:	/* Empty */
	|	format_list MDB_TOK_LEXPR expression MDB_TOK_REXPR {
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_IMMEDIATE;
			arg.a_un.a_val = $3;

			mdb_argvec_append(&yyargv, &arg);
		}

	|	format_list MDB_TOK_IMMEDIATE {
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_IMMEDIATE;
			arg.a_un.a_val = $2;

			mdb_argvec_append(&yyargv, &arg);
		}

	|	format_list MDB_TOK_STRING	{
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_STRING;
			arg.a_un.a_str = $2;

			mdb_argvec_append(&yyargv, &arg);
		}

	|	format_list MDB_TOK_CHAR	{
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_CHAR;
			arg.a_un.a_char = $2;

			mdb_argvec_append(&yyargv, &arg);
		}
	;

argument_list:	/* Empty */
	|	argument_list MDB_TOK_LEXPR expression MDB_TOK_REXPR {
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_IMMEDIATE;
			arg.a_un.a_val = $3;

			mdb_argvec_append(&yyargv, &arg);
		}

	|	argument_list MDB_TOK_STRING {
			mdb_arg_t arg;

			arg.a_type = MDB_TYPE_STRING;
			arg.a_un.a_str = $2;

			mdb_argvec_append(&yyargv, &arg);
		}
	;

expression:	expression '+' expression { $$ = $1 + $3; }
	|	expression '-' expression { $$ = $1 - $3; }
	|	expression '*' expression { $$ = $1 * $3; }

	|	expression '%' expression {
			if ($3 == 0UL)
				yyerror("attempted to divide by zero");

			$$ = $1 / $3;
		}

	|	expression '&' expression { $$ = $1 & $3; }
	|	expression '|' expression { $$ = $1 | $3; }
	|	expression '^' expression { $$ = $1 ^ $3; }

	|	expression MDB_TOK_EQUAL expression { $$ = ($1 == $3); }
	|	expression MDB_TOK_NOTEQUAL expression { $$ = ($1 != $3); }

	|	expression MDB_TOK_LSHIFT expression { $$ = $1 << $3; }
	|	expression MDB_TOK_RSHIFT expression { $$ = $1 >> $3; }

	|	expression '#' expression {
			if ($3 == 0UL)
				yyerror("attempted to divide by zero");

			$$ = (($1 + ($3 - 1)) / $3) * $3;
		}

	|	'*' expression %prec MDB_COR_VALUE {
			uintptr_t value;

			vfetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_COR1_DEREF expression %prec MDB_COR_VALUE {
			uint8_t value;

			vfetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_COR2_DEREF expression %prec MDB_COR_VALUE {
			uint16_t value;

			vfetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_COR4_DEREF expression %prec MDB_COR_VALUE {
			uint32_t value;

			vfetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_COR8_DEREF expression %prec MDB_COR_VALUE {
			uint64_t value;

			vfetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	'%' expression %prec MDB_OBJ_VALUE {
			uintptr_t value;

			ffetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_OBJ1_DEREF expression %prec MDB_OBJ_VALUE {
			uint8_t value;

			ffetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_OBJ2_DEREF expression %prec MDB_OBJ_VALUE {
			uint16_t value;

			ffetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_OBJ4_DEREF expression %prec MDB_OBJ_VALUE {
			uint32_t value;

			ffetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	MDB_TOK_OBJ8_DEREF expression %prec MDB_OBJ_VALUE {
			uint64_t value;

			ffetch(&value, sizeof (value), $2);
			$$ = value;
		}

	|	'-' expression %prec MDB_INT_NEGATE { $$ = -$2; }
	|	'~' expression %prec MDB_BIT_COMPLEMENT { $$ = ~$2; }
	|	'#' expression %prec MDB_LOG_NEGATE { $$ = !$2; }
	|	'(' expression ')' { $$ = $2; }

	|	MDB_TOK_VAR_REF %prec MDB_VAR_REFERENCE {
			$$ = mdb_nv_get_value($1);
		}

	|	MDB_TOK_SYMBOL {
			if (strcmp($1, ".") == 0) {
				$$ = mdb_nv_get_value(mdb.m_dot);
				strfree($1);	

			} else {
				const char *obj = MDB_TGT_OBJ_EVERY, *name = $1;
				char *s = (char *)$1;
				GElf_Sym sym;

				if ((s = strrsplit(s, '`')) != NULL) {
					name = s;
					obj = $1;
				}

				if (mdb_tgt_lookup_by_name(mdb.m_target,
				    obj, name, &sym) == -1) {
					strfree($1);
					yyperror("failed to dereference "
					    "symbol");
				}
		
				strfree($1);	
				$$ = (uintmax_t)sym.st_value;
			}
		}

	|	'+' { $$ = mdb_dot_incr("+"); }
	|	'^' { $$ = mdb_dot_decr("^"); }
	|	'&' { $$ = mdb.m_raddr; }
	|	MDB_TOK_IMMEDIATE
	;

%%
