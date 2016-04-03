

extern char * note_names[];


typedef struct {
	int legal_notes[12];
	char * name;
} SCALE;


SCALE  * guess_scale(SCALE *s, int num_scales, int * note_frequencies);
void build_all_scales(SCALE ** out, int * n);

