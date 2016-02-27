CC = gcc
CFLAGS = -Wall -I "../debugging"

all:			librawIO13

librawIO13:		sofs_rawdisk.o
			ar -r librawIO13.a $^ sofs_buffercacheinternals.o sofs_buffercache.o
			cp librawIO13.a ../../lib
			rm -f $^ librawIO13.a

clean:
			rm -f librawIO13.a
			rm -f ../../lib/librawIO13.a
