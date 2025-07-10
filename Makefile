CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -g -D_GNU_SOURCE $(shell pkg-config --cflags dbus-1)
LDFLAGS = -lasound -lcdio -lcdio_paranoia -lwiringPi -lbluetooth -lpthread $(shell pkg-config --libs dbus-1)

SRCDIR = src
BUILDDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET = cd-player

.PHONY: all clean install test

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

test:
	@echo "Testing hardware components..."
	@echo "WiringPi GPIO test:"
	@gpio readall 2>/dev/null || echo "WiringPi not available"
	@echo "D-Bus configuration:"
	@pkg-config --cflags --libs dbus-1 2>/dev/null || echo "D-Bus pkg-config not available"
	@echo "I2C devices:"
	@sudo i2cdetect -y 2 2>/dev/null || echo "I2C port 2 not available"
	@echo "CD-ROM device:"
	@ls -la /dev/cdrom /dev/sr0 2>/dev/null || echo "CD-ROM not found"
	@echo "ALSA devices:"
	@aplay -l 2>/dev/null || echo "No ALSA devices"
