
#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>


#define CHUNK_SIZE (44100/60)

#define LEN(x) (sizeof(x)/sizeof(x[0]))

double note_table[] = {65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.93, 110.00, 116.54, 123.47};
int bucket_to_note[CHUNK_SIZE];
char *note_names[] = {
	"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};

typedef struct {
	fftw_plan fftwplan;
	fftw_complex  * fft_in;
	fftw_complex  * fft_out;
	short tmpdata[CHUNK_SIZE];
	double energy[CHUNK_SIZE];
	int fd;
} STATE;


void build_bucket_to_note() {
	int i;
	for (i = 0; i < CHUNK_SIZE; i++) {
		double f;
		f = i*60;
		while (f > 123.47) {
			f/=2.0;
		}

		double logf = log(f);
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

		fprintf(stderr, "ASSIGN: %f (bucket %i) to %s\n",
				f, i, note_names[j]);

		bucket_to_note[i] = closest;
	}
}

void setup_fftw(STATE * s) {
	s->fftw_in= (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * CHUNK_SIZE);
	s->fftw_out= (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * CHUNK_SIZE);

	s->fftwplan = fftw_plan_dft_c2c_1d(CHUNK_SIZE, s->fftw_in, s->fftw_out, FFTW_MEASURE);
}



void error(char * s) {
	fprintf(stderr, "UHOH: %s\n", s);
	abort();
}


int get_data_chunk(STATE * s) {
	int left_to_read = CHUNK_SIZE;

	while (left_to_read > 0) {
		int len;
		len = read(s->fd, s->tmpdata  + (left_to_read - CHUNK_SIZE), left_to_read * 2);
		if (len <0) {
			error(strerror(errno));
		}

		left_to_read-=len;
	}
	return 0;
}

int do_fft(STATE * s) {

	int i;
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->fftw_in[i] = s->tmpdata[i];
	}
	fftw_execute(s->fftwplan);
	for (i = 0; i < CHUNK_SIZE; i++) {
		s->energy[i] = s->fftw_out[i] * conj(s->fftw_out[i])
	}
}

int bucketize(STATE * s, double * bucket) {

	double total_energy;
	double min_energy;
	int i;

	memset(bucket, 0, 12 * sizeof(int));

	for (i= 0; i < CHUNK_SIZE; i++) {
		total_energy +=s->energy[i];
		bucket[bucket_to_note[i]] += s->energy[i];
	}

	min_energy = total_energy / 10.0;

	for (i = 0; i < LEN(note_table); i++) {
		if (bucket[i] > min_energy) {
			fprintf("%s", note_names[i]);
		} else {
			fprintf("  ");
		}
	}
	fprintf("\n");
}

void loop(STATE * s) {

	double tb[12];
	
	while (1) {
		fprintf(stderr, "GET\n");

		get_data_chunk();
		fprintf(stderr, "FFT\n");
		do_fft();
		fprintf(stderr, "BUCKET\n");
		bucketize(s, tb);
	}
}

void main(void) {
	STATE s;
	build_bucket_to_note();

	setup_fftw(&s);

	s->fd = 0;
	loop(s);
}

	




