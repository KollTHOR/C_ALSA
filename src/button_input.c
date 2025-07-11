#include "button_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>

#define DEBOUNCE_DELAY 50000

int button_init(button_manager_t *manager, int play_pin, int prev_pin, int next_pin) {
    memset(manager, 0, sizeof(button_manager_t));
    
    // Initialize WiringPi
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Failed to initialize WiringPi\n");
        return -1;
    }
    
    // Store pin numbers (WiringPi numbering)
    manager->play_pin = play_pin;
    manager->prev_pin = prev_pin;
    manager->next_pin = next_pin;
    
    // Set pins as input with pull-up resistors
    pinMode(play_pin, INPUT);
    pullUpDnControl(play_pin, PUD_UP);
    
    pinMode(prev_pin, INPUT);
    pullUpDnControl(prev_pin, PUD_UP);
    
    pinMode(next_pin, INPUT);
    pullUpDnControl(next_pin, PUD_UP);
    
    // Initialize button states
    manager->play_last_state = digitalRead(play_pin);
    manager->prev_last_state = digitalRead(prev_pin);
    manager->next_last_state = digitalRead(next_pin);
    
    printf("WiringPi GPIO initialized successfully\n");
    printf("Button pins - Play: %d, Prev: %d, Next: %d\n", play_pin, prev_pin, next_pin);
    
    return 0;
}

button_event_t button_poll(button_manager_t *manager) {
    // Read current button states
    int play_current = digitalRead(manager->play_pin);
    int prev_current = digitalRead(manager->prev_pin);
    int next_current = digitalRead(manager->next_pin);
    
    button_event_t event = BUTTON_NONE;
    
    // Check for button press (falling edge: 1 -> 0)
    // Buttons are active low with pull-up resistors
    if (manager->play_last_state == 1 && play_current == 0) {
        event = BUTTON_PLAY_PAUSE;
        usleep(DEBOUNCE_DELAY); // Debounce delay
    }
    else if (manager->prev_last_state == 1 && prev_current == 0) {
        event = BUTTON_PREV;
        usleep(DEBOUNCE_DELAY); // Debounce delay
    }
    else if (manager->next_last_state == 1 && next_current == 0) {
        event = BUTTON_NEXT;
        usleep(DEBOUNCE_DELAY); // Debounce delay
    }
    
    // Update last states
    manager->play_last_state = play_current;
    manager->prev_last_state = prev_current;
    manager->next_last_state = next_current;
    
    return event;
}

int button_wait_for_press(button_manager_t *manager, int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        button_event_t event = button_poll(manager);
        if (event != BUTTON_NONE) {
            return event;
        }
        usleep(10000); // 10ms
        elapsed += 10;
    }
    return BUTTON_NONE;
}

void button_cleanup(button_manager_t *manager) {
    // WiringPi doesn't require explicit cleanup for GPIO pins
    // Just reset the structure
    memset(manager, 0, sizeof(button_manager_t));
}
