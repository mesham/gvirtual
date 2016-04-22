CC=mpicc
CFLAGS=-g -I ../src -I ../../memkind_build/include -I ../../distmem/src

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: simple rmaops_local rmaops_dist

simple: simple.o
	$(CC) -o simple simple.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

rmaops_local: rmaops_local.o
	$(CC) -o rmaops_local rmaops_local.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

rmaops_dist: rmaops_dist.o
	$(CC) -o rmaops_dist rmaops_dist.o -lgvirtual -ldistmem -lmemkind -L../ -L../../memkind_build/lib -L../../distmem

clean:
	rm -f *.o simple rmaops_local rmaops_dist