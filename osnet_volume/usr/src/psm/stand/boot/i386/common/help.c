/*
 * Copyright (c) 1997, Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident   "@(#)help.c 1.7     97/05/01 SMI"

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/salib.h>

extern int getchar();

/* help_cmd() - displays boot shell command reference screen */


/*ARGSUSED*/
void
help_cmd(int argc, char *argv[])
{
	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "           <<<<< Solaris x86 Boot Interpreter Commands >>>>>\n",
	    "============================= "
		"I/O Commands =========================\n",
	    "console                 Take input from console until CTRL-D.\n",
	    "echo [-n] arg1 ...      Display args; -n suppresses newline.\n",
	    "read var1 ... varn      Read line from console, assign to vars.\n",
	    "readt time v1 ... vn    Same as read, but timeout after x secs.\n",
	    "run name [arg1 ...]     Load and run "
		"standalone program, pass args.\n",
	    "source name             Read and interpret \"name\"d file.\n\n",
	    "=============== "
		"Variable/Property Handling Commands ================\n",
	    "set                     "
		"Display values of all current variables.\n",
	    "set name                Set \"name\"d variable to null string.\n",
	    "set name string         "
		"Set value of \"name\"d variable to \"string\".\n",
	    "set name <expr>         "
		"Set \"name\"d variable to value of \"expr\"ession.\n",
	    "unset name              Delete the \"name\"d variable.\n\n",
	    "getprop prop [var]      "
		"Assign value of \"prop\"erty to given \"var\"iable.\n",
	    "                        Print the value if \"var\" is omitted.\n",
	    "getproplen prop [var]   "
		"Assign length of \"prop\"erty value to the given\n",
	    "                        "
		"\"var\"iable.  Length includes terminating null.\n",
	    "setprop prop [string]"
		"   Set \"prop\"erty value to specified \"string\".\n",
	    "setbinprop prop list"
		"    Set \"prop\"erty to \"list\" of integers.  The\n",
	    "                        "
		"integers in the list must be separated by commas.\n\n",
	    "            <<< Listing paused; Press any key to continue >>>");

	(void) getchar();

	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "\r                                                              ",
	    "\n         "
		"<<<<< Solaris x86 Boot Interpreter Commands, cont'd. >>>>>\n",
	    "=============== "
		"Variable/Property Handling Commands ================\n",
	    "setenv prop string      "
		"Perform setprop that will persist across boots.\n",
	    "                        "
		"Note that a successful boot will be necessary\n",
	    "                        for the property to become persistent.\n",
	    "printenv [prop]         "
		"Display the value a persistent \"prop\"erty has\n",
	    "                        "
		"at boot.  A missing \"prop\"erty name implies all\n",
	    "                        persistent properties.\n",
	    ".properties             "
		"Display all properties of the active node.\n\n",
	    "========================="
		" Device Tree Commands ====================\n",
	    "dev [path]              Set the active device node.\n",
	    "pwd                     Display name of active device node.\n",
	    "ls                      "
		"Display nodes immediately below the active node.\n",
	    "mknod path [reg]        "
		"Create a new node (with given \"reg\" property) and\n",
	    "                        make it the active node.\n",
	    "show-devs [path]        "
		"Display all device nodes below the specified node\n",
	    "                        "
		"(or all device nodes if \"path\" is omitted).\n\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "            <<< Listing paused; Press any key to continue >>>");

	(void) getchar();

	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "\r                                                              ",
	    "\n         "
		"<<<<< Solaris x86 Boot Interpreter Commands, cont'd. >>>>>\n",
	    "========================="
		" Conditional Commands =====================\n",
	    "if <expr>               "
		"Execute next command group, if \"expr\" is true.\n",
	    "elseif <expr>           "
		"Takes effect only if \"expr\" is true, and\n",
	    "                        all preceding if's and elseif's failed.\n",
	    "else                    "
		"Execute next command group, if all else fails.\n",
	    "endif                   "
		"Marks the end of conditional statement block.\n\n",
	    "====================== "
		"String Handling Functions ===================\n",
	    ".strcmp ( str1, str2 ) "
		" Compare two strings; 0 if equal, <0, >0 otherwise.\n",
	    ".strncmp ( s1, s2, n ) "
		" Compare at most n characters of two strings.\n",
	    ".streq  ( str1, str2 ) "
		" Return true if \"str1\" equals \"str2\".\n",
	    ".strneq ( s1, s2, n )  "
		" Compare equality of \"n\" chars of two strings.\n",
	    ".strfind ( s, addr, n )"
		" Scan \"n\" locations in memory, looking for given\n",
	    "                        \"s\"tring and return true if found.\n\n",
	    "========================== "
		"Debugging Commands =====================\n",
	    "singlestep [on|off]     Turn singlestep mode on or off.\n",
	    "verbose [on|off]        Turn verbose mode on or off.\n\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "            <<< Listing paused; Press any key to continue >>>");

	(void) getchar();

	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "\r                                                              ",
	    "\n         <<<<< Solaris x86 "
		"Boot Interpreter Commands, cont'd. >>>>>\n",
	    "======================== "
		"Miscellaneous Commands ===================\n",
	    "claim addr len          "
		"Reserve \"len\" bytes at given \"addr\"ess.\n",
	    "release addr            "
		"Release previously claimed memory.\n",
	    "help                    Display this help screen.\n",
	    "setcolor fg bg          "
		"Set text mode foreground/background attributes;\n",
	    "                        "
		"allowable colors are: black blue green cyan red\n",
	    "                        "
		"magenta brown white gray lt_blue lt_green lt_cyan\n",
	    "                        lt_red lt_magenta yellow hi_white\n\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n",
	    "\n");
}
