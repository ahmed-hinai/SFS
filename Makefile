CC = gcc  # or whatever compiler you are using
sfs: sfs.c 
			$(CC) sfs.c -o /usr/local/bin/sfs -Wall -Wextra -pedantic -lpthread
