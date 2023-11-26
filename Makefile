CC = gcc -g -Wall -O3 -fopenmp -flto=2 -march=native 
#CC = gcc -pg -Wall -fopenmp

SRC = main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c \
	classes.c expmults.c doall.c tune.c block.c listen.c \
	tactics.c badmoves.c expbin.c vonm.c

OBJ = main.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o \
	classes.o expmults.o doall.o tune.o block.o listen.o \
	tactics.o badmoves.o expbin.o vonm.o

snob-factor:	$(OBJ)
	$(CC)  $(OBJ) -lm -o snob-factor

pro: prompt.c
	$(CC) -opro prompt.c

clean:
	rm -f *.o snob-factor pro

test:
	cd ./examples && time ../snob-factor < phi.cmd 
