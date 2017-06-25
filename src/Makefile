
CFLAGS = -lm -g3 -Wall -lfftw3

code: code.c
	cc code.c ${CFLAGS} -o code
awesome: key2.c scale.c shared.h shared.c chord.c
	cc key2.c scale.c shared.c chord.c ${CFLAGS} -o awesome

awesome_old: key.c scale.c shared.h
	cc key.c scale.c shared.c ${CFLAGS} -o awesome

