MAKE = gcc -O2 -g
LIBPATH = /home/ndhai/local/lib
INCLIB = -Iinclude -L$(LIBPATH) -lna -lmercury -lmercury_util -lmercury_hl -lrt -pthread

all: bin/client bin/server

bin/client: bin/rpc_write.o
	$(MAKE) bin/rpc_write.o src/client.c -o bin/client $(INCLIB)

bin/server: bin/rpc_write.o
	$(MAKE) bin/rpc_write.o src/server.c -o bin/server $(INCLIB)

bin/rpc_write.o: src/rpc_write.c include/rpc_write.h
	$(MAKE) -c src/rpc_write.c -o bin/rpc_write.o $(INCLIB)

clean:
	rm -rf bin/*

