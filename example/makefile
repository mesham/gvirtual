CC=mpicc
CFLAGS=-g -I ../src -I ../../memkind_build/include -I ../../distmem/src

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: simple

simple: simple.o
	$(CC) -o simple simple.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

clean:
	rm -f *.o simple