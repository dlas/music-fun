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

	/* Input file descriptor */
	int fd;

	int history[MAX_SAMPLES];
	double * e_history[MAX_SAMPLES];
} STATE;


int pickwinner(double * energies, int low_cutoff, int high_cutoff) {
	double max = 0;
	int maxi = 0;
	int i;
	for (i = low_cutoff; i < high_cutoff; i++) {
		if (energies[i] > max) {
			maxi = i;
			max = energies[i];
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
int do_fft(STATE * s, double * output) {

	int i;
	double total_energy;
	/* Input is in stereo so we only care about every other sample */
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->fftw_in[i] = s->tmpdata[i*1];
	}

	/* FFT */
	fftw_execute(s->fftwplan);

	/* Now compute the energy in each frequency*/
	for (i = 0; i < CHUNK_SIZE; i++) {
		double e;

		e = (s->fftw_out[i] * conj(s->fftw_out[i]));
		if (i > CHUNK_SIZE/2) {
			e = 0;
		}
		output[i] = e;
		if (i > 6) {
			total_energy+=e;
		}
	}

	if (total_energy >0) {
		for (i = 0; i < CHUNK_SIZE; i++) {
			output[i]/=total_energy;
		}
	}

	return 0;
}

double * add_e_history(double ** e_history, int n) {
	int i;
	e_history[n-1] = e_history[0];

	for (i = 0; i < n-1; i++) {
		e_history[i] = e_history[i+1];
	}
	return e_history[n-1];
}
void setup_e_history(double ** e_history, int n) {
	int i;
	for (i = 0; i < n; i++) {
		e_history[i] = calloc(CHUNK_SIZE,  sizeof(double));
	}
}

void add_history(int * history, int n, int new) {
	int i;

	for (i = 0; i < n-1; i++) {
		history[i] = history[i+1];
	}

	history[n-1] = new;
}


double music[] = {N_E5, N_D5s, N_E5, N_D5s, N_E5, N_B4, N_D5, N_C5, N_A4};//,  N_C4, N_E4, N_A4, N_B4};

double energy_in_f(double * history, double freq_shift, int i) {
	int fidx;
	double m;
	//int idx;
	double s = 0;

	if (i >= LEN(music)) {
		return 0;
	}
	m = music[i];
	if (m == 0) {
		return 0;
	}

//	for (fidx = m * freq_shift; fidx < CHUNK_SIZE; fidx*=2) {
//		s+=history[fidx];
//	}
	
	fidx = m * freq_shift;
	s = history[fidx];

	/*
	for (idx = fidx-1; idx<fidx+1; idx++) {

		s += history[idx];
	}
	*/

	if (s > 0.1) {
		return 1;
	} else {
		return 0;
	}

	//printf("%i %f %i %f %i\n", i, m, fidx, s, pickwinner(history));
	return s;
}


/*
void calc_score3(double ** history, int history_n, double * score_out, double *shift_time_out, double *shift_freq_out) {
	int t_max = LEN(music);
	double time_shift = (double)(history_n) / (double)t_max;
	double freq_shift;
	double score = 0;
	double firstf;
	int x;
	firstf = pickwinner(history[0]);
	if (firstf <=25) {
		*score_out = 0;
		return;
	}
	freq_shift = firstf / music[0];

	for (x = 0; x < history_n; x++) {
		double * freqs = history[x];
		double e1, e2;
		int mil, mih;
		mil = (double)x/time_shift;
		mih = (double)(x+1) / time_shift;
		e1 = energy_in_f(freqs, freq_shift, mil);
		e2 = energy_in_f(freqs, freq_shift, mih);
		if (e1 > e2) {
			score +=e1;
		} else {
			score+=e2;
		}
	}
	
	*score_out = score / (double)history_n;
	*shift_time_out = time_shift;
	*shift_freq_out = freq_shift;

}
*/
//double music[] = {N_E5, N_E5, N_E5, N_E5, N_E5};
void calc_score2(int * history, double * energy, int history_n, double * score_out, double * time_shift_out, double * freq_shift_out) {
	int t_max = LEN(music);
	double time_shift = (double)(history_n) / (double)t_max;
	double freq_shift = 0;

	double i_h=0, i_hh=0,i_m=0,i_mm=0,i_hm=0;
	double score = 0;
	int i;
	int x;

	for (i = 0; i < t_max/2; i++) {
		double m;
		m = log(music[i]);
		i_m+=m;
		i_mm+=m*m;
	}

	for (i = 0; i < history_n/2; i++) {
		double h;
		h = log(history[i]);
		i_h+=h;
		i_hh+=h*h;
	}

	freq_shift = (i_m/(double)(t_max/2) - i_h/(double)(history_n/2));

	for (x = 0; x < history_n; x++) {
		int il, ih;
		double distl, disth;
		double touse;
		il = (double)x / time_shift;
		ih = 1+(double)(x)/time_shift;
		distl = pow((freq_shift + log(history[x]) - log(music[il])), 4);

		disth = pow((freq_shift + log(history[x]) - log(music[ih])), 4);

		if (disth > distl || ih >= t_max) {
			touse = distl;
		} else {
			touse = disth;
		}

		touse/=(energy[x]);
		score+=touse;
	}

	score/=(double)history_n;
	score+=0.000000001;

	*score_out = score;
	*freq_shift_out = freq_shift;
	*time_shift_out = time_shift;
	//printf("FS: %f %f %f %f %f %i %i %f\n", freq_shift, i_m, i_h, i_m/(double)(history_n), i_h/(double)t_max, history_n, t_max, score);
}

void calc_range(int * history, int history_n, int *low_out, int*high_out) {
	double i_m = 0, i_h = 0;
	double freq_shift;
	int t_max = LEN(music);

	int i;

	for (i = 0; i < t_max/2; i++) {
		double m;
		m = log(music[i]);
		i_m+=m;
	}

	for (i = 0; i < history_n/2; i++) {
		double h;
		h = log(history[i]);
		i_h+=h;
	}

	freq_shift = (i_m/(double)(t_max/2) - i_h/(double)(history_n/2));

	*low_out = N_A4 / exp(freq_shift);
	*high_out = N_F5 / exp(freq_shift);

	//printf("FR: %f %f %i %i %i\n", freq_shift, exp(freq_shift), *low_out, *high_out, history_n);
}

void do_it(STATE* s, int blen, double *score_out, double *time_shift_out, double * freq_shift_out, int * lr, int * hr) {

	int low, high;
	int new_history[MAX_SAMPLES];
	double new_energy[MAX_SAMPLES];
	int i;
	calc_range(s->history + MAX_SAMPLES - blen, blen, &low, &high);

	for (i = 0; i < blen; i++) {
		int f;
		int idx;
		idx = MAX_SAMPLES-blen+i;
		
		f = pickwinner(s->e_history[idx], low, high);
		new_history[i] = f;
		new_energy[i] = s->e_history[idx][f];
	}

	calc_score2(new_history, new_energy, blen, score_out, time_shift_out, freq_shift_out);
	*lr=low;
	*hr=high;

}


int check_history(STATE * s) {
	/* See notebook #2, page 17 for derivation */

	double score = 0, time_shift = 0, freq_shift = 0;
	int i;
	double min_score = 1000000000000;
	double best_freq_shift, best_time_shift;
	double ls;
	int best_i;
	int low, high;
	int l,h;

	

	for (i = 12; i < 60; i++) {

		//calc_score3(history + history_n - i, i, &score, &time_shift, &freq_shift);
		//calc_score2(s->history + MAX_SAMPLES - i, i, &score, &time_shift, &freq_shift);
		//printf("SSS: %g %f %f\n", score, freq_shift, time_shift);
		//printf("CANDIDATE: %f %f %f\n", score, time_shift,freq_shift);
		do_it(s, i, &score, &time_shift, &freq_shift, &l, &h);
		if (score < min_score) {
			min_score = score;
			best_freq_shift = freq_shift;
			best_time_shift = time_shift;
			best_i = i;
			low=l;
			high=h;
		}
	}
	ls=log10(min_score);
	printf("%f %f %i %i-%i ", ls, best_freq_shift, best_i, low, high);
	for (i = 40; i < MAX_SAMPLES; i++) {
		printf("%02i ", s->history[i]);
	}
	printf("\n");

	if (ls < -5) {
		printf("************* BEST: %g %f %f %i***************\n\n\n", (ls), best_freq_shift, best_time_shift, best_i);
	}
	return 0;
}
void loop(STATE * s) {


	int w;
	double * t;
	
	while (1) {

		get_data_chunk(s);
		t = add_e_history(s->e_history, MAX_SAMPLES);
		do_fft(s, t);
		w=pickwinner(t, 3, CUTOFF);
		add_history(s->history, MAX_SAMPLES, w);
		check_history(s);

	}
}

int main(void) {
	STATE s;
	memset(&s, 0, sizeof(s));

	setup_fftw(&s);
	setup_e_history(s.e_history, MAX_SAMPLES);

	s.fd = 0;
	loop(&s);
	return 0;
}

	




