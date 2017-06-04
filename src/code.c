/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/


#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "notes.h"

/* What is the input sample rate */
#define SAMPLE_RATE 44100
/* How to quantize one second */
#define TIME 10
/* How many samples will we work with at once */
#define CHUNK_SIZE (SAMPLE_RATE/TIME)


#define LEN(x) (sizeof(x)/sizeof(x[0]))

#define DOWNSAMPLE_RATIO 1

#define DCHUNK_SIZE (SAMPLE_RATE/TIME/DOWNSAMPLE_RATIO)

#define CUTOFF 2000

#define MAX_SAMPLES 100

/*
 * Utility struct that holds the state of the world while we process
 * things.
 */
typedef struct {
	/* FFTW plan and input and output array */
	fftw_plan fftwplan;
	fftw_complex  * fftw_in;
	fftw_complex  * fftw_out;

	/* This is the array of audio samples. We expect samples
 	 * Stereo, signed, 16 bit little endian PCM format */
	short tmpdata[CHUNK_SIZE * 2];

	/* A processed array with the total energy at each frequency */
	double energy[CHUNK_SIZE];

	double downsampled[DCHUNK_SIZE];

	/* Input file descriptor */
	int fd;

	int history[MAX_SAMPLES];
} STATE;

void redownsample(STATE * s) {

	int i;
	for (i = 0; i < DCHUNK_SIZE; i++) {
		s->downsampled[i] = 0;
	}

	for (i = 0; i < CHUNK_SIZE; i++) {
		int di;
		di = i/DOWNSAMPLE_RATIO;
		s->downsampled[di] += s->energy[i];
	}
}

int pickwinner(STATE * s) {
	double max = 0;
	int maxi = 0;
	int i;
	for (i = 6; i < CUTOFF; i++) {
		if (s->downsampled[i] > max) {
			maxi = i;
			max = s->downsampled[i];
		}
	}
	return maxi;
}



/*
 * Allocate arrays for fftw and setup a fft plan 
 */
void setup_fftw(STATE * s) {
	s->fftw_in= (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * CHUNK_SIZE);
	s->fftw_out= (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * CHUNK_SIZE);

	s->fftwplan = fftw_plan_dft_1d(CHUNK_SIZE,
			s->fftw_in,
			s->fftw_out,
			FFTW_FORWARD,FFTW_MEASURE);
}



void error(char * s) {
	fprintf(stderr, "UHOH: %s\n", s);
	abort();
}


/*
 * Read raw data into memory. Put it in s->tmpdata
 */
int get_data_chunk(STATE * s) {
	int left_to_read = CHUNK_SIZE * 2 * 1;
	int pos = 0;

	while (left_to_read > 0) {
		int len;
		len = read(s->fd, ((char*)(s->tmpdata))  + pos, left_to_read);
		if (len <0) {
			error(strerror(errno));
		}
		pos+=len;

		left_to_read-=len;
	}
	return 0;
}

/*
 * This does the FFT, computing s->energy froms->tmpdata
 */
int do_fft(STATE * s) {

	int i;
	/* Input is in stereo so we only care about every other sample */
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->fftw_in[i] = s->tmpdata[i*1];
	}

	/* FFT */
	fftw_execute(s->fftwplan);

	/* Now compute the energy in each frequency*/
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->energy[i] = (s->fftw_out[i] * conj(s->fftw_out[i]));
		
	}

	return 0;
}

void add_history(int * history, int n, int new) {
	int i;

	for (i = 0; i < n-1; i++) {
		history[i] = history[i+1];
	}

	history[n-1] = new;
}


double music[] = {N_E5, N_D5s, N_E5, N_D5s, N_E5, N_B4, N_D5, N_C5, N_A4};//, N_C4, N_E4, N_A4, N_B4};

//double music[] = {N_E5, N_E5, N_E5, N_E5, N_E5};

void calc_score2(int * history, int history_n, double * score_out, double * time_shift_out, double * freq_shift_out) {
	int t_max = LEN(music);
	double time_shift = (double)(history_n-1) / (double)t_max;
	double top = 0, bottom = 0;
	double freq_shift;
	double M = 0;

	int i;

	/* See notebook page 17, 19 */
	for (i = 0; i < t_max; i++) {
		int first = floor((double)i * time_shift);
		int last = floor((double)(i+1) * time_shift);
		int x;
		assert(first >= 0);
		assert(last < history_n);
		for (x = first; x < last; x++) {
			double factor = 1;
			double h,m;
			//h = log(history[x]);
			//m = log(music[i]);
			h = history[x];
			m = music[i];
			
			if (x == first) {
				factor = (double)x*time_shift- floor((double)x*time_shift);
			} else if (x == last) {
				factor = ceil((double)x*time_shift) - (double)x*time_shift;
			}
			if (factor == 0) { factor = 1.0;}

			

			assert(factor >=0.0);
			assert(factor <=1.0);
			factor /=time_shift;

			//top += factor * music[i] * (double)history[x];
			//bottom += factor * (double)(history[x]*history[x]);
			top += factor * h*m;
			bottom += factor*h*h;
			M+=factor*m*m;
		}
	}

	if (bottom == 0) {
		printf("NO DATA (%i)\n", history_n);
		*score_out = 10000000;
		*time_shift_out = 0;
		*freq_shift_out = 0;
	} else {
		
		double score;
		freq_shift = top/bottom;
		score = freq_shift*freq_shift*bottom - 2.0*freq_shift*top + M;
		*score_out = score;
		*freq_shift_out = freq_shift;
		*time_shift_out = time_shift;
	}
}

void calc_score(int * history, int history_n, double * score_out, double * time_shift_out, double * freq_shift_out) {
	int t_max = LEN(music);
	double time_shift = (double)(history_n) / (double)t_max;
	double freq_shift = 0;

	double i_h=0, i_hh=0,i_m=0,i_mm=0,i_hm=0;
	double score = 0;
	int i;
	int x;

	for (i = 0; i < t_max; i++) {
		double m;
		m = log(music[i]);
		i_m+=m;
		i_mm+=m*m;
	}

	for (i = 0; i < history_n; i++) {
		double h;
		h = log(history[i]);
		i_h+=h;
		i_hh+=h*h;
	}

	freq_shift = i_m/(double)(t_max) - i_h/(double)history_n;

	for (x = 0; x < history_n; x++) {
		int il, ih;
		double distl, disth;
		il = (double)x / time_shift;
		ih = (double)(x+1)/time_shift;
		distl = (freq_shift + log(history[x]) - log(music[il]));
		distl*=distl;

		disth = freq_shift + log(history[x]) - log(music[ih]);
		disth*=disth;

		if (disth > distl || ih >= t_max) {
			score+=distl;
		} else {
			score+=disth;
		}

	//	printf("%i %i %i %f %f %f\n", x, il, ih, distl, disth, score);
	}

	score/=(double)history_n;

	*score_out = score;
	*freq_shift_out = freq_shift;
	*time_shift_out = time_shift;
	//printf("FS: %f %f %f %f %f %i %i %f\n", freq_shift, i_m, i_h, i_m/(double)(history_n), i_h/(double)t_max, history_n, t_max, score);
}

int check_history(int * history, int history_n) {
	/* See notebook #2, page 17 for derivation */

	double score = 0, time_shift = 0, freq_shift = 0;
	int i;
	double min_score = 1000000;
	double best_freq_shift, best_time_shift;
	int best_i;

	
	for (i = history_n - 40; i < history_n; i++) {
		printf("%i ", history[i]/10);
	}
	printf("\n");

	for (i = 9; i < 41; i++) {

		calc_score(history + history_n - i, i, &score, &time_shift, &freq_shift);
		//printf("CANDIDATE: %f %f %f\n", score, time_shift,freq_shift);
		if (score < min_score) {
			min_score = score;
			best_freq_shift = freq_shift;
			best_time_shift = time_shift;
			best_i = i;
		}
	}

	if (min_score < 0.005) {
		printf("************* BEST: %g %f %f %i***************\n\n\n", (min_score), best_freq_shift, best_time_shift, best_i);
	}
	return 0;
}
void loop(STATE * s) {


	int w;
	
	while (1) {

		get_data_chunk(s);
		do_fft(s);
		redownsample(s);
		w = pickwinner(s);
		add_history(s->history, MAX_SAMPLES, w * 10.0);
		check_history(s->history, MAX_SAMPLES);
		printf("%i %i\n", w, w*TIME*DOWNSAMPLE_RATIO);

	}
}

int main(void) {
	STATE s;

	setup_fftw(&s);

	s.fd = 0;
	loop(&s);
	return 0;
}

	




