CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO13"

IFUNCS1 =
 IFUNCS1 += soAllocInode.o
 IFUNCS1 += soFreeInode.o
 IFUNCS1 += soAllocDataCluster.o
 IFUNCS1 += soFreeDataCluster.o

IFUNCS2 =
 IFUNCS2 += soReadInode.o
 IFUNCS2 += soWriteInode.o
 IFUNCS2 += soCleanInode.o
 IFUNCS2 += soAccessGranted.o

IFUNCS3 =
 IFUNCS3 += soReadFileCluster.o
 IFUNCS3 += soWriteFileCluster.o
 IFUNCS3 += soHandleFileCluster.o
 IFUNCS3 += soHandleFileClusters.o
 IFUNCS3 += soCleanDataCluster.o

IFUNCS4 =
#IFUNCS4 += soGetDirEntryByName.o
#IFUNCS4 += soAddAttDirEntry.o
IFUNCS4 += soRemDetachDirEntry.o
IFUNCS4 += soRenameDirEntry.o
IFUNCS4 += soGetDirEntryByPath.o

OBJS = sofs_blockviews.o sofs_basicoper.o
OBJS += $(IFUNCS1:%=sofs_ifuncs_1/%)
OBJS += $(IFUNCS2:%=sofs_ifuncs_2/%)
OBJS += $(IFUNCS3:%=sofs_ifuncs_3/%)
OBJS += $(IFUNCS4:%=sofs_ifuncs_4/%)

all:		ifuncs1 ifuncs2 ifuncs3 ifuncs4 libsofs13

libsofs13:	$(OBJS)
			ar -r libsofs13.a $^ sofs_basicconsist.o
			cp libsofs13.a ../../lib
			rm -f $^ libsofs13.a

ifuncs1:	
			make -C sofs_ifuncs_1 $(IFUNCS1)

ifuncs2:	
			make -C sofs_ifuncs_2 $(IFUNCS2)

ifuncs3:	
			make -C sofs_ifuncs_3 $(IFUNCS3)

ifuncs4:	
			make -C sofs_ifuncs_4 $(IFUNCS4)

clean:
			rm -f ../../lib/libsofs13.a
			make -C sofs_ifuncs_1 clean
			make -C sofs_ifuncs_2 clean
			make -C sofs_ifuncs_3 clean
			make -C sofs_ifuncs_4 clean
