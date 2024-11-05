CC = gcc
CFLAGS = -Wall -Wextra -pedantic -lpthread
OBJ = sfs.o  
TARGET = sfs
INSTALL_DIR = /usr/local/bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

sfs.o: sfs.c
	$(CC) -c sfs.c $(CFLAGS)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)

clean:
	rm -f $(OBJ) $(TARGET)


