CC = gcc  # or whatever compiler you are using
sfs: sfs.c 
			$(CC) sfs.c -o sfs -Wall -Wextra -pedantic -lpthread
