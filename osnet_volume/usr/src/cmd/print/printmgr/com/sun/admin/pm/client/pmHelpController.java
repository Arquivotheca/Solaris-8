/*
 *
 * ident	"@(#)pmHelpController.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmHelpController.java
 * Help subsystem implementation
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.io.*;
import javax.swing.JPanel;
import javax.swing.border.*;
import javax.swing.*;

import com.sun.admin.pm.server.*;

class pmHelpController {

    public pmHelpFrame frame = null;
    
    /*
     * request presentation of the specified help item.
     */
    public void showHelpItem(String tag) {
        Debug.info("HELP: controller.showHelpitem "  + tag);
        if (tag != null) {
            pmHelpItem item = viewPanel.loadItemForTag(tag);
            outerPanel.setSelectedComponent(viewPanel);
        }
    }

    public void showHelpItem(pmHelpItem item) {
        if (item != null)
            showHelpItem(item.tag);
    }

    JTabbedPane outerPanel;
    pmHelpDetailPanel viewPanel;
    pmHelpIndexPanel indexPanel;
    pmHelpSearchPanel searchPanel;
  
    Vector history;

    public JTabbedPane getTopPane() {
        return outerPanel;
    }

    public pmHelpController(pmHelpFrame f) {

        frame = f;
    
        outerPanel = new JTabbedPane();
    
        viewPanel = new pmHelpDetailPanel(this);
        indexPanel = new pmHelpIndexPanel(this);
        searchPanel = new pmHelpSearchPanel(this);
    
        outerPanel.add(pmUtility.getResource("View"), viewPanel);
        outerPanel.add(pmUtility.getResource("Index"), indexPanel);
        outerPanel.add(pmUtility.getResource("Search"), searchPanel);
    
        pmHelpRepository.populateHelpItemDB();
        pmHelpRepository.populateHelpKeywordDB();
        pmHelpRepository.populateHelpTitleDB();

        indexPanel.queryPanel.handleText("");      // prime it... ugly.
    
        history = new Vector();

        frame.setDefaultComponent(outerPanel);
    }  


}




