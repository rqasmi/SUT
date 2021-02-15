# .DEFAULT_GOAL=all

CC=gcc
CFLAGS= -fsanitize=undefined -g -std=gnu99 -O2 -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wshadow \

SUT=sut.o
IO=io.o

sut: sut.c io.c
	$(CC) -o $(SUT) -c sut.c
	$(CC) -o $(IO) -c io.c

clean:
	rm -rf $(SUT) $(IO)

