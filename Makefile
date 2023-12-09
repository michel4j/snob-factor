CC = gcc -g -Wall -fPIC -fopenmp -O3 -flto=auto

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

all: clean snob-factor snob-factors libsnob.so

snob-factor:	$(OBJ)
	$(CC)  $(OBJ) -lm -o snob-factor

snob-factors:	$(SMALLOBJ)
	$(CC) $(SMALLOBJ) -lm -o snob-factors

libsnob.so: $(LIBOBJ)
	$(CC) -shared $(LIBOBJ)  -o libsnob.so 

pro: prompt.c
	$(CC) -o pro prompt.c

clean:
	rm -f *.o snob-factor pro

test: clean snob-factor
	./test.sh vm
