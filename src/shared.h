/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/



typedef struct {
	int legal_notes[12];
	char * name;
} SCALE;


SCALE  * guess_scale(SCALE *s, int num_scales, int * note_frequencies);
void build_all_scales(SCALE ** out, int * n);
int get_data_chunk(short * output, int chunk_samples);

extern double note_table[12];
extern char *note_names[12];


