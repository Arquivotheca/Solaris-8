/*
 * ident	"@(#)PrinterDebug.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * PrinterDebug class
 * Helper class to print attributes in objects.
 */

package com.sun.admin.pm.server;

import java.io.*;
import java.util.*;

public class  PrinterDebug {

    public static void printObj(Printer p)
    {
	String arr[];
	int i;

	Debug.message("SVR: PrinterDebug.print_obj(Printer)");

	Debug.message("SVR: \tName:\t\t" + p.getPrinterName());
	Debug.message("SVR: \tType:\t\t" + p.getPrinterType());
	Debug.message("SVR: \tServer:\t\t" + p.getPrintServer());
	Debug.message("SVR: \tComment:\t" + p.getComment());
	Debug.message("SVR: \tDevice:\t\t" + p.getDevice());
	Debug.message("SVR: \tNotify:\t\t" + p.getNotify());
	Debug.message("SVR: \tProtocol:\t" + p.getProtocol());
	Debug.message("SVR: \tDest:\t\t" + p.getDestination());
	Debug.message("SVR: \tExtensions:\t" + p.getExtensions());
	Debug.message("SVR: \tDefault:\t" + p.getIsDefaultPrinter());
	Debug.message("SVR: \tBanner:\t\t" + p.getBanner());
	Debug.message("SVR: \tEnable:\t\t" + p.getEnable());
	Debug.message("SVR: \tAccept:\t\t" + p.getAccept());

	arr = p.getFileContents();
	if (arr == null) {
		Debug.message("SVR: \tContents:\tNULL");
	} else {
		Debug.message("SVR: \tContents:");
		for (i = 0; i < arr.length; i++) {
			Debug.message("SVR: \t\t\t" + arr[i]);
		}
	}
	arr = p.getUserAllowList();
	if (arr == null) {
		Debug.message("SVR: \tUser allow:\tNULL");
	} else {
		Debug.message("SVR: \tUser allow:");
		for (i = 0; i < arr.length; i++) {
			Debug.message("SVR: \t\t\t" + arr[i]);
		}
	}
	arr = p.getUserDenyList();
	if (arr == null) {
		Debug.message("SVR: \tUser deny:\tNULL");
	} else {
		Debug.message("SVR: \tUser deny:");
		for (i = 0; i < arr.length; i++) {
			Debug.message("SVR: \t\t\t" + arr[i]);
		}
	}
    }

    public static void printObj(NameService ns)
    {
	Debug.message("SVR: PrinterDebug.printObj(NameService)");

	// Keep passwd secret.
	String passwd = ns.getPasswd();
	if (passwd != null) {
		passwd = "*****";
	}
	Debug.message("SVR: \tnameservice:\t" + ns.getNameService());
	Debug.message("SVR: \tnshost:\t\t" + ns.getNameServiceHost());
	Debug.message("SVR: \tuser:\t\t" + ns.getUser());
	Debug.message("SVR: \tpasswd:\t\t" + passwd);
	Debug.message("SVR: \tboundtonisslave:" + ns.getBoundToNisSlave());
	Debug.message("SVR: \tisAuth:\t\t" + ns.isAuth());
    }

    public static String arr_to_str(String[] arr)
    {
	if (arr == null) {
		return (new String("NULL"));
	}
	if (arr.length == 0) {
		return (new String("0 length array"));
	}
	String str = "";
	for (int i = 0; i < arr.length; i++) {
		if (arr[i] == null) {
			break;
		}
		str = str.concat(arr[i] + " ");
	}
	return (str);
    }
}
