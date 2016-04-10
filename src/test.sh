#!/bin/bash

echo "" > $2

for i in $1/*.pcm; do
	echo RUNNING TEST $i
	echo ================================================ >> $2
	echo FILE:$i >> $2
	./awesome -nodisplay -max 500 < "$i" >>$2; 
	echo >> $2
	echo >> $2
done
