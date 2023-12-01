CC = gcc -g -Wall -fpic -fopenmp -O3 -flto=auto
#CC = gcc -pg -Wall -fopenmp

SRC = main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c \
	classes.c expmults.c doall.c tune.c block.c listen.c \
	tactics.c badmoves.c expbin.c vonm.c snob.c

OBJ = main.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

LIBOBJ = dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o 

SMALLOBJ = snob.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

all: clean snob-factor snob-factor-small libsnob.so

snob-factor:	$(OBJ)
	$(CC) $(OBJ) -lm -o snob-factor

snob-factor-small:	$(SMALLOBJ)
	$(CC) $(SMALLOBJ) -lm -o snob-factor-small

libsnob.so: $(LIBOBJ)
	$(CC) -shared -o libsnob.so $(LIBOBJ)

pro: prompt.c
	$(CC) -opro prompt.c

clean:
	rm -f *.o snob-factor pro libsnob.so snob-factor-small

test:
	cd ./examples && time ../snob-factor < phi.cmd | tee phi.out

small-test:
	cd ./examples && time ../snob-factor-small phi.v phi.s phi.rep 