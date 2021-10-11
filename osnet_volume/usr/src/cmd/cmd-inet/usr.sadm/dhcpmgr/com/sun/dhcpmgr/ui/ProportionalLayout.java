/*
 * @(#)ProportionalLayout.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.awt.*;
import java.util.Hashtable;

/**
 * This layout manager provides a layout which allocates space either
 * horizontally or vertically to components in proportion to a weight assigned
 * to each component.  All components receive the same dimension along the
 * other axis.
 */
public class ProportionalLayout implements LayoutManager {
    /**
     * Constant for a horizontal proportional layout.
     */
    public static final int HORIZONTAL = 0;
    /**
     * Constant for a vertical proportional layout.
     */
    public static final int VERTICAL = 1;
    private int direction;
    private Hashtable components;
    private int totalWeight = 0;
    
    /**
     * Construct a horizontal version of this layout.
     */
    public ProportionalLayout() {
	this(HORIZONTAL);
    }
    
    /**
     * Construct a proportional layout in the direction specified.
     * @param direction Must be either HORIZONTAL or VERTICAL
     */
    public ProportionalLayout(int direction) {
	this.direction = direction;
	components = new Hashtable();
    }
    
    public void addLayoutComponent(String name, Component component) {
	Integer weight;
	try {
	    weight = Integer.decode(name);
	} catch (NumberFormatException e) {
	    weight = new Integer(1);
	}
	totalWeight += weight.intValue();
	components.put(component, weight);
    }
    
    public void removeLayoutComponent(Component component) {
	Integer weight = (Integer)components.get(component);
	totalWeight -= weight.intValue();
	components.remove(component);
    }
    
    private Dimension computeLayoutSize(Container target, boolean minimum) {
	Dimension dim = new Dimension(0, 0);
	for (int i = 0; i < target.getComponentCount(); ++i) {
	    Component c = target.getComponent(i);
	    if (c.isVisible()) {
		Dimension d;
		if (minimum) {
		    d = c.getMinimumSize();
		} else {
		    d = c.getPreferredSize();
		}
		if (direction == HORIZONTAL) {
		    dim.height = Math.max(dim.height, d.height);
		    dim.width += d.width;
		} else {
		    dim.height += d.height;
		    dim.width = Math.max(dim.width, d.width);
		}
	    }
	}
	Insets insets = target.getInsets();
	dim.width += insets.left + insets.right;
	dim.height += insets.top + insets.bottom;
	return dim;
    }
    
    public Dimension preferredLayoutSize(Container target) {
	return computeLayoutSize(target, false);
    }
    
    public Dimension minimumLayoutSize(Container target) {
	return computeLayoutSize(target, true);
    }
    
    public void layoutContainer(Container target) {
	Insets insets = target.getInsets();
	Dimension dim = target.getSize();
	int x = insets.left;
	int y = insets.top;
	int totalHeight = dim.height - insets.bottom;
	int totalWidth = dim.width - insets.right;
	
	for (int i = 0; i < target.getComponentCount(); ++i) {
	    Component c = target.getComponent(i);
	    if (c.isVisible()) {
		 if (direction == HORIZONTAL) {
		    float fw = (float)totalWidth
			* (float)((Integer)components.get(c)).intValue()
			/ (float)totalWeight;
		    c.setBounds(x, y, (int)fw, totalHeight);
		    x += (int)fw;
		} else {
		    float fh = (float)totalHeight
			* (float)((Integer)components.get(c)).intValue()
			/ (float)totalWeight;
		    c.setBounds(x, y, totalWidth, (int)fh);
		    y += (int)fh;
		}
	    }
	}
    }
}
