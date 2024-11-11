CC = gcc
CFLAGS = -Wall -Wextra -pedantic -lpthread
OBJ = sfs.o  
TARGET = sfs
INSTALL_DIR = /usr/local/bin
ASSET_DIR = /usr/local/share/$(TARGET)
ASSETS = ./fonts/mono9.tlf ./fonts/mini.flf ./fonts/smmono9.tlf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

sfs.o: sfs.c
	$(CC) -c sfs.c $(CFLAGS)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)
	mkdir -p $(ASSET_DIR)
	install -m 644 $(ASSETS) $(ASSET_DIR)

clean:
	rm -f $(OBJ) $(TARGET)

uninstall: $(TARGET)
	rm -f $(INSTALL_DIR)/$(TARGET)

