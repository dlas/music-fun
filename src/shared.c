/* Copyright (C) 2016 David Stafford

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2 as published by the Free
Software Foundation. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include <unistd.h>
#include <stdlib.h>


/*
 * What are the frequencies and names of the notes.
 */
double note_table[] = {65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.93, 110.00, 116.54, 123.47};
char *note_names[] = {"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};

/*
 * Read raw data into memory. Put it in output
 */
int get_data_chunk(short * output, int chunk_samples) {
	/*
	 * Bytes = chunk_size * bytes_per_short * channels 
	 */
	int left_to_read = chunk_samples* 2 * 2;
	int pos = 0;

	/* Keep reading until we have enough data */
	while (left_to_read > 0) {
		int len;
		len = read(0, ((char*)(output))  + pos, left_to_read);
		if (len <0) {
			abort();
		}
		if (len == 0) {
			return -1;
		}
		pos+=len;

		left_to_read-=len;
	}
	return 0;
}


