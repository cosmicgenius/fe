CC = g++
CFLAGS = -pedantic -Wall -Wextra -g
OPTFLAGS = -O3

main: obj/main.o obj/input.o obj/algebra.o
	$(CC) -o build/main obj/main.o obj/input.o obj/algebra.o $(CFLAGS) $(OPTFLAGS)

test: obj/test.o obj/algebra.o
	$(CC) -o build/test obj/test.o obj/algebra.o $(CFLAGS) $(OPTFLAGS)
	./build/test

obj/main.o: src/main.cpp include/input.hpp include/algebra.hpp
	$(CC) -o obj/main.o -c src/main.cpp $(CFLAGS) $(OPTFLAGS)

obj/test.o: src/test.cpp include/algebra.hpp
	$(CC) -o obj/test.o -c src/test.cpp $(CFLAGS) $(OPTFLAGS)

obj/input.o: src/input.cpp include/input.hpp include/algebra.hpp 
	$(CC) -o obj/input.o -c src/input.cpp $(CFLAGS) $(OPTFLAGS)

obj/algebra.o: src/algebra.cpp  include/algebra.hpp
	$(CC) -o obj/algebra.o -c src/algebra.cpp $(CFLAGS) $(OPTFLAGS)

run:
	./build/main

gen: 
	mkdir -p build
	mkdir -p obj

.PHONY: clean
clean: 
	-rm obj/*.o build/* 


