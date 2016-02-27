CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO13" -I "../sofs13"
LFLAGS = -L "../../lib"

all:			showblock_sofs13

showblock_sofs13:	showblock_sofs13.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs13 -lrawIO13 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f showblock_sofs13 showblock_sofs13.o
			rm -f ../../run/showblock_sofs13
