/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/


#define _GNU_SOURCE
#include <complex.h>
#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We define scale's based on a progression of notes. These define
 * the "allowed" note for any given key.
 */
int major_scale_progression[] = {2,2,1,2,2,2,1, -1};
int harmonic_minor_progression[]=  {2,1,2,2,1,2,2, -1};


/*
 * Build  "scale", a set of allowed notes from a given progression.
 * basename is just used to render the name of the scale.
 */
SCALE * build_scales(int * progression, char * basename) {
	int i;
	SCALE * s;
	s = (SCALE*)malloc(sizeof(SCALE) *12 );

	/* Loop over all starting notes */
	for (i = 0; i < 12; i++) {
		int index;
		int j;
		SCALE * current;
		current = s+i;
		memset(current, 0, sizeof(SCALE));
		
		asprintf(&current->name, "%s %s", basename, note_names[i]);

		index = i;
		/* Loop over the progression */
		for (j = 0; progression[j]>0; j++) {
			current->legal_notes[index] = 1;
			index = (index + progression[j]) % 12;
		}
	}
	return s;
}

void * append(void * a, int ai, void *b, int bi) {
	void * res;
	res = malloc(ai + bi);
	memcpy(res, a, ai);
	memcpy(res + ai, b, bi);
	return res;
}

void  build_all_scales(SCALE ** out, int * n) {
	SCALE  *major, *all;
	// SCALE * minor
	int i;

	fprintf(stderr, "START BUILD SCALE\n");
	//minor = build_scales(harmonic_minor_progression, "minor");
	major = build_scales(major_scale_progression, "major");

	//all = append(minor, 12*sizeof(SCALE), major, 12 * sizeof(SCALE));
	*n = 12;
	*out = major;
	all = major;

	for (i = 0; i < 12; i++) {
		int j;
		fprintf(stderr, "%s", all[i].name);
		for (j = 0; j < 12; j++) {
			fprintf(stderr, "%i", all[i].legal_notes[j]);
		}
		fprintf(stderr, "\n");
	}
		
}


/* Compute  "score" for a particular set of notes to be in a
 * particular key.  This works by incrementing to the score for
 * every on-key note and penalizing it for every off-note key
 */
int test_scale(int * note_frequencies, SCALE * s) {

	int score = 0;

	int i;
	for (i = 0; i < 12; i++) {
		if (s->legal_notes[i]) {
			score += note_frequencies[i];
		} else {
			score -= note_frequencies[i];
		}
	}

	return score;
}

/*
 * Try all scales in s and return the best fit.
 */
SCALE * guess_scale(SCALE * s, int num_scales, int * note_frequencies) {
	int i;
	SCALE * best = NULL;
	int best_score = -10000000;

	for (i = 0; i < num_scales; i++) {
		int try;
		try = test_scale(note_frequencies, s+i);
		if (try > best_score) {
			best_score = try;
			best = s+i;
		}
	}

	if (best!= NULL) {
		fprintf(stderr, "SCALE: %s (%i)\n", best->name, best_score);
	}

	return best;
}





