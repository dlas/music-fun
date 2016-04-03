
#define _GNU_SOURCE
#include <complex.h>
#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int major_scale_progression[] = {2,2,1,2,2,2,1, -1};
int harmonic_minor_progression[]=  {2,1,2,2,1,2,2, -1};


SCALE * build_scales(int * progression, char * basename) {
	int i;
	SCALE * s;
	s = (SCALE*)malloc(sizeof(SCALE) *12 );

	for (i = 0; i < 12; i++) {
		int index;
		int j;
		SCALE * current;
		current = s+i;
		memset(current, 0, sizeof(SCALE));
		
		asprintf(&current->name, "%s %s", basename, note_names[i]);

		index = i;
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
	SCALE * minor, *major, *all;
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


int test_scale(int * note_frequencies, SCALE * s) {

	int score = 0;

	int i;
	for (i = 0; i < 12; i++) {
		if (s->legal_notes[i]) {
			score += note_frequencies[i];
		} else {
			score -= note_frequencies[i] * 2;
		}
	}

	return score;
}

SCALE * guess_scale(SCALE * s, int num_scales, int * note_frequencies) {
	int i;
	SCALE * best = NULL;
	int best_score = 0;

	for (i = 0; i < num_scales; i++) {
		int try;
		try = test_scale(note_frequencies, s+i);
		if (try > best_score) {
			best_score = try;
			best = s+i;
		}
	}

	if (best!= NULL) {
		fprintf(stderr, "SCALE: %s (%i)", best->name, best_score);
	}

	return best;
}





