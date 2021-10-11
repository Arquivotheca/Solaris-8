$ *************************************<+>*************************************
$ *****************************************************************************
$ **
$ **  File:        pam_ldap.msg
$ **
$ **  Project:     pam
$ **
$ **  Description:
$ **  -----------
$ **  This file is the source for the pam_ldap message catalog.  
$ **
$ **  It contains messages for the pam_ldap service module (set 1).  
$ **
$ *****************************************************************************
$ **
$ **     (c) Copyright 1999 Sun Microsystems, Inc.
$ **     All Rights reserved
$ **
$ *****************************************************************************
$ *************************************<+>*************************************

$ *****************************************************************************
$
$ ***** NOTE FOR MESSAGE CATALOG TRANSLATORS *****
$
$ There may be three types of messages in this file:
$
$  1. Messages that appear in dialogs or are displayed to the user.
$
$     These messages are the default and they should ALL BE LOCALIZED.
$     Note that these messages do NOT have any identification (see the
$     comments for type 2 and 3 below).
$
$  2. Messages that only appear in the error log file. 
$
$     The localization of these messages is OPTIONAL.  These messages are
$     identified by the following:
$
$  MESSAGES xx-yy IN SET zz WILL ONLY APPEAR IN THE DT ERRORLOG FILE
$
$  3. Messages that should not be localized.
$
$     These messages are identified by the following:
$
$  DO NOT TRANSLATE or CHANGE or LOCALIZE MESSAGES xx-yy from set zz
$
$ ***** END (NOTE FOR MESSAGE CATALOG TRANSLATORS) *****
$
$ ******************************************************************************

$ ldap module messages 
$set 1

$ update_password.c
$ Msg 1 thru 10 print on consecutive lines.
$ Keep indentation for "passwd:" in messages 2 thru 9 

1 Supported configurations for passwd management are as follows:
2     passwd: files
3     passwd: files ldap
4     passwd: files nis
5     passwd: files nisplus
6     passwd: compat
7     passwd: compat AND
8     passwd_compat: ldap OR
9     passwd_compat: nisplus
10 Please check your /etc/nsswitch.conf file
11 %s%s:  %s does not exist
12 Can't find name service for passwd
13 Enter login(LDAP) password:
14 %s%s: Sorry.
15 %s%s: Sorry, wrong passwd
16 New password: 
17 System error: no LDAP passwd record for %s
18 %s %s: Couldn't change passwd for %s
19 LDAP passwd changed for %s
20 Re-enter new password: 
21 %s%s: Too many tries; try again
22 %s%s: They don't match; try again.
23 %s%s: Too many failures; try later.
24 %s%s: Password too short; must be at least 6 characters.
25 %s%s: Password cannot be circular shift of login id.
26 %s%s: The first %d characters of the password
27 must contain at least two alphabetic characters and at least
28 one numeric or special character.
29 %s%s: Passwords must differ by at least 3 positions

$ ldap_authenticate.c
30 LDAP Password: 
31 Password: 

$ ldap_utils.c
32 %s %s: Permission denied
