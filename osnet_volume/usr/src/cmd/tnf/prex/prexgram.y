%{
/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident  "@(#)prexgram.y 1.24 97/11/07 SMI"
%}

%token ADD
%token ALLOC
%token BUFFER
%token CLEAR
%token COMMA
%token CONNECT
%token DEALLOC
%token DELETE
%token FILTER
%token CONTINUE
%token CREATE
%token DISABLE
%token ENABLE
%token EQ
%token FCNNAME
%token FCNS
%token ON
%token OFF
%token HELP
%token KTRACE
%token HISTORY
%token IDENT
%token INVAL
%token KILL
%token LIST
%token NL
%token PFILTER
%token PROBES
%token QUIT
%token REGEXP
%token RESUME
%token SCALED_INT
%token SETNAME
%token SETS
%token SOURCE
%token SUSPEND
%token TRACE
%token UNTRACE
%token VALSTR
%token VALUES

%{
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "set.h"
#include "cmd.h"
#include "fcn.h"
#include "list.h"
#include "expr.h"
#include "spec.h"
#include "source.h"
#include "prbk.h"

extern int	yylex();

extern void help_on_topic(char *topic);
extern void help_on_command(int cmd);
extern boolean_t g_kernelmode;
extern tnfctl_handle_t *g_hndl;

void		quit(boolean_t	killtarget, boolean_t  runtarget);
extern void	help(void);
extern void	process_cmd(tnfctl_handle_t *hndl, cmd_t *cmd);
%}

%union
{
	char *	strval;
	expr_t *	exprval;
	spec_t *	specval;
	void *	pidlistval;
	int		intval;
}

%type <strval>	SETNAME FCNNAME IDENT VALSTR REGEXP
%type <intval>  CONTINUE DISABLE ENABLE HELP LIST QUIT SOURCE TRACE UNTRACE BUFFER KTRACE PFILTER CLEAR CONNECT command
%type <exprval>	expr exprlist
%type <specval> spec speclist
%type <pidlistval> pidlist
%type <intval>	SCALED_INT singlepid

%%

file			: statement_list
			;

statement_list		: /* empty */			{ prompt(); }
			| statement_list statement
				{ 
			  		  if (g_kernelmode) 
						prbk_warn_pfilter_empty();
						prompt(); 
				}
			;

statement		: empty_statement
			| help_statement
			| continue_statement
			| quit_statement
			| enable_statement
			| disable_statement
			| trace_statement
			| untrace_statement
			| connect_statement
			| clear_statement
			| pfilter_statement
			| ktrace_statement
			| buffer_statement
			| create_statement
			| source_statement
			| listsets_statement
			| listhistory_statement
			| listfcns_statement
			| listprobes_statement
			| listvalues_statement
			| error NL		{ yyerrok; }
			;

empty_statement		: NL
			;

command			: CONTINUE	{ $$ = $1; } /* user&kernel */
			| DISABLE	{ $$ = $1; }
    			| ENABLE	{ $$ = $1; }
			| HELP		{ $$ = $1; }
			| LIST		{ $$ = $1; }
    			| QUIT		{ $$ = $1; }
			| SOURCE	{ $$ = $1; }
			| TRACE		{ $$ = $1; }
			| UNTRACE	{ $$ = $1; }
			| BUFFER	{ $$ = $1; } /* kernel only */
			| KTRACE	{ $$ = $1; }
			| PFILTER	{ $$ = $1; }
    			| CLEAR		{ $$ = $1; } /* user only */
			| CONNECT	{ $$ = $1; }
    			;

help_statement		: HELP NL		{ help(); }
			| HELP command NL       { help_on_command($2); }
			| HELP IDENT NL         { help_on_topic($2); }
			;

continue_statement	: CONTINUE NL
				{
					if (!g_kernelmode) YYACCEPT;
				}
			;

quit_statement		: QUIT NL		{ quit(B_TRUE, B_TRUE); }
			| QUIT KILL NL		{ quit(B_TRUE, B_FALSE); }
			| QUIT RESUME NL	{ quit(B_FALSE, B_TRUE); }
			| QUIT SUSPEND NL	{ quit(B_FALSE, B_FALSE); }
			;

enable_statement	: ENABLE SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($2, CMD_ENABLE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| ENABLE exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($2, CMD_ENABLE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

disable_statement	: DISABLE SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($2, CMD_DISABLE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| DISABLE exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($2, CMD_DISABLE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

trace_statement		: TRACE SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($2, CMD_TRACE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| TRACE exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($2, CMD_TRACE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

untrace_statement	: UNTRACE SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($2, CMD_UNTRACE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| UNTRACE exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($2, CMD_UNTRACE, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

connect_statement	: CONNECT FCNNAME SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($3, CMD_CONNECT, $2);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| CONNECT FCNNAME exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($3, CMD_CONNECT, $2);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

clear_statement		: CLEAR SETNAME NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_set($2, CMD_CLEAR, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			| CLEAR exprlist NL
				{
					cmd_t *cmd_p;
					cmd_p = cmd_expr($2, CMD_CLEAR, NULL);
					if (cmd_p)
						process_cmd(g_hndl, cmd_p);
				}
			;

create_statement	: CREATE SETNAME exprlist NL	{ (void) set($2, $3); }
			| CREATE FCNNAME IDENT NL	{ fcn($2, $3); }
			;

source_statement	: SOURCE VALSTR NL	{ source_file($2); }
			| SOURCE IDENT NL	{ source_file($2); }
			;

listsets_statement	: LIST SETS NL		{ set_list(); }
			;

listhistory_statement	: LIST HISTORY NL	{ cmd_list(); }
			;

listfcns_statement	: LIST FCNS NL		{ fcn_list(); }
			;

			;

pfilter_statement	: PFILTER ON NL
					{ prbk_set_pfilter_mode(B_TRUE); }
			| PFILTER OFF NL
					{ prbk_set_pfilter_mode(B_FALSE); }
			| PFILTER ADD pidlist NL
					{ prbk_pfilter_add($3); }
			| PFILTER DELETE pidlist NL
					{ prbk_pfilter_drop($3); }
			| PFILTER NL
					{ prbk_show_pfilter_mode(); }
			;

ktrace_statement	: KTRACE ON NL
					{ prbk_set_tracing(B_TRUE); }
			| KTRACE OFF NL
					{ prbk_set_tracing(B_FALSE); }
			| KTRACE NL
					{ prbk_show_tracing(); }
			;

listprobes_statement	: LIST speclist PROBES SETNAME NL
						{ list_set($2, $4); }
			| LIST speclist PROBES exprlist NL
						{ list_expr($2, $4); }
			;

listvalues_statement	: LIST VALUES speclist NL { list_values($3); }
			;

exprlist		: /* empty */		{ $$ = NULL; }
			| exprlist expr		{ $$ = expr_list($1, $2); }
			;

speclist		: /* empty */		{ $$ = NULL; }
			| speclist spec		{ $$ = spec_list($1, $2); }
			;

expr			: spec EQ spec		{ $$ = expr($1, $3); }
			| spec			{ $$ = expr(spec(strdup("keys"),
							SPEC_EXACT), $1); }
			;

spec			: IDENT			{ $$ = spec($1, SPEC_EXACT); }
			| VALSTR		{ $$ = spec($1, SPEC_EXACT); }
			| REGEXP		{ $$ = spec($1, SPEC_REGEXP); }
			;

pidlist			: pidlist COMMA singlepid
				{ $$ = prbk_pidlist_add($1, $3); }
			| singlepid
				{ $$ = prbk_pidlist_add(NULL, $1); }
			;

singlepid		: SCALED_INT
			;

buffer_statement	: BUFFER NL
				{
				    prbk_buffer_list();
				}
			| BUFFER ALLOC NL
				{
				    extern int g_outsize;
				    prbk_buffer_alloc(g_outsize);
				}
			| BUFFER ALLOC SCALED_INT NL
				{
				    prbk_buffer_alloc($3);
				}
			| BUFFER DEALLOC NL
				{
				    prbk_buffer_dealloc();
				}
			;


%%
