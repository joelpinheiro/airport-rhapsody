CC = gcc
CFLAGS = -Wall

all:			libdebugging

libdebugging:		sofs_probe.o
			ar -r libdebugging.a $^
			cp libdebugging.a ../../lib
			rm -f $^ libdebugging.a

clean:
			rm -f libdebugging.a
			rm -f ../../lib/libdebugging.a
