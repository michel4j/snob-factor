CC = gcc -g3 -Wall -fPIC -O3

SRC = main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c \
	classes.c expmults.c doall.c tune.c block.c listen.c \
	tactics.c badmoves.c expbin.c vonm.c

OBJ = main.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

SMALLOBJ = snob.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

LIBOBJ = dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o 

all: clean snob-factor auto-snob

snob-factor:	$(OBJ)
	$(CC)  $(OBJ) -lm -o snob-factor

auto-snob:	$(SMALLOBJ)
	$(CC) $(SMALLOBJ) -lm -o auto-snob

clean:
	rm -f *.o snob-factor auto-snob

test: all
	cd .. && ./tests/test.sh vm
