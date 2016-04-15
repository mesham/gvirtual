CC=mpicc
CFLAGS=-g -I ../src -I ../../memkind_build/include -I ../../distmem/src

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: simple rmaops

simple: simple.o
	$(CC) -o simple simple.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

rmaops: rmaops.o
	$(CC) -o rmaops rmaops.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

clean:
	rm -f *.o simple rmaops