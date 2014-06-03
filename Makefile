CC=			gcc
CFLAGS=		-g -Wall -O2 #-fno-inline-functions -fno-inline-functions-called-once
DFLAGS=
PROG=		ropebwt2
INCLUDES=	
LIBS=		-lz -lpthread

.SUFFIXES:.c .o

.c.o:
		$(CC) -c $(CFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

ropebwt2:rle.o rope.o mrope.o rld0.o crlf.o main.o
		$(CC) $(CFLAGS) $(DFLAGS) $^ -o $@ $(LIBS)

rle.o:rle.h
rope.o:rle.h rope.h
mrope.o:rope.h mrope.h
rld0.o:rld0.h
crlf.o:crlf.h
main.o:rle.h mrope.h rld0.h crlf.h

clean:
		rm -fr gmon.out *.o ext/*.o a.out $(PROG) *~ *.a *.dSYM session*
