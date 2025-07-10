#include "lcd_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdarg.h>

// LCD Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// Backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

// Enable bit
#define En 0x04
#define Rw 0x02
#define Rs 0x01

static void lcd_write_4bits(lcd_t *lcd, uint8_t value) {
    uint8_t data = value | LCD_BACKLIGHT;
    if (write(lcd->i2c_fd, &data, 1) < 0) {
        perror("LCD write error");
    }
    
    data |= En;
    if (write(lcd->i2c_fd, &data, 1) < 0) {
        perror("LCD write error");
    }
    usleep(1);
    
    data &= ~En;
    if (write(lcd->i2c_fd, &data, 1) < 0) {
        perror("LCD write error");
    }
    usleep(50);
}

static void lcd_write_byte(lcd_t *lcd, uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    
    lcd_write_4bits(lcd, highnib | mode);
    lcd_write_4bits(lcd, lownib | mode);
}

static void lcd_command(lcd_t *lcd, uint8_t value) {
    lcd_write_byte(lcd, value, 0);
}

static void lcd_write_char(lcd_t *lcd, uint8_t value) {
    lcd_write_byte(lcd, value, Rs);
}

int lcd_init(lcd_t *lcd, uint8_t address) {
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-2");
    
    lcd->i2c_fd = open(filename, O_RDWR);
    if (lcd->i2c_fd < 0) {
        perror("Failed to open I2C device");
        return -1;
    }
    
    if (ioctl(lcd->i2c_fd, I2C_SLAVE, address) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        close(lcd->i2c_fd);
        return -1;
    }
    
    lcd->address = address;
    
    // Initialize LCD in 4-bit mode
    usleep(50000); // Wait for LCD to power up
    
    // Initialize sequence
    lcd_write_4bits(lcd, 0x30);
    usleep(4500);
    lcd_write_4bits(lcd, 0x30);
    usleep(4500);
    lcd_write_4bits(lcd, 0x30);
    usleep(150);
    lcd_write_4bits(lcd, 0x20); // Set to 4-bit mode
    
    // Function set: 4-bit mode, 2 lines, 5x8 dots
    lcd_command(lcd, LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
    
    // Display control: display on, cursor off, blink off
    lcd_command(lcd, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
    
    // Clear display
    lcd_clear(lcd);
    
    // Entry mode set: increment cursor, no shift
    lcd_command(lcd, LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
    
    return 0;
}

int lcd_clear(lcd_t *lcd) {
    lcd_command(lcd, LCD_CLEARDISPLAY);
    usleep(2000);
    return 0;
}

int lcd_print(lcd_t *lcd, int row, int col, const char *text) {
    if (row < 0 || row > 1 || col < 0 || col > 15) {
        return -1;
    }
    
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(lcd, LCD_SETDDRAMADDR | (col + row_offsets[row]));
    
    for (int i = 0; text[i] != '\0' && i < (16 - col); i++) {
        lcd_write_char(lcd, text[i]);
    }
    
    return 0;
}

int lcd_printf(lcd_t *lcd, int row, int col, const char *format, ...) {
    char buffer[17]; // 16 chars + null terminator
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    return lcd_print(lcd, row, col, buffer);
}

void lcd_cleanup(lcd_t *lcd) {
    if (lcd->i2c_fd >= 0) {
        lcd_clear(lcd);
        close(lcd->i2c_fd);
        lcd->i2c_fd = -1;
    }
}
