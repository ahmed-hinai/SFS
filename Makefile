CC = gcc
CFLAGS = -Wall -Wextra -pedantic -lpthread
OBJ = sfs.o  
TARGET = sfs
INSTALL_DIR = /usr/local/bin
ASSET_DIR = /usr/local/share/$(TARGET)
FONT_DIR = $(ASSET_DIR)/fonts 
DESKTOP_DIR = /usr/share/applications
FONTS = ./fonts/mono9.tlf ./fonts/mini.flf ./fonts/smmono9.tlf
ICON = ./sfs.png
DESKTOP_FILE = ./sfs.desktop

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

sfs.o: sfs.c
	$(CC) -c sfs.c $(CFLAGS)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)
	mkdir -p $(ASSET_DIR)
	mkdir -p $(FONT_DIR)
	install -m 644 $(FONTS) $(FONT_DIR)
	install -m 644 $(DESKTOP_FILE) $(DESKTOP_DIR)
	install -m 644 $(ICON) $(ASSET_DIR)

clean:
	rm -f $(OBJ) $(TARGET)

uninstall: $(TARGET)
	rm -f $(INSTALL_DIR)/$(TARGET)
	rm -rf $(ASSET_DIR)
	rm -f $(DESKTOP_FILE)/$(DESKTOP_FILE)

