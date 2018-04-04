MAKE = mpicc -O2 -g
LIBPATH = /home/ndhai/local/lib
INCLIB = -Iinclude -L$(LIBPATH) -lmercury -lmercury_util -lmercury_hl -lrt -pthread -lna

_DEPS = rpc_write.o na_test.o mercury_test.o na_test_getopt.o mercury_rpc_cb.o #test_bulk.o
DEPS = $(patsubst %,bin/%,$(_DEPS))

all: bin/client bin/server bin/main

bin/%.o: src/%.c include/%.h
	$(MAKE) $< -c -o $@ $(INCLIB)


bin/client: $(DEPS) src/client.c
	$(MAKE) $^ -o $@ $(INCLIB)

bin/server: $(DEPS) src/server.c
	$(MAKE) $^ -o $@ $(INCLIB)

bin/main: $(DEPS) src/main.c
	$(MAKE) $^ -o $@ $(INCLIB)

clean:
	rm -rf bin/*

