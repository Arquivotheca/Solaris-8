/*
 * @(#)QuickSort.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

/**
 * This class provides an implementation of the quicksort algorithm suitable
 * for use on any array of objects which implement the Comparable interface
 * @see Comparable
 */

public class QuickSort {

    /**
     * Sort the provided array
     * @param the array to sort
     */
    public static void sort(Comparable [] array) {
	sort(array, 0, array.length-1);
    }
    
    private static void sort(Comparable [] array, int low, int high) {
	int scanUp, scanDown, mid;
	Comparable pivot, tmp;
	
	if ((high-low) <= 0) {
	    return;
	} else {
	    if ((high-low) == 1) {
		if (array[high].compareTo(array[low]) < 0) {
		    tmp = array[low];
		    array[low] = array[high];
		    array[high] = tmp;
		}
		return;
	    }
	}
	
	mid = (low + high) / 2;
	pivot = array[mid];
	tmp = array[mid];
	array[mid] = array[low];
	array[low] = tmp;
	scanUp = low + 1;
	scanDown = high;
	
	do {
	    while ((scanUp <= scanDown)
		    && (array[scanUp].compareTo(pivot) <= 0)) {
		++scanUp;
	    }
	    while (pivot.compareTo(array[scanDown]) < 0) {
		--scanDown;
	    }
	    if (scanUp < scanDown) {
		tmp = array[scanUp];
		array[scanUp] = array[scanDown];
		array[scanDown] = tmp;
	    }
	} while (scanUp < scanDown);
	
	array[low] = array[scanDown];
	array[scanDown] = pivot;
	
	if (low < (scanDown-1)) {
	    sort(array, low, scanDown-1);
	}
	if ((scanDown+1) < high) {
	    sort(array, scanDown+1, high);
	}
    }
}
