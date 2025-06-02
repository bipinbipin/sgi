
/*
 * Algorithm from "A New Approach to Text Searching"
 * Communications of the ACM
 * October 1992 Volume 35,  Number 10
 * pp 74-82
 * 
 * Fast text matching for patterns with classes like:
 * [nvtc].[234].[difs].
 * would match n3f, v3s, t4d ...
 * 
 * Gives lots of false hits in the toogl application due to large number
 * of characters in classes:
 * "tory" matches a pattern from "poly", "circ", "tpon":
 * [tpc].[oip].[lro].[ycn].
 * but still gives a factor of 10 improvement over not doing this.
 * 
 */
#include <stdlib.h>
#include <iostream.h>

#include "search.h"

Search::Search() 
{
    int i;
    for( i = 0; i < sizeof(table)/sizeof(table[0]); table[i++] = ~0);
    cstate = 0;
    lim = 0;
}

void Search::add(const char *s) 
{
    int i;
    for( i = 0; i < MAXPATTERN && s[i]; i++) {
	int c = (s[i])&(NALPH-1);
	table[c] &= ~(1 << i);
	lim |= (1 << i);
    }
}

void Search::print_table()
{
    int i, j;
    for(i =' '; i < NALPH; i++) {
	cerr << (char)i << " ";
	for(j = 0; j < MAXPATTERN; j++) {
	    cerr << (char)((table[i] & (1 << j)) ? '1' : '0');
	}
	cerr << "\n";
    }
	    
}

int Search::check(const char *s) 
{
    int i, c;
    unsigned int initial = ~0,  state;
    unsigned llim = ~(lim >> 1);
    
    state = initial;
    for( i = 0; s[i]; i++) {
	c = s[i] & (NALPH-1);
	state = (state << 1) | table[c];
	if (state < llim) {
	    return 1;
	}
    }
    return 0;    
}
