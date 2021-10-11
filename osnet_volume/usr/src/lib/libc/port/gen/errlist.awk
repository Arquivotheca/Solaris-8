#ident	"@(#)errlist.awk	1.8	99/04/20 SMI"	/* SVr4.0 1.1	*/

# create two files from a list of input strings,
# new_list.c contains an array of characters indexed into by perror and strerror,
# errlst.c contains an array of pointers to strings, for compatibility
# with existing user programs that reference it directly

# WARNING!
#        Do NOT add entries to this list such that it grows the list
#        beyond the last entry:
#              151     Stale NFS file handle
#        Growing this list may damage programs because this array is
#        copied into a reserved array at runtime.  See bug 4097669.
#
#        If you need to add an entry please use one of the empty
#        slots.
#        The arrays _sys_errs[], accessible via perror(3C) and strerror(3C)
#        interfaces, and sys_errlist[] are created from this list.
#        It is the direct referencing of sys_errlist[] that is the problem.
#        Your code should only use perror() or strerror().


BEGIN	{
		FS = "\t"
		hi = 0

		newfile = "new_list.c"
		oldfile = "errlst.c"

		print "#ident\t\"@(#)errlist.awk\t1.2\t90/08/16 SMI\"\n" >oldfile
		print "/*LINTLIBRARY*/" >oldfile
		print "#ifdef __STDC__" >oldfile
		print "\t#pragma weak sys_errlist = _sys_errlist" >oldfile
		print "\t#pragma weak sys_nerr = _sys_nerr" >oldfile
		print "#endif" >oldfile
		print "#include \"synonyms.h\"\n" >oldfile
		print "const char *sys_errlist[] = {" >oldfile

		print "#ident\t\"@(#)errlist.awk\t1.2\t90/08/16 SMI\"\n" >newfile
		print "/*LINTLIBRARY*/" >newfile
		print "#include \"synonyms.h\"\n" >newfile
	}

/^[0-9]+/ {
		if ($1 > hi)
			hi = $1
		astr[$1] = $2
	}

END	{
		print "const int _sys_index[] =\n{" >newfile
		k = 0
		mx = 151	# max number of entries for sys_errlist[]
		if (hi > mx)
		{
			printf "awk: ERROR! sys_errlist[] > %d entries\n", mx
			printf "Please read comments in"
			printf " usr/src/lib/libc/port/gen/errlist\n"
			exit 1
		}
		for (j = 0; j <= hi; ++j)
		{
			if (astr[j] == "")
				astr[j] = sprintf("Error %d", j)
			printf "\t%d,\n", k >newfile
			k += length(astr[j]) + 1
		}
		print "};\n" >newfile

		print "const char _sys_errs[] =\n{" >newfile
		for (j = 0; j <= hi; ++j)
		{
			print "\t\"" astr[j] "\"," >oldfile
			printf "\t" >newfile
			n = length(astr[j])
			for (k = 1; k <= n; ++k)
				printf "'%s',", substr(astr[j],k,1) >newfile
			print "'\\0'," >newfile
		}
		print "};\n" >newfile
		print "};\n" >oldfile

		print "const int _sys_num_err = " hi + 1 ";" >newfile
		print "const int sys_nerr = " hi + 1 ";" >oldfile
	}
