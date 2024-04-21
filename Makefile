.PHONY: clean proj2
CFLAGS =  -std=gnu99 -Wall -Wextra -Werror -pedantic
CC = gcc

proj2: skibus.c
	$(CC) $(CFLAGS) -o proj2 skibus.c
clean:
	rm -f proj2.out
	rm -f proj2