CC=			gcc
CFLAGS=		-g -Wall -O2 #-fno-inline-functions -fno-inline-functions-called-once
DFLAGS=
PROG=		ropebwt2
INCLUDES=	
LIBS=		-lz

.SUFFIXES:.c .o

.c.o:
		$(CC) -c $(CFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

ropebwt2:rle.o rope.o bcr.o main.o
		$(CC) $(CFLAGS) $(DFLAGS) $^ -o $@ $(LIBS)

rle.o:rle.h
rope.o:rle.h rope.h
bcr.o:rope.h bcr.h
main.o:rle.h rope.h bcr.h

clean:
		rm -fr gmon.out *.o ext/*.o a.out $(PROG) *~ *.a *.dSYM session*
