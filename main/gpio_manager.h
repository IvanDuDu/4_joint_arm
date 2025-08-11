#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// GPIO pin definitions
#define RESET_BUTTON GPIO_NUM_13

// Button event types
typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_SHORT_PRESS,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_DOUBLE_CLICK
} button_event_type_t;

// Button event structure
typedef struct {
    gpio_num_t gpio_num;
    button_event_type_t event_type;
    uint32_t press_duration_ms;
    uint32_t timestamp;
} button_event_t;

// Button configuration
typedef struct {
    uint32_t debounce_time_ms;      // Debounce time in milliseconds
    uint32_t long_press_time_ms;    // Long press threshold in milliseconds
    uint32_t double_click_time_ms;  // Double click window in milliseconds
} button_config_t;

// Default button configuration
#define DEFAULT_BUTTON_CONFIG() { \
    .debounce_time_ms = 50, \
    .long_press_time_ms = 2000, \
    .double_click_time_ms = 500 \
}

// External queue for button events
extern QueueHandle_t gpio_event_queue;

// Function prototypes
esp_err_t gpio_manager_init(void);
esp_err_t gpio_manager_init_with_config(const button_config_t* config);
void gpio_manager_deinit(void);
bool gpio_manager_is_initialized(void);

// Button event handling
esp_err_t gpio_register_button_callback(void (*callback)(button_event_t*));
void gpio_task(void* parameters);

// Utility functions
const char* gpio_get_event_name(button_event_type_t event_type);

#endif // GPIO_MANAGER_H