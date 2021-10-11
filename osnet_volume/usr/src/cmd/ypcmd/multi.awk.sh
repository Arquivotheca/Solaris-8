#!/usr/bin/nawk -f
#
#ident	"@(#)multi.awk.sh	1.4	96/04/25 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# Awk code to handle the creation of the YP_MULTI_ entries
# in the hosts.byname map.  Called by multi directly.
#

{
    # Here we loop through the list of hostnames
    # doing two separate things...
    # First, we're building a list of hostnames
    # for the current IP address ($1).
    # Second, if we've seen a name before then
    # we add the current address ($1) to a list
    # of address associated with this particular
    # name ($i).
    #
    # Note, that we're pretty careful about keeping
    # out duplicates (and this has a cost).

    for (i = 2; i <= NF; i++) {
	# Make the namelist for this address
	if (namelist[$1] == "") {
	    namelist[$1] = $i;
	} else if (namelist[$1] == $i) {
	    ;
	} else if (index(namelist[$1], $i) == 0) {
	    namelist[$1] = namelist[$1] " " $i;
	} else {
	    nf = 1;
	    numnames = split(namelist[$1], n);
	    for (j = 1; j <= numnames; j++) {
		if (n[j] == $i) {
		    nf = 0;
		    break;
		}
	    }
	    if (nf) {
		namelist[$1] = namelist[$1] " " $i;
		nf = 0;
	    }
	}

	# Do we have an address for this name?
        # If not, and it's not already there, add it.
	if (addr[$i] == "") {
	    addr[$i] = $1;
	} else if (index(addr[$i], $1) == 0) {
	    addr[$i] = addr[$i] "," $1
	}
    }
}

END {
    # There are now a bunch o addresses in the addr
    # array that are actually lists.  We go through
    # all of them here and build a list of hostname
    # aliases into the namelist array.
    #
    for (host in addr) {
	if (index(addr[host], ",") == 0)
	    continue;
	numaddr = split(addr[host], tmpaddr, ",");
	for (i = 1; i <= numaddr; i++) {
	    numnames = split(namelist[tmpaddr[i]], tmpname);
	    for (j = 1; j <= numnames; j++) {
		if (namelist[addr[host]] == "") {
		    namelist[addr[host]] = tmpname[j];
		    continue;
		}
		if (namelist[addr[host]] == tmpname[j]) {
		    continue;
		}
		if (index(namelist[addr[host]], tmpname[j]) == 0) {
		    namelist[addr[host]] = namelist[addr[host]] " " tmpname[j];
		    continue;
		} else {
		    nf = 1;
		    for (k = 1; k <= numnames; k++) {
			if (tmpname[j] == tmpname[k]) {
			    nf = 0;
			    break;
			}
		    }
		    if (nf == 1) {
			namelist[addr[host]] = namelist[addr[host]] " " tmpname[j];
			nf = 1;
		    }
		}
	    }
	}
    }

    # Now do that funky output thang...
    for (host in addr) {
	if (index(addr[host], ",")) {
	    printf("YP_MULTI_");
	}
	printf("%s %s\t%s\n",
	       host, addr[host], namelist[addr[host]]);
    }
}
