CPP = g++
CPP_FLAGS = -g -Wall -DDEBUG -O3 -I. `pkg-config --cflags glib-2.0`
LIBS = `pkg-config --libs glib-2.0` #-lunicode
MODULE = CJKVTokenizer

all: build

build: CJKVTokenizer.o libCJKVTokenizer.a

CJKVTokenizer.o:
	$(CPP) $(CPP_FLAGS) -c CJKVTokenizer.cc

libCJKVTokenizer.a: CJKVTokenizer.o
	ar crv $@ CJKVTokenizer.o
	ranlib $@

test:
	$(CPP) $(CPP_FLAGS) -o $@ $@.cc -L. -lCJKVTokenizer $(LIBS)
	./$@

clean:
	rm -rf *.o *~ *.a test
