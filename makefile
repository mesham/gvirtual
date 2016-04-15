CC=mpicc
CFLAGS=-g -fpic -I ../memkind_build/include -I ../distmem/src 

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

gvirtual: src/gvirtual.o src/localheap.o src/distributedheap.o src/directory.o src/cache.o
	gcc -shared -o libgvirtual.so src/gvirtual.o src/localheap.o src/distributedheap.o src/directory.o src/cache.o -lm
	
clean:
	rm -f src/*.o libgvirtual.so	