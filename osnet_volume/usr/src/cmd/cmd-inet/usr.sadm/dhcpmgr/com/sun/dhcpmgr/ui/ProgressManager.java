/*
 * @(#)ProgressManager.java	1.1	99/05/26 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.ProgressMonitor;
import javax.swing.SwingUtilities;
import java.awt.Component;

/**
 * Provides a framework for doing long-running operations and keeping the
 * user apprised of the results.  Also provides an exception if Cancel is
 * pressed in the progress dialog.
 */
public class ProgressManager {
    private ProgressMonitor monitor;
    private int count;
    private String message;
    private Runnable progressUpdater;
    
    /**
     * Create a new ProgressManager; see ProgressMonitor for description
     * of the parameters here.
     * @see javax.swing.ProgressMonitor
     */
    public ProgressManager(Component comp, Object msg, String note, int min,
            int max) {
	monitor = new ProgressMonitor(comp, msg, note, min, max);
	// Create background object to update monitor
        progressUpdater = new Runnable() {
            public void run() {
	        monitor.setProgress(count);
	        monitor.setNote(message);
	    }
        };
    }

    /**
     * Update the progress display.  Throws InterruptedException if user
     * has pressed the Cancel button on the progress dialog
     * @param progress the amount of the task that has been completed
     * @param msg the message to be displayed at this time
     * @throws java.lang.InterruptedException
     */
    public void update(int progress, String msg) throws InterruptedException {
        count = progress;
	message = msg;
    	SwingUtilities.invokeLater(progressUpdater);
	if (monitor.isCanceled()) {
	    throw new InterruptedException();
	}
    }
}
