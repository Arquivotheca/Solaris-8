/*
 * ident	"@(#)PermSARegTable.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)PermSARegTable.java	1.2	05/10/99
//  PermSARegTable.java: Periodically reregister registrations.
//  Author:           James Kempf
//  Created On:       Thu May 14 14:11:49 1998
//  Last Modified By: James Kempf
//  Last Modified On: Thu Jan 28 14:53:43 1999
//  Update Count:     36
//

package com.sun.slp;

import java.net.*;
import java.io.*;
import java.util.*;

/**
 * Periodically reregister advertisments in the SA client.
 *
 * @version 1.2 00/10/14
 * @author Erik Guttman, James Kempf
 */

class PermSARegTable extends Thread {

    private Hashtable htRegs;
    private SLPConfig config;
    private final static long INCREMENT = Defaults.lMaxSleepTime / 2L;
								// 9 hours...
    private final static long SLEEPY_TIME = INCREMENT / 2L;
						// 4 hours, more or less...

    // We use these indicies for record access. We should use a class
    //  here, but it's another 1K!

    private final static int TIME = 0;
    private final static int REG = 1;

    PermSARegTable(SLPConfig config) {
	htRegs = new Hashtable();
	this.config = config;
	start();
    }

    // We just lock the hashtable when we need to update. Otherwise, we
    //  get into deadlock if an outgoing request is being made when
    //  somebody else wants to get into this class to look something
    //  up.

    void reg(ServiceURL URL, CSrvReg sr) {

	// Make up a record for the table.

	Object[] rec =
	    new Object[] {
	    new Long(System.currentTimeMillis() + INCREMENT),
		sr};

	// Note that we do not account for multiple nonservice: URLs under
	// separate type names, because that is disallowed by the protocol.

	htRegs.put(URL, rec);
    }

    // Remove

    void dereg(ServiceURL URL) {
	htRegs.remove(URL);

    }

    // Send off the vector of registations for expired advertisements.

    private void send(SrvLocMsg reg) {
	InetAddress addr = config.getLoopback();

	try {
	    Transact.transactTCPMsg(addr, reg, true);

	} catch (ServiceLocationException ex) {

	    config.writeLog("periodic_exception",
			    new Object[] {new Short(ex.getErrorCode()),
					      ex.getMessage()});
	} catch (IllegalArgumentException iae) {
	    Assert.assert(false, "reregister_bug", new Object[0]);

	}
    }

    // Walk the registration table, collecting registrations
    //  to reregister. We synchronize on this method to close
    //  the window between when the table is walked and
    //  when the registration is sent
    //  during which the client may deregister the URL but
    //  it is reregistered anyway.

    private synchronized void walk() {
	Enumeration e;
	long lnow = System.currentTimeMillis();

	e = htRegs.keys();

	while (e.hasMoreElements()) {
	    ServiceURL url = (ServiceURL)e.nextElement();
	    Object[] rec = (Object[])htRegs.get(url);
	    long xtime = ((Long)rec[TIME]).longValue();

	    // If the deadline to refresh passed, then do it.

	    if (xtime <= lnow) {
		send((SrvLocMsg)rec[REG]);
		rec[TIME] = new Long(lnow + INCREMENT);
	    }
	}

    }

    public void run() {

	setName("SLP PermSARegTable");

	while (true) {

	    try {

		// Sleep for half the reregistration interval (which itself
		//  is half the lifetime of the URLs.

		sleep(SLEEPY_TIME);

	    } catch (InterruptedException ie) {

	    }

	    walk();

	}
    }
}
