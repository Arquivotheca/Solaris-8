/*
 * ident	"@(#)PrinterUtil.java	1.2	99/03/22 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * PrinterUtil class
 * Methods not associated with a printer instance.
 */

package com.sun.admin.pm.server;

import java.io.*;
import java.util.*;

public class  PrinterUtil {

    //
    // main for testing
    //
    public static void main(String[] args) {
	String dp = null;
	String devs[] = null;
	String printers[] = null;

        NameService ns = new NameService();
	try {
//		checkRootPasswd("xxx");
		dp = getDefaultPrinter(ns);
		devs = getDeviceList();
		printers = getPrinterList(ns);
	}
	catch (Exception e)
	{
		System.out.println(e);
		System.exit(1);
	}
	System.out.println("Default printer is:	" + dp);
	for (int i = 0; i < devs.length; i++) {
		System.out.println(devs[i]);
	}
	for (int i = 0; i < printers.length; i += 3) {
		System.out.println("printername:        " + printers[i]);
		System.out.println("servername:         " + printers[i+1]);
		System.out.println("comment:            " + printers[i+2]);
	}
	System.exit(0);
    }

    //
    // Get the default printer for a specified name space
    //
    public synchronized static String getDefaultPrinter(
	NameService ns) throws Exception
    {
	Debug.message("SVR: PrinterUtil.getDefaultPrinter()");

	String nsarg = ns.getNameService();
	String ret = DoPrinterUtil.getDefault(nsarg);
	if (ret == null) {
		return (new String(""));
	}
	return (new String(ret));
    }

    //
    // Get a list of possible printer devices for this machine.
    //
    public synchronized static String[] getDeviceList() throws Exception
    {
	Debug.message("SVR: PrinterUtil.getDeviceList()");

	String emptylist[] = new String[1];
	emptylist[0] = "";

	String ret[] = DoPrinterUtil.getDevices();
	if (ret == null) {
		return (emptylist);
	}
	return (ret);
    }

    //
    // Get a list of printers in the specified name service.
    //
    public synchronized static String[] getPrinterList(
	NameService ns) throws Exception
    {
	Debug.message("SVR: PrinterUtil.getPrinterList()");

	String emptylist[] = new String[1];
	emptylist[0] = "";

	String nsarg = ns.getNameService();
	String[] ret = DoPrinterUtil.getList(nsarg);
	if (ret == null) {
		return (emptylist);
	}
	return (ret);
    }

    //
    // Does this printer already exist in the specified
    // name service
    //
    public synchronized static boolean exists(
	String name,
	NameService ns) throws Exception
    {
	Debug.message("SVR: PrinterUtil.exists()");

	String nsname = ns.getNameService();
	return (DoPrinterUtil.exists(name, nsname));
    }

    public synchronized static boolean isLocal(
	String printername) throws Exception
    {
	Debug.message("SVR: PrinterUtil.isLocal()");

	return (DoPrinterUtil.isLocal(printername));
    }

    public synchronized static void checkRootPasswd(
	String passwd) throws Exception
    {
	DoPrinterNS.doCheckRootPasswd(passwd);
    }
}
