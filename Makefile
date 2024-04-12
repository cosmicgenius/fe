CC = g++
CFLAGS = -pedantic -Wall -Wextra -g
OPTFLAGS = -O3

test: obj/test.o obj/algebra.o
	$(CC) -o build/test obj/test.o obj/algebra.o $(CFLAGS) $(OPTFLAGS)
	./build/test

obj/test.o: src/test.cpp include/algebra.hpp
	$(CC) -o obj/test.o -c src/test.cpp $(CFLAGS) $(OPTFLAGS)

obj/algebra.o: include/algebra.hpp
	$(CC) -o obj/algebra.o -c src/algebra.cpp $(CFLAGS) $(OPTFLAGS)

gen: 
	mkdir -p build
	mkdir -p obj

.PHONY: clean
clean: 
	-rm obj/*.o build/* 


