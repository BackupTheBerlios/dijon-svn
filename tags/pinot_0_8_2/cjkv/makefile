CPP = g++
CPP_FLAGS = -Wall -O3 -I.
MODULE = cjkv-tokenizer

all: build

build: cjkv-tokenizer.o libcjkv-tokenizer.a

cjkv-tokenizer.o:
	$(CPP) $(CPP_FLAGS) -c cjkv-tokenizer.cc

libcjkv-tokenizer.a: cjkv-tokenizer.o
	ar crv $@ cjkv-tokenizer.o
	ranlib $@

test:
	$(CPP) $(CPP_FLAGS) -o $@ $@.cc -L. -lcjkv-tokenizer -lunicode
	./$@

clean:
	rm -rf *.o *~ *.a test
