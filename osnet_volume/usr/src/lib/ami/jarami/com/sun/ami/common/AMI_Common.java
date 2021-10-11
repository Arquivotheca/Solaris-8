/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Common.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

import java.security.*;
import java.lang.System;

import com.sun.ami.AMI_Exception;

/**
 * This class load libami.so.1 for ami.jar
 */

public class AMI_Common extends Object {

	private static String loadLibrary = "ami";
	private static boolean libraryLoaded = false;

	public AMI_Common() {
		if (libraryLoaded)
			return;
		synchronized (loadLibrary) {
			if (libraryLoaded)
				return;
			try {
				// Will look in LD_LIBRARY_PATH
				System.loadLibrary(loadLibrary);
			} catch (UnsatisfiedLinkError e) {
				try {
					System.load("/usr/lib/lib" +
					    loadLibrary + ".so.1");
				} catch (UnsatisfiedLinkError f) {
					// throw (new AMI_Exception(
					// "Unable to load libami library"));
				}
			}
			libraryLoaded = true;
		}
	}
}

