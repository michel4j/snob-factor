CC = gcc -g3 -Wall -fPIC -O3 -fopenmp

SRC = main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c \
	classes.c expmults.c doall.c tune.c block.c listen.c \
	tactics.c badmoves.c expbin.c vonm.c

OBJ =  dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

SMALLOBJ = snob.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

LIBOBJ = dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o 

EXECS = auto-snob

all: clean $(EXECS)

auto-snob:	$(SMALLOBJ)
	$(CC) $(CFLAGS) $(SMALLOBJ) -lm -o auto-snob

clean:
	rm -f *.o snob-factor auto-snob

test: all
	cd .. && ./tests/test.sh 5r8c 

