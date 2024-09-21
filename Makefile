CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address

mysh: mysh.c 
	$(CC) $(CFLAGS) mysh.c -o mysh