CC = gcc -g -Wall -O3 -fpic -fopenmp -flto=2 -march=native 
#CC = gcc -pg -Wall -fopenmp

SRC = main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c \
	classes.c expmults.c doall.c tune.c block.c listen.c \
	tactics.c badmoves.c expbin.c vonm.c

OBJ = main.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

LIBOBJ = dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

all: clean snob-factor libsnob.so pro

snob-factor:	$(OBJ)
	$(CC) $(OBJ) -lm -o snob-factor

libsnob.so: $(LIBOBJ)
	gcc -shared -o libsnob.so $(LIBOBJ)

pro: prompt.c
	$(CC) -opro prompt.c

clean:
	rm -f *.o snob-factor pro libsnob.so

test:
	cd ./examples && time ../snob-factor < phi.cmd 
