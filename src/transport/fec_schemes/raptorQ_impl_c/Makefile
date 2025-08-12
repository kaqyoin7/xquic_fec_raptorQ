ALL: main 
.PHONY: ALL clean libraptor

CC = gcc
LDFLAGS ?= 
CFLAGS += -fPIC -O2 -std=c99 -lm

Decoder.o: Decoder.c Decoder.h
	$(CC) Decoder.c -o Decoder.o -c $(CFLAGS)
Encoder.o: Encoder.c Encoder.h
	$(CC) Encoder.c -o Encoder.o -c $(CFLAGS)
Generators.o: Generators.c Generators.h
	$(CC) Generators.c -o Generators.o -c $(CFLAGS)
Helper.o: Helper.c Helper.h
	$(CC) Helper.c -o Helper.o -c $(CFLAGS)
Main.o: Main.c
	$(CC) Main.c -o Main.o -c $(CFLAGS)
Symbol.o: Symbol.c Symbol.h
	$(CC) Symbol.c -o Symbol.o -c $(CFLAGS)
main: Decoder.o Encoder.o Generators.o Helper.o Main.o Symbol.o
	$(CC) Decoder.o Encoder.o Generators.o Helper.o Main.o Symbol.o -o main -lm $(LDFLAGS)

libraptorq: libraptorq.a
libraptorq.a: Decoder.o Encoder.o Generators.o Helper.o Symbol.o
	ar rcs libraptorq.a $^

clean:
	rm -f *.o main libraptorq.so libraptorq.a libraptor.dll 
