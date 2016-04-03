
CFLAGS = -lm -g3 -Wall -lfftw3

awesome: key2.c scale.c shared.h shared.c
	cc key2.c scale.c shared.c ${CFLAGS} -o awesome

awesome_old: key.c scale.c shared.h
	cc key.c scale.c shared.c ${CFLAGS} -o awesome

