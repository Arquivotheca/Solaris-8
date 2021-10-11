/*
 * ident	"@(#)StderrLog.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

package com.sun.slp;

import java.io.*;

/**
 * A logging class which writes to stderr. This class can be dynamically
 * loaded by SLPConfig and used as the log object by the writeLog and
 * writeLogLine methods.
 *
 * This class does not actually write anything until the flush() method
 * in invoked; this will write the concatenation of all messages
 * passed to the write() method since the last invokation of flush().
 *
 * The actual logging class used can be controlled via the
 * sun.net.slp.loggerClass property.
 *
 * See also the SLPLog (in slpd.java) and Syslog classes.
 */

class StderrLog extends Writer {

    private StringBuffer buf;

    public StderrLog() {
	buf = new StringBuffer();
    }

    public void write(char[] cbuf, int off, int len) throws IOException {
	buf.append(cbuf, off, len);
    }

    public void flush() {
	String date = SLPConfig.getDateString();

	System.err.println("********" +
			   date + "\n" +
			   buf + "\n" +
			   "********\n");
	buf = new StringBuffer();
    }

    public void close() {
    }
}
