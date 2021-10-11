/*
 *
 * ident	"@(#)pmCalls.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmCalls.java
 * Debug messages
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.applet.*;
import java.io.*;
import java.util.*;
import javax.swing.*;

import com.sun.admin.pm.server.*;


/*
 * Class of calls to backend
 */

public class pmCalls {

/*
 * Debugging routines
 */

    public static void testout(String out) {
	Debug.info(out);
    }

    public static void debugShowPrinter(Printer p) {
	Debug.info("CLNT:  debugShowPrinter");

        if (p.getPrinterName() != null) {
            Debug.info("CLNT:  printer" +
                            p.getPrinterName());
        }
 
        if (p.getPrintServer() != null)
            Debug.info("CLNT:  server " +
                             p.getPrintServer());
 
        if (p.getPrinterType() != null)
            Debug.info("CLNT:  printer type " +
                            p.getPrinterType());
 
        if (p.getComment() != null)
            Debug.info("CLNT:  Comment " +
                            p.getComment());
 
        if (p.getDevice() != null)
            Debug.info("CLNT:  Device " +
                            p.getDevice());
 
        if (p.getNotify() != null)
            Debug.info("CLNT:  Notify " +
                            p.getNotify());
 
        if (p.getProtocol() != null)
            Debug.info("CLNT:  Protocol " +
                            p.getProtocol());
                             
        if (p.getDestination() != null)
            Debug.info("CLNT:  Destination " +
                            p.getDestination());
 
        if (p.getFileContents() != null) {
    
            String filedata[] = p.getFileContents();
            String filecontents = new String();
 
	    Debug.info("File Contents: ");

            if (filedata != null) {
		for (int i = 0; i < filedata.length; i++) {
			Debug.info(filedata[i]);
		}
	    }
        }

	if (p.getNotify() != null) {
	    Debug.info("CLNT:  Fault Notification: " + p.getNotify());
	}
 
	String ua[] = p.getUserAllowList();
        Debug.info("CLNT:  UserAllowList ");
        if (ua != null) {
		for (int i = 0; i < ua.length; i++) {
			Debug.info(ua[i]);
		}
	}
 
        Debug.info("CLNT:  getIsDefaultPrinter is " + p.getIsDefaultPrinter());
	Debug.info("CLNT:  getBanner is " + p.getBanner());
 
    }

    public static void debugshowPrinterList(NameService ns) {

	String[] list;

	try {
		list = PrinterUtil.getPrinterList(ns);
		for (int i = 0; i < list.length; i++)
			Debug.info("CLNT:  " + list[i]);
	} catch (Exception e) {
		Debug.info("CLNT: debugshowPrinterList(): exception " + e);
	}

    }

}
