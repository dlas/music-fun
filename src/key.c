
#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "shared.h"

#define TIME 10

#define CHUNK_SIZE (44100/TIME)


#define LEN(x) (sizeof(x)/sizeof(x[0]))

double note_table[] = {65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.93, 110.00, 116.54, 123.47};
int bucket_to_note[CHUNK_SIZE];
char *note_names[] = {
	"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};

typedef struct {
	fftw_plan fftwplan;
	fftw_complex  * fftw_in;
	fftw_complex  * fftw_out;
	short tmpdata[CHUNK_SIZE * 2];
	double energy[CHUNK_SIZE];
	int fd;
} STATE;


void build_bucket_to_note2() {
	int i;

	for (i = 0; i < CHUNK_SIZE; i++) {
		bucket_to_note[i] = -1;
	}

	for (i = 0; i < LEN(note_table); i++) {
		double f = note_table[i];

		do {	
			int bucket = f/TIME;
			bucket_to_note[bucket] = i;
			bucket_to_note[bucket+1] = i;
			f=f*2.0;
		}while (f < 44100);


	}

	for (i = 0; i < CHUNK_SIZE; i++) {
		fprintf(stderr, "ASSIGN: %i to %i\n", i*TIME, bucket_to_note[i]);
	}
}
void build_bucket_to_note() {
	int i;

	for (i = 0; i < CHUNK_SIZE; i++) {
		double f;
		f = i*TIME;

		while (f > 123.47) {
			f/=2.0;
		}

		int j;
		int closest = 0;
		double closest_dist = 100000000;
		for (j = 0; j < LEN(note_table); j++) {
			double dist = fabs(log(f) - log(note_table[j]));
			if (dist < closest_dist) {
				closest_dist = dist;
				closest = j;
			}
		}

		fprintf(stderr, "ASSIGN: %f(%f) (bucket %i) to %i:%s\n",
				(double)(i*TIME), f, i, closest, note_names[closest]);

		bucket_to_note[i] = closest;
	}
}

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


int get_data_chunk(STATE * s) {
	int left_to_read = CHUNK_SIZE * 2 * 2;
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

int do_fft(STATE * s) {

	int i;
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->fftw_in[i] = s->tmpdata[i*2];
	}
	fftw_execute(s->fftwplan);
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->energy[i] = (s->fftw_out[i] * conj(s->fftw_out[i]));
		
	}

	return 0;
}

int bucketize(STATE * s, double * bucket, int *tcount) {

	double total_energy;
	double min_energy;
	int i;

	memset(bucket, 0, LEN(note_table) * sizeof(double));

	for (i= 26; i < CHUNK_SIZE/16; i++) {
		int b;
		total_energy +=s->energy[i];
		b = bucket_to_note[i];
		if (b != -1) {
			bucket[b] += s->energy[i];
		}
	}

	min_energy = total_energy / 10;

	for (i = 0; i < LEN(note_table); i++) {
		if (bucket[i] > min_energy && log10(bucket[i])>10.0) {
			printf("%s", note_names[i]);
			tcount[i]++;
		} else {
			printf("  ");
		}
	}

	for (i = 0; i < LEN(note_table); i++) {
		printf("%f\t", log10(bucket[i]));
	}
	printf("\n");
	return 0;
}

void loop(STATE * s, SCALE * scale, int scale_n) {

	double tb[LEN(note_table)];
	int tcount[LEN(note_table)];

	int count = 0;
	SCALE * last_scale = NULL;
	
	while (1) {

		get_data_chunk(s);
		do_fft(s);
		if (last_scale) {
			fprintf(stderr, "SCALE: %s \t", last_scale->name);
		}
		bucketize(s, tb, tcount);
		count++;
		if (count > 1000) {
			last_scale = guess_scale(scale, scale_n, tcount);
			memset(tcount, 0, sizeof(int) * LEN(note_table));
			count = 0;
		}
	}
}

int main(void) {
	STATE s;
	SCALE * scale;
	int scale_n;
	build_all_scales(&scale, &scale_n);
	build_bucket_to_note2();

	setup_fftw(&s);

	s.fd = 0;
	loop(&s, scale, scale_n);
	return 0;
}

	




