CC=mpicc
CFLAGS=-g -fpic -I ../memkind/src -I ../distmem/src 

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

gvirtual: src/gvirtual.o src/localheap.o
	gcc -shared -o libgvirtual.so src/gvirtual.o src/localheap.o
	
clean:
	rm -f src/*.o libgvirtual.so	