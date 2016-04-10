/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/


#define _GNU_SOURCE
#include "shared.h"
#include <stdlib.h>
#include <stdio.h>


typedef struct {

	int note_pairs[144];

} CHORDS;

void bump_chords(CHORDS * c, int * notes) {

	int i,j;

	for (i = 0; i < 12; i++) {
		for (j = 0; j < 12; j++) {
			if (notes[i] && notes[j]) {
				c->note_pairs[i + 12*j]++;
			}
		}
		
	}
}

void clear_chords(CHORDS * c) {
	int i;
	for (i = 0; i < 144; i++) {
		c->note_pairs[i] = 0;
	}
}

CHORDS * make_chords(void) {
	CHORDS * c;
	c = (CHORDS*)malloc(sizeof(CHORDS));
	
	clear_chords(c);
	return c;
}

void dump_chords(CHORDS * c) {

	int i,j;

	for (i = 0; i < 12; i++) {
		printf("\t%s", note_names[i]);
	}

	printf("\n");

	for (i = 0; i < 12; i++) {
		printf("%s\t", note_names[i]);
		for (j = 0; j < 12; j++) {
			printf("%i\t", c->note_pairs[i + 12 *j]);
		}
	}
}

