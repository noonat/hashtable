CFLAGS=-O0 -g --std=c99 -Wall -Werror -I.
CC?=llvm-gcc

test: test/test
	@./$<

test/test: hash.o table.o test/test.o
	$(CC) $(CFLAGS) -o $@ $^

test/test.o: test/test.c test/munit.h
	$(CC) $(CFLAGS) -c -o $@ $<

hash.o: hash.c hash.h
	$(CC) $(CFLAGS) -c -o $@ $<

table.o: table.c table.h hash.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test/*.o test/test

.PHONY: clean test
