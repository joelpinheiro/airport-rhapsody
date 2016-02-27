CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO13" -I "../sofs13"
LFLAGS = -L "../../lib"

all:			testifuncs13

testifuncs13:		testifuncs13.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs13 -lsofs13bin -lrawIO13 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f testifuncs13 testifuncs13.o
			rm -f ../../run/testifuncs13
