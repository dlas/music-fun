/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/


#define _GNU_SOURCE
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "shared.h"


/* What is the input sample rate */
#define SAMPLE_RATE 44100
/* How to quantize one second */
#define TIME 10
/* How many samples will we work with at once */
#define CHUNK_SIZE (SAMPLE_RATE/TIME)

/*
 * How many octaves will we consider? 
 */
#define OCTAVES 5

/*
 * What is the minimun energy to care about a note 
 */
#define MINIMUM_ENERGY 1e9

#define LEN(x) (sizeof(x)/sizeof(x[0]))

/*
 * THEORY OF OPERATION
 *
 * Consutrct one "filter" for every note that we want to recognize. 
 * We compute the running projection of each new sample into a sine and
 * cosine wave of each note. (This is essentially like taking a fourier
 * transofrm but we're only interested in certain notes.)
 *
 * We keep a running "total" of the sin and cos part but we decimate
 * decrease it (by a small factor) each sample to keep things current.
 *
 * We can then pick out which notes are present by finding the notes
 * with the highest relative energy defines by the sum of the squares
 * of the sine and cosine parts.
 */
typedef struct FILTER{
	/* What is the wavelength of this note in samples */
	int length;
	/* What sample of the wavelength are we on */
	int index;
	/* What factor do we decimate by on each sample */
	double rolloff;
	/* normalizer == 1/length. This adjusts for the fact that
	 * we don't count all frequencies fairly. (lower frequencies are
	 * "longer" and accumulate more energy... Is this right?)
	 */
	double normalizer;

	/* What do we call this note */
	char * name;

	/* Projected amplitue onto the sine and cosine waves */
	double xsin;
	double xcos;

	double accumulator;
	double max;
	
} FILTER;


/* Get the total (sine + cosine) energy in a particular filter.
 * This gets the average energy in the filter since the last time
 * filter_energy was called (which should be exactly once per
 * CHUNK_SIZE samples).
 */
double filter_energy_average(FILTER * f) {
	double e = f->accumulator;
	f->accumulator = 0;
	return e * f->normalizer / CHUNK_SIZE;
}

double filter_energy_max(FILTER * f) {
	double e = f->max;
	f->max = 0;
	return e * f->normalizer;
}

/* Update a filter with a new sample */
void filter_touch(FILTER * f, double delta) {
	double e;
	/* What is the current phase angle?*/
	double rad = 2*M_PI * (double)f->index / (double)f->length;
	/* Update sine and cosine parts */
	f->xsin = f->xsin * f->rolloff + delta *  sin(rad);
	f->xcos = f->xcos * f->rolloff + delta *  cos(rad);
	/* Update the index */
	f->index = (f->index+1) % f->length;

	/* Update accumulator which we will use to keep track of the
	 * average energy over a sampl.
	 */

	e = (f->xsin*f->xsin) + (f->xcos*f->xcos);

	if (e> f->max) {
		f->max= e;
	}
	f->accumulator += (f->xsin*f->xsin) + (f->xcos*f->xcos);
}


/*
 * Try to be clever about picking the rolloff. The idea is that for
 * each note we'll pick a decay factor that's some big (like 20) multiple
 * of the period in samples. The equation comes from:
 *
 * X_0 = v; x_n = a*x_(n-1).  Let X_n = v/2.
 * v/2 = a*x_n-1 --> v/2 = a^n*v --> a^n = 1/2. 
 * Therefore, a = exp(ln(1/2)/n).
 * When we do, this we also need to introduce a normalization factor of
 * (1-a). 
 */
double halflife_to_rolloff(double halflife) {
	return exp(-.6931/halflife);
}

/*
 * What normalization factor do we need to apply during filter_energy
 * to compensate for the decay factor above?
 *
 * This comes from:  x_n == a(x_(n-1)) +v.
 * But in the steady state, we have x_n==x_(n-1) so
 * x_n = a*(x_n) + v --> x_n = v/(1-a). 
 *
 * The factor of 1000 is essentially arbitrary but is calibrated against
 * some cutoffs in other code and generally makes the normalizing factors
 * close to 1.0
 */
double normalizer_from_rolloff(double rolloff) {
	return 1000 * (1 - rolloff);
}

/*
 * Setup a bunch of filters from a table of notes.
 */
FILTER * make_filters(double * note_table,
	char ** names,
	int notes,
	int octaves,
	double sample_freq) {

	int space = notes * octaves;
	int n,o;

	/* Current octave multiplyer */
	double mult = 1.0;
	FILTER * fs;
	fs = (FILTER*)malloc(sizeof(*fs) * space);
	memset(fs, 0, sizeof(*fs) * space);

	/* Loop over each note of each octave */
	for (o = 0; o < octaves; o++) {
		for (n = 0; n < notes; n++) {
			/* Built filter structure for this note */
			int len;
			double freq;
			FILTER * cur;
			/* What is the frequency of this note? */
			freq = mult * note_table[n];
			/* What is the "wavelength" measure in samples */
			len = sample_freq / freq;
			/* What is the fs object that we're setting up */
			cur = fs+(n + notes *o);
			cur->length = len;
			cur->index = 0;
			cur-> rolloff = halflife_to_rolloff(len*16);
		//	cur-> rolloff = halflife_to_rolloff(8 * CHUNK_SIZE);
			cur->normalizer = normalizer_from_rolloff(cur->rolloff);
			asprintf(&cur->name, "%i%s", o, names[n]);
	//		fprintf(stderr, "%s: %f %f %f %i (%i:%i)\n",
	//			cur->name, freq, cur->rolloff, cur->normalizer, len, o, n);
		}
		/* Don't forget to udpate the octave! */
		mult*=2.0;
	}

	return fs;
}

/*
 * Update an entire filter bank with a new sample */
void  update_filters(FILTER * fs, int n, short sample) {
	int i;
	double s = sample;
	for (i = 0; i < n; i++) {
		filter_touch(fs+i, s);
	}
}

/* Guess which note (ignoring octave) are present 
 * |fs| a filter bank
 * |energy| a pre-allocated array of octaves*notes doubles
 * |notes_present| a pre-allocated array of ints. notes that are present will be set to 1
 * */
double filter_guess_notes(FILTER * fs,
	double * energy,
	int notes,
	int octaves,
	int *notes_present,
	double sample_energy) {

	int i;
	double total_energy = 0;
	int n = notes * octaves;
	int note, octave;

	/* Clear notes */
	memset(notes_present, 0, sizeof(*notes_present) * notes);

	/* Pre-compute the energy for each note and accumulate the total energy.
	 * XXX: Is this right? Do we want to use the combined energy of all
	 * filters or the current sample energy???
	 */
	for (i = 0; i < n; i++) {
		double e = filter_energy_max(fs+i);
		//total_energy += filter_energy_average(fs+i);
		total_energy +=e;
		energy[i] = e;
	}

	total_energy *=1;
	/* Iterate over all notes.  Mark notes that have enough energy. */
	for (octave = 0; octave < octaves; octave ++) {
		for (note = 0; note < notes; note++) {
			double e;
			e = energy[note + notes * octave];
			/* Count something as a note if it has at last
			 * 1/10 of the energy as well as a minimum energy
			 */
			if (e > total_energy / 10 && e > MINIMUM_ENERGY) {
				notes_present[note] = 1;
			}
		}
	}
	return total_energy;
}

/*
 * Dump notes to STDOUT */
void dump_notes(int * notes_present) {
	int i;
	for (i = 0; i < 12; i++) {
		if (notes_present[i]) {
			printf("%s ", note_names[i]);
		} else {
			printf("   ");
		}
	}
	printf("\n");
}

/*
 * Dump energies to STDOUT in a pleasant way */
void dump_energies(double * energy, double total_energy, int notes, int octaves) {
	int i, octave, note;
	printf("\ec\n");
	for (i = 0; i < notes; i++) {
		printf("\t%s", note_names[i]);
	}
	printf("\n");
	for (octave = 0; octave < octaves; octave++) {
		printf("\e[37m%i:\t", octave);
		for (note = 0; note < notes; note ++) {
			double e;
			double le;
			int color;
			e = energy[note + notes * octave];
			color = 30;
			if (e > total_energy/100) {
				color = 32;
			} 
			if (e > total_energy/10) {
				color = 33;
			}
			if (e > total_energy/2) {
				color = 31;
			}

			le = log10(e);
			printf("\e[%im%.1f\t", color, le);
		}
		printf("\n");
	}


	printf("\e[37mFILTER ENERGY: %f\t %f\n",  total_energy, log10(total_energy));
}


/*
 * Process a bunch of samples.  There is a high system-call overhead to
 * getting samples so we don't retrieve samples one at a time. Instead, 
 * loop2, will retrieve them at the display-update rate. process_chunk will
 * loop through every sample ina chunk and update its filter bank. 
 */
double process_chunk(short * input, FILTER * fs, int n_filter) {
	int i;
	double sample_energy = 0;
	for (i = 0; i < CHUNK_SIZE; i++) {
		short s;
		s = input[i*2];
		update_filters(fs, n_filter, s);
		sample_energy += s*s;
	}
	return sample_energy;
}

void dump_accumulator(int * in, int n) {
	int i;
	for (i = 0; i < n; i++) {
		printf("\t%i", in[i]);
	}
	printf("\n");

}

void update_accumulator(int * in, int * out, int n) {
	int i;
	for (i = 0; i < n; i++) {
		out[i] += in[i];
	}
}


void loop2(FILTER * fs, SCALE * scales, int scale_n, int enable_display, int max) {

	short tmpdata[CHUNK_SIZE*2*2];
	double energy[LEN(note_table) * OCTAVES];
	double se;
	double fe;
	int notes_present[LEN(note_table)];
	int notes_accumulator[LEN(note_table)];
	int count = 0;
	memset(notes_accumulator, 0, sizeof(notes_accumulator));
	SCALE * scale = NULL;

	while(1) {
		/* Grab a chunks worth of data */
		if ( get_data_chunk(tmpdata, CHUNK_SIZE) < 0) {
			break;
		}
		/* Update the filter bank */
		se = process_chunk(tmpdata, fs, OCTAVES * LEN(note_table));
		/* Extract notes */
		fe = filter_guess_notes(fs,
				energy,
				LEN(note_table),
				OCTAVES,
				notes_present, 
				se);

		update_accumulator(notes_present, notes_accumulator, LEN(note_table));
		if ( max > 0 && count > max) {
			break;
		}

		/* Periodically (once we've generated enough note counts)
		 * try to guess the scale.
		 */
		if (count > 1000) {
			scale = guess_scale(scales, scale_n, notes_accumulator);
			memset(notes_accumulator, 0, sizeof(notes_accumulator));
			count = 0;
			if (scale) printf("SCALE: %s\n", scale->name);
		}
		count++;

		if (enable_display) {
			/* Update the display */
			dump_energies(energy, fe, 12, OCTAVES);
			dump_accumulator(notes_accumulator, LEN(note_table));
			//printf("%i\n", count);
			if (scale) printf("SCALE: %s\t", scale->name);
			dump_notes(notes_present);
		}
	}
	printf("DONEDONE! %i\n", count);
	scale = guess_scale(scales, scale_n, notes_accumulator);
	if (scale) printf("SCALE: %s\n", scale->name);
}

void parse_args(int  argc, char ** argv, int * enable_display, int * max) {
	int i;
	*enable_display = 1;
	*max = -1;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-nodisplay")) {
			*enable_display = 0;
		} else if (!strcmp(argv[i], "-max")) {
			*max = atoi(argv[++i]);
		} else {
			fprintf(stderr, "BAD ARGS\n");
			exit(1);
		}
	}
}


int main(int argc, char ** argv) {

	FILTER * fs;
	SCALE * scale;
	int scale_n;
	int enable_display;
	int max;
	parse_args(argc, argv, &enable_display, &max);
	build_all_scales(&scale, &scale_n);
	fs = make_filters(note_table, note_names, LEN(note_table), OCTAVES, SAMPLE_RATE);
	loop2(fs, scale, scale_n, enable_display, max);


	return 0;
}
