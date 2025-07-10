#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <stdint.h>

typedef struct {
    int i2c_fd;
    uint8_t address;
} lcd_t;

// Function declarations
int lcd_init(lcd_t *lcd, uint8_t address);
int lcd_clear(lcd_t *lcd);
int lcd_print(lcd_t *lcd, int row, int col, const char *text);
int lcd_printf(lcd_t *lcd, int row, int col, const char *format, ...);
void lcd_cleanup(lcd_t *lcd);

#endif
