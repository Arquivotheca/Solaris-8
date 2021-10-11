/*
 * @(#)FieldLayout.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.awt.*;
import java.util.*;

/**
 * <CODE>FieldLayout</CODE> treats components as a list of
 * labeled fields, where each label is placed on the left
 * edge of the container with its associated field to its
 * right.<P>
 *
 * Two kinds of components may be added: "Label" and "Field."
 * Labels and Fields must be added in pairs, because there is
 * a one-to-one correspondence between them.<P>
 *
 * When a <CODE>Field</CODE> is added, it is associated with the
 * last <CODE>Label</CODE> added.<P>
 */
public class FieldLayout implements LayoutManager {
    public static final String LABEL = "Label";
    public static final String LABELTOP = "LabelTop";
    public static final String FIELD = "Field";
    
    class Row {
	Component label;
	Component field;
	boolean center;
	
	public Row() {
	    label = null;
	    field = null;
	    center = true;
	}
    }
    
    Vector rows;
    int hgap;
    int vgap;

    /**
     * Constructs a new <CODE>FieldLayout</CODE> with a centered alignment.
     */
    public FieldLayout() {
	this(5, 5);
    }

    /**
     * Constructs a new <CODE>FieldLayout</CODE> with the specified gap values.
     * @param <VAR>hgap</VAR> The horizontal gap variable.
     * @param <VAR>vgap</VAR> The vertical gap variable.
     */
    public FieldLayout(int hgap, int vgap) {
	this.hgap = hgap;
	this.vgap = vgap;
	rows = new Vector();
    }

    /**
     * Adds the specified component to the layout.
     * @param <VAR>name</VAR> The name of the component.
     * @param <VAR>comp</VAR> The component to be added.
     */
    public void addLayoutComponent(String name, Component comp) {
	if (LABEL.equals(name)) {
	    Row r = new Row();
	    r.label = comp;
	    r.center = true;
	    rows.addElement(r);
	} else if (LABELTOP.equals(name)) {
	    Row r = new Row();
	    r.label = comp;
	    r.center = false;
	    rows.addElement(r);
	} else if (FIELD.equals(name)) {
	    ((Row)rows.lastElement()).field = comp;
	}
    }

    /**
     * Removes the specified component from the layout.
     * @param <VAR>comp</VAR> The component to remove.
     */
    public void removeLayoutComponent(Component comp) {
	Enumeration en = rows.elements();
	while (en.hasMoreElements()) {
	    Row r = (Row)en.nextElement();
	    if (comp == r.label || comp == r.field) {
		rows.removeElement(r);
		return;
	    }
	}
    }

    /**
     * Returns the preferred dimensions for this layout given the components
     * in the specified target container.
     * @param <VAR>target</VAR> The component that needs to be laid out.
     * @see java.awt.Container
     * @see #minimumLayoutSize
     */
    public Dimension preferredLayoutSize(Container target) {
	Dimension dim = new Dimension(0, 0);
	int widestLabel = 0, widestField = 0;

	Enumeration en = rows.elements();
	while (en.hasMoreElements()) {
	    Row r = (Row)en.nextElement();
	    Dimension ld = new Dimension(0, 0);
	    Dimension fd = new Dimension(0, 0);
	    if (r.label.isVisible()) {
		ld = r.label.getPreferredSize();
		widestLabel = Math.max(widestLabel, ld.width);
	    }
	    if (r.field.isVisible()) {
		fd = r.field.getPreferredSize();
		widestField = Math.max(widestField, fd.width);
	    }
	    dim.height += Math.max(ld.height, fd.height) + vgap;
	}
	dim.width = widestLabel + hgap + widestField;
	Insets insets = target.getInsets();
	dim.width  += insets.left + insets.right + hgap*2;
	dim.height += insets.top + insets.bottom + vgap;
	return dim;
    }

    /**
     * Returns the minimum dimensions needed to layout the components
     * contained in the specified target container.
     * @param <VAR>target</VAR> The component that needs to be laid out.
     * @see #preferredLayoutSize
     */
    public Dimension minimumLayoutSize(Container target) {
	Dimension dim = new Dimension(0, 0);
	int widestLabel = 0, widestField = 0;

	Enumeration en = rows.elements();
	while (en.hasMoreElements()) {
	    Row r = (Row)en.nextElement();
	    Dimension ld = new Dimension(0, 0);
	    Dimension fd = new Dimension(0, 0);
	    if (r.label.isVisible()) {
		ld = r.label.getMinimumSize();
		widestLabel = Math.max(widestLabel, ld.width);
	    }
	    if (r.field.isVisible()) {
		fd = r.field.getMinimumSize();
		widestField = Math.max(widestField, fd.width);
	    }
	    dim.height += Math.max(ld.height, fd.height) + vgap;
	}
	dim.width = widestLabel + hgap + widestField;
	Insets insets = target.getInsets();
	dim.width  += insets.left + insets.right + hgap*2;
	dim.height += insets.top + insets.bottom + vgap;
	return dim;
    }

    /**
     * Performs the layout of the container.  Components are treated
     * either as labels or fields. Labels go on the left (right-aligned),
     * with their associated fields placed immediately to their right.
     * @param <VAR>target</VAR> The specified component being laid out.
     * @see java.awt.Container
     */
    public void layoutContainer(Container target) {
	Insets insets = target.getInsets();
	Dimension dim = target.getSize();
	int x = 0, y = insets.top, offset = 0;
	int widestLabel = 0;

	// find widest label component
	Enumeration en = rows.elements();
	while (en.hasMoreElements()) {
	    Row r = (Row)en.nextElement();
	    if (r.label.isVisible()) {
		Dimension d = r.label.getPreferredSize();
		widestLabel = Math.max(widestLabel, d.width);
	    }
	}

	// lay out rows, right-aligning labels
	en = rows.elements();
	while (en.hasMoreElements()) {
	    Row r = (Row)en.nextElement();
	    Dimension ld = new Dimension(0, 0);
	    Dimension fd = new Dimension(0, 0);
	    Component l = r.label;
	    Component f = r.field;

	    if (l.isVisible()) {
		ld = l.getPreferredSize();
		x = insets.left;
		/*
		 * If the field is visible, move it right to line up with
		 * the widest line.
		 */
		if (f.isVisible())
		    x += Math.max(widestLabel - ld.width, 0);
		offset = 0;
		if (f.isVisible() && r.center) {
		    fd = f.getPreferredSize();
		    // center label on field
		    offset = Math.max(0, (fd.height-ld.height)/2);
		}
		l.setBounds(x, y+offset, ld.width, ld.height);
	    }
	    if (f.isVisible()) {
		fd = f.getPreferredSize();
		x = insets.left + widestLabel + hgap;
		int w = dim.width-x-hgap;
		f.setBounds(x, y, w, fd.height);
	    }
	    y += Math.max(ld.height, fd.height) + vgap;
	}

    }

    /**
     * Returns the <CODE>String</CODE> representation of this
     * <CODE>FieldLayout</CODE>'s values.
     */
    public String toString() {
	return getClass().getName() + "[hgap=" + hgap + ",vgap=" + vgap + "]";
    }
}
