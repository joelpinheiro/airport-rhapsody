CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -I "../debugging" -I "../rawIO13" -I "../sofs13" -I "../syscalls13"
LFLAGS = -L "../../lib" -L/lib

all:			mount_sofs13

mount_sofs13:		mount_sofs13.o
			$(CC) $(LFLAGS) -o $@ $^ -lsyscalls13bin -lsofs13 -lsofs13bin -lrawIO13 -ldebugging -lpthread -lfuse
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/mount_sofs13
