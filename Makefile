CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -g -D_GNU_SOURCE $(shell pkg-config --cflags dbus-1 gio-2.0)
LDFLAGS = -lasound -lcdio -lcdio_paranoia -lcdio_cdda -lwiringPi -lbluetooth -lpthread $(shell pkg-config --libs dbus-1 gio-2.0)

SRCDIR = src
BUILDDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET = cd-player

.PHONY: all clean install test assets

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

install: $(TARGET) assets
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

test:
	@echo "Testing CD audio libraries..."
	@pkg-config --exists libcdio && echo "✅ libcdio found" || echo "❌ libcdio missing"
	@ldconfig -p | grep -q libcdio_cdda && echo "✅ libcdio_cdda found" || echo "❌ libcdio_cdda missing"
	@echo "Testing hardware components..."
	@sudo i2cdetect -y 2 2>/dev/null || echo "I2C port 2 not available"
