CC = gcc  # or whatever compiler you are using
sfs: sfs.c 
			$(CC) sfs.c -o sfs.o -Wall -Wextra -pedantic -lpthread 
