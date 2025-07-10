#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <stdbool.h>

typedef enum {
    BUTTON_NONE = 0,
    BUTTON_PLAY_PAUSE,
    BUTTON_PREV,
    BUTTON_NEXT
} button_event_t;

typedef struct {
    int play_pin;
    int prev_pin;
    int next_pin;
    
    int play_last_state;
    int prev_last_state;
    int next_last_state;
} button_manager_t;

// Function declarations
int button_init(button_manager_t *manager, int play_pin, int prev_pin, int next_pin);
button_event_t button_poll(button_manager_t *manager);
int button_wait_for_press(button_manager_t *manager, int timeout_ms);
void button_cleanup(button_manager_t *manager);

#endif
