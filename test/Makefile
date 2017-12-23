MAKE = gcc -O2 -g
LIBPATH = /home/ndhai/local/lib
INCLIB = -Iinclude -L$(LIBPATH) -lna -lmercury -lmercury_util -lmercury_hl -lrt -pthread

all: bin/client bin/server

bin/client: bin/readfile.o
	$(MAKE) bin/readfile.o src/client.c -o bin/client $(INCLIB)

bin/server: bin/readfile.o
	$(MAKE) bin/readfile.o src/server.c -o bin/server $(INCLIB)

bin/readfile.o: src/readfile.c include/readfile.h
	$(MAKE) -c src/readfile.c -o bin/readfile.o $(INCLIB)

clean:
	rm -rf bin/*

