/*
 * ident	"@(#)DoPrinterUtil.java	1.4	99/08/04 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * DoPrinterUtil class
 * Worker utility class.
 */

package com.sun.admin.pm.server;

import java.io.*;
import java.util.*;

public class  DoPrinterUtil {

    public static String getDefault(String ns) throws Exception
    {
	Debug.message("SVR: DoPrinterUtil.getDefault()");
	Debug.message("SVR: name service equals " + ns);

	String o = null;
	String cmd = "/usr/bin/lpget -n " + ns + " _default";
	SysCommand syscmd = new SysCommand();
	syscmd.exec(cmd);
	o = syscmd.getOutput();
	syscmd = null;

	if (o == null) {
		return (null);
	}
	int i = o.indexOf("use=");
	if (i == -1) {
		return (null);
	}
	o = o.substring((i+4));
	o = o.trim();

	Debug.message("SVR: default is " + o);
	return (new String(o));
    }

    public static String[] getDevices() throws Exception
    {
	Debug.message("SVR: DoPrinterUtil.getDevices()");

	int i = 0;
	String dev = "";
	String devices = "";

	String serial_possibilities[] = {"a", "b", "c", "d",
		"e", "f", "g", "h", "i", "j", "k", "l", "m",
		"n", "o", "p", "q", "r", "s", "t", "u", "v",
		"w", "x", "y", "z"};

	String cmd = "/usr/bin/find /dev -print";
	SysCommand syscmd = new SysCommand();
	syscmd.exec(cmd);
	if (syscmd.getExitValue() != 0) {
		String errstr = syscmd.getError();
		syscmd = null;
		throw new pmCmdFailedException(errstr);
	}

	String o = syscmd.getOutput();
	syscmd = null;

	if (o == null) {
		return (null);
	}

	for (i = 0; i < serial_possibilities.length; i++) {
		dev = "/dev/term/" + serial_possibilities[i] + "\n";
		if (o.indexOf(dev) != -1) {
			devices = devices.concat(" " + dev + " ");
		}
	}
	// sparc bpp parallel ports
	for (i = 0; i < 100; i++) {
		dev = "/dev/bpp" + i + "\n";
		if (o.indexOf(dev) != -1) {
			devices = devices.concat(" " + dev + " ");
		}
	}
	// sparc ecpp parallel ports
	for (i = 0; i < 100; i++) {
		dev = "/dev/ecpp" + i + "\n";
		if (o.indexOf(dev) != -1) {
			devices = devices.concat(" " + dev + " ");
		}
	}
	// intel parallel ports
	for (i = 0; i < 100; i++) {
		dev = "/dev/lp" + i + "\n";
		if (o.indexOf(dev) != -1) {
			devices = devices.concat(" " + dev + " ");
		}
	}
	// SunPics
	dev = "/dev/lpvi\n";
	if (o.indexOf(dev) != -1) {
		devices = devices.concat(" " + dev + " ");
	}

	o = null;

	if (devices.equals("")) {
		return (null);
	}

	String ret[];
	StringTokenizer st = new StringTokenizer(devices);
	if (st.countTokens() == 0) {
		return (null);
	} else {
		ret = new String[st.countTokens()];
		for (i = 0; st.hasMoreTokens(); i++) {
			ret[i] = st.nextToken();
		}
	}
	return (ret);
    }

    public static String[] getList(String nsarg)
	throws Exception
    {
	Debug.message("SVR: DoPrinterUtil.getList()");

	int i = 0;
	int j = 0;
	int listi = 0;

	String cmd = null;
	String printername = "";
	String printserver = "";
	String comment = "";
	String nameservice;
	String list[];

	String o = null;
	cmd = "/usr/bin/lpget -n " + nsarg + " list";
	SysCommand syscmd = new SysCommand();
	syscmd.exec(cmd);
	if (syscmd.getExitValue() != 0) {
		String errstr = syscmd.getError();
		syscmd = null;
		throw new pmCmdFailedException(errstr);
	}
	o = syscmd.getOutput();
	syscmd = null;

	if (o == null) {
		return (null);
	}

	// Count entries
	int index = 0;
	while ((index = o.indexOf("bsdaddr=", index)) != -1) {
		index = index + 8;
		i++;
	}
	if (i <= 0)
		return (null);

	list = new String [i*3];

	int colon = 0;
	int nextcolon = 0;
	while ((colon = o.indexOf(":", colon + 1)) != -1) {
		nextcolon = o.indexOf(":", colon + 1);
		if (nextcolon == -1)
			nextcolon = o.length();
		// Extract printername
		i = colon;
		while ((o.charAt(i) != '\n') && (i != 0)) {
			i--;
		}
		if (i == 0)
			printername = o.substring(i, colon);
		else
			printername = o.substring(i + 1, colon);

		// Skip _all and _default keywords
		if (printername.equals("_all")) {
			continue;
		}
		if (printername.equals("_default")) {
			continue;
		}

		// Extract servername
		i = o.indexOf("bsdaddr=", colon);
		if ((i != -1) && (i < nextcolon)) {
			j = o.indexOf(",", i);
			if (j != -1)
				printserver = o.substring(i + 8, j);
		}
		// Skip entries without a server.
		if (printserver.equals("")) {
			Debug.warning(
			    "SVR: printer does not have a server: "
			    + printername);
			continue;
		}

		// Extract description
		i = o.indexOf("description=", colon);
		if ((i != -1) && (i < nextcolon)) {
			j = i;
			while (j < o.length()) {
				if (o.charAt(j) == '\n')
					break;
				j++;
			}
			comment = o.substring(i + 12, j);
		}

		list[listi++] = printername;
		list[listi++] = printserver;
		list[listi++] = comment;
		printername = "";
		printserver = "";
		comment = "";
	}
	return (list);
    }

    public static boolean exists(
	String name,
	String ns) throws Exception
    {
	int exitvalue;

	Debug.message("SVR: DoPrinterUtil.exists() " + ns);

	SysCommand syscmd = new SysCommand();
	syscmd.exec("/usr/bin/lpget -n " + ns + " " + name);
	exitvalue = syscmd.getExitValue();
	syscmd = null;
	if (exitvalue == 0) {
		return (true);
	}
	return (false);
    }

    public static boolean isLocal(
	String pn) throws Exception
    {
	int exitvalue;

	Debug.message("SVR: DoPrinterUtil.isLocal()");

	SysCommand syscmd = new SysCommand();
	syscmd.exec("/usr/bin/test -d /etc/lp/printers/" + pn);
	exitvalue = syscmd.getExitValue();
	syscmd = null;
	if (exitvalue == 0) {
		//
		// SPECIAL CODE FOR 2.6
		// Configuration file will contain "Remote:"
		// on 2.6.
		syscmd = new SysCommand();
		String cmd_array[] = { "/bin/grep", "^Remote: ",
			"/etc/lp/printers/" + pn + "/configuration" };
		syscmd.exec(cmd_array);
		exitvalue = syscmd.getExitValue();
		syscmd = null;
		if (exitvalue == 0) {
			return (false);
		}
		return (true);
	}
	return (false);
    }
}
