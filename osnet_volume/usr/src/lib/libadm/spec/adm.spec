#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)adm.spec	1.2	99/05/14 SMI"
#
# lib/libadm/spec/adm.spec

# OA&M Device Managment
function	devattr
declaration	char *devattr( char *device, char *attribute)
version		SUNW_0.7
end		

# OA&M Device Managment
function	devfree
declaration	int devfree(int key, char *device)
version		SUNW_0.7
end		

# OA&M Device Managment
function	devreserv
declaration	char **devreserv(int key, char **rsvlist[])
version		SUNW_0.7
end		

# OA&M Device Managment
function	getdev
declaration	char **getdev(char **devices, char **criteria, int options)
version		SUNW_0.7
end		

# OA&M Device Managment
function	getdgrp
declaration	char **getdgrp(char **dgroups, char **criteria, int options)
version		SUNW_0.7
end		

# OA&M Device Managment
function	getvol
declaration	int getvol(char *device, char *label, int options, char *prompt)
version		SUNW_0.7
end		

# OA&M Device Managment
function	listdev
declaration	char **listdev(char *device)
version		SUNW_0.7
end		

# OA&M Device Managment
function	listdgrp
declaration	char **listdgrp(char *dgroup)
version		SUNW_0.7
end		

# OA&M Device Managment
function	reservdev
include		<sys/types.h>, <devmgmt.h>
declaration	struct reservdev **reservdev(void)
version		SUNW_0.7
end		

# VTOC reading/writing
function	read_vtoc
include		<sys/types.h>, <sys/vtoc.h>
declaration	int read_vtoc(int fd, struct vtoc *vtoc)
version		SUNW_0.7
exception	$return < 0
end		

# VTOC reading/writing
function	write_vtoc
include		<sys/types.h>, <sys/vtoc.h>
declaration	int write_vtoc(int	fd, struct vtoc	*vtoc)
version		SUNW_0.7
exception	$return < 0
end		

# Regular Expressions
function	advance  extends libgen/spec/gen.spec
version		SUNW_0.7
end		

# Regular Expressions
data		circf
version		SUNW_0.7
end		

# Regular Expressions
function	compile extends libgen/spec/gen.spec compile
version		SUNW_0.7
end		

# Regular Expressions
data		loc1
version		SUNW_0.7
end		

# Regular Expressions
data		loc2
version		SUNW_0.7
end		

# Regular Expressions
data		locs
version		SUNW_0.7
end		

# Regular Expressions
data		nbra
version		SUNW_0.7
end		

# Regular Expressions
data		sed
version		SUNW_0.7
end		

# Regular Expressions
function	step  extends libgen/spec/gen.spec
version		SUNW_0.7
end		

# Packaging Stuff
data		pkgdir
version		SUNW_0.7
end		

# Packaging Stuff
function	pkginfo
include		<pkginfo.h>, <valtools.h>
declaration	int pkginfo(struct pkginfo *info, char *pkginst, ...)
version		SUNW_0.7
end		

# Packaging Stuff
function	pkgnmchk
include		<pkginfo.h>, <valtools.h>
declaration	int pkgnmchk(char *pkg, char *spec, int presvr4flg)
version		SUNW_0.7
end		

# Packaging Stuff
function	pkgparam
include		<pkginfo.h>, <valtools.h>
declaration	char *pkgparam( char *pkg, char *param)
version		SUNW_0.7
end		

# Obsolete Physical Memory Information
function	asysmem
include		
declaration	long asysmem(void)
version		SUNW_0.7
errno		
exception	$return == -1
end		

# Obsolete Physical Memory Information
function	sysmem
include		
declaration	long sysmem(void)
version		SUNW_0.7
errno		
exception	$return == -1
end		

# cmd/oampkg/pkgadd
function	fpkginfo
declaration	int fpkginfo(struct pkginfo *info, char *pkginst)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
data		ckquit
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	ckpath
declaration	int ckpath(char *pathval, int pflags, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	ckyorn
declaration	int ckyorn(char *yorn, char *defstr, char *error, \
			char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	_getvol
declaration	int _getvol(char *device, char *label, int options, \
			char *prompt, char *norewind)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	ckitem
declaration	int ckitem(CKMENU *menup, char *item[], short max, \
			char *defstr, char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	fpkginst
declaration	char *fpkginst(char *pkg, ... )
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	get_PKGADM
declaration	char *get_PKGADM(void)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	set_PKGpaths
declaration	void set_PKGpaths(char *path)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	fpkgparam
declaration	char *fpkgparam(FILE *fp, char *param)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	allocmenu
declaration	CKMENU *allocmenu(char *label, int attr)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	setinvis
declaration	int setinvis(CKMENU *menup, char *choice)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	puttext
declaration	int puttext(FILE *fp, char *str, int lmarg, int rmarg)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgadd
function	setitem
declaration	int setitem(CKMENU *menup, char *choice)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkginstall
function	get_PKGOLD
declaration	char * get_PKGOLD(void)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkginstall
function	get_PKGLOC
declaration	char * get_PKGLOC(void)
version		SUNWprivate_1.1
end		

function	set_PKGADM
declaration	void set_PKGADM(char *newpath)
version		SUNWprivate_1.1
end		

function	set_PKGLOC
declaration	void set_PKGLOC(char *newpath)
version		SUNWprivate_1.1
end		

function	getinput
declaration	int getinput(char *s)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkginstall
function	printmenu
declaration	void printmenu(CKMENU *menup)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgrm
function	ckstr
declaration	int ckstr(char *strval, char *regexp[], int length, \
			char *defstr, char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/oampkg/pkgparam
data		pkgfile
version		SUNWprivate_1.1
end		

# cmd/volmgt/util
function	getfullrawname
declaration	char * getfullrawname(char *cp)
version		SUNWprivate_1.1
end		

# cmd/volmgt/util
function	getfullblkname
declaration	char * getfullblkname(char *cp)
version		SUNWprivate_1.1
end		

# cmd/devmgt/de
function	_devtabpath
declaration	char * _devtabpath(void)
version		SUNWprivate_1.1
end		

# cmd/devmgt/de
function	_opendevtab
declaration	int _opendevtab(char *mode)
version		SUNWprivate_1.1
end		

# cmd/devmgt/de
function	_rsvtabpath
declaration	char * _rsvtabpath(void)
version		SUNWprivate_1.1
end		

# cmd/devmgt/ge
data		ckwidth
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdgrp
function	_dgrptabpath
declaration	char * _dgrptabpath(void)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdgrp
function	_rmdgrpmems
declaration	int _rmdgrpmems( char   *dgrp, char  **mems, char ***notfounds)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdgrp
function	_rmdgrptabrec
declaration	int _rmdgrptabrec(char *dgrp)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdgrp
function	_adddgrptabrec
declaration	int _adddgrptabrec( char   *dgrp, char  **members)
version		SUNWprivate_1.1
end		

# cmd/devmgmt/libstrgrp
function	_opendgrptab
declaration	int _opendgrptab(char *mode)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdev
function	_rmdevtabrec
declaration	int _rmdevtabrec(char *device)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdev
function	_adddevtabrec
declaration	int _adddevtabrec( char   *alias, char  **attrval)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdev
function	_rmdevtabattrs
declaration	int _rmdevtabattrs( char   *device, char  **attributes, char ***notfounds)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdev
function	_validalias
declaration	int _validalias(char   *alias)
version		SUNWprivate_1.1
end		

# cmd/devmgt/putdev
function	_moddevtabrec
declaration	int _moddevtabrec( char *device, char **attrval)
version		SUNWprivate_1.1
end		

# cmd/devmgmt/mkdtab
function	_enddevtab
declaration	void _enddevtab(void)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckint
declaration	int ckint(long *intval, short base, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckint_hlp
declaration	void ckint_hlp(short base, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckint_val
declaration	int ckint_val(char *value, short base)
version		SUNWprivate_1.1
end		

# cmd/valtools
data		ckindent
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckint_err
declaration	void ckint_err(short base, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckitem_hlp
declaration	void ckitem_hlp(CKMENU *menup, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckitem_err
declaration	void ckitem_err(CKMENU *menup, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckpath_stx
declaration	int ckpath_stx(int pflags)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckpath_hlp
declaration	void ckpath_hlp(int pflags, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckpath_val
declaration	int ckpath_val(char *path, int pflags)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckpath_err
declaration	void ckpath_err(int pflags, char *error, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckrange_hlp
declaration	void ckrange_hlp(long lower, long upper, int base, char *help)
version		SUNWprivate_1.1
end		
# cmd/valtools

function	ckrange_err
declaration	void ckrange_err(long lower, long upper, int base, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckrange_val
declaration	int ckrange_val(long lower, long upper, int base, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckrange
declaration	int ckrange(long *rngval, long lower, long upper, \
			short base, char *defstr, char *error, \
			char *help, char *prompt)
# cmd/valtools
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckstr_hlp
declaration	void ckstr_hlp(char *regexp[], int length, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckstr_val
declaration	int ckstr_val(char *regexp[], int length, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckstr_err
declaration	void ckstr_err(char *regexp[], int length, \
			char *error, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckyorn_hlp
declaration	void ckyorn_hlp(char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckyorn_val
declaration	int ckyorn_val(char *str)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckyorn_err
declaration	void ckyorn_err(char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckkeywd
declaration	int ckkeywd(char *strval, char *keyword[], \
			char *defstr, char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckdate_hlp
declaration	int ckdate_hlp(char *fmt, char *hlp)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckdate_val
declaration	int ckdate_val(char *fmt, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckdate
declaration	int ckdate(char *date, char *fmt, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckdate_err
declaration	int ckdate_err(char *fmt, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	cktime_hlp
declaration	int cktime_hlp(char *fmt, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	cktime_val
declaration	int cktime_val(char *fmt, char *input)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	cktime_err
declaration	int cktime_err(char *fmt, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	cktime
declaration	int cktime(char *tod, char *fmt, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckuid
declaration	int ckuid(char *uid, short disp, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckuid_hlp
declaration	void ckuid_hlp(int disp, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckuid_dsp
declaration	int ckuid_dsp(void)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckuid_val
declaration	int ckuid_val(char *usrnm)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckuid_err
declaration	void ckuid_err(short disp, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckgid_err
declaration	void ckgid_err(int disp, char *error)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckgid
declaration	int ckgid(char *gid, short disp, char *defstr, \
			char *error, char *help, char *prompt)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckgid_hlp
declaration	void ckgid_hlp(int disp, char *help)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckgid_dsp
declaration	int ckgid_dsp(void)
version		SUNWprivate_1.1
end		

# cmd/valtools
function	ckgid_val
declaration	int ckgid_val(char *grpnm)
version		SUNWprivate_1.1
end		

# required by pkginfo
function	pkginfofind
declaration	int pkginfofind(char *path, char *pkg_dir, char *pkginst)
version		SUNWprivate_1.1
end		

function	puterror
declaration	void puterror(FILE *fp, char *defmesg, char *error)
version		SUNWprivate_1.1
end		

function	puthelp
declaration	void puthelp(FILE *fp, char *defmesg, char *help)
version		SUNWprivate_1.1
end		

function	putprmpt
declaration	void putprmpt(FILE *fp, char *prompt, \
			char *choices[], char *defstr)
version		SUNWprivate_1.1
end		

