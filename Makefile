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

ropebwt2:rle6.o rope6.o main.o
		$(CC) $(CFLAGS) $(DFLAGS) $^ -o $@ $(LIBS)

rle6.o:rle6.h
rope6.o:rle6.h rope6.h
main.o:rle6.h

clean:
		rm -fr gmon.out *.o ext/*.o a.out $(PROG) *~ *.a *.dSYM session*
