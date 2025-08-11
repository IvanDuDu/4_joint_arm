#include "gpio_manager.h"
#include "servo_controller.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "GPIO_MGR";

// Global state
QueueHandle_t gpio_event_queue = NULL;
static bool gpio_manager_initialized = false;
static button_config_t current_config;
static void (*button_callback)(button_event_t*) = NULL;

// Button state tracking
typedef struct {
    bool is_pressed;
    uint32_t press_start_time;
    uint32_t last_release_time;
    uint32_t click_count;
    TimerHandle_t debounce_timer;
    TimerHandle_t long_press_timer;
    TimerHandle_t double_click_timer;
} button_state_t;

static button_state_t button_state = {0};

// Forward declarations
static void IRAM_ATTR gpio_isr_handler(void* arg);
static void debounce_timer_callback(TimerHandle_t timer);
static void long_press_timer_callback(TimerHandle_t timer);
static void double_click_timer_callback(TimerHandle_t timer);
static void process_button_press(void);
static void process_button_release(void);
static void send_button_event(button_event_type_t event_type, uint32_t duration);
static esp_err_t create_timers(void);
static void cleanup_timers(void);

esp_err_t gpio_manager_init(void) {
    button_config_t default_config = DEFAULT_BUTTON_CONFIG();
    return gpio_manager_init_with_config(&default_config);
}

esp_err_t gpio_manager_init_with_config(const button_config_t* config) {
    if (gpio_manager_initialized) {
        ESP_LOGW(TAG, "GPIO manager already initialized");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Button configuration is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing GPIO manager...");
    memcpy(&current_config, config, sizeof(button_config_t));

    // Create event queue
    gpio_event_queue = xQueueCreate(10, sizeof(button_event_t));
    if (gpio_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create GPIO event queue");
        return ESP_ERR_NO_MEM;
    }

    // Create timers
    esp_err_t ret = create_timers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timers");
        goto cleanup;
    }

    // Configure reset button GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ULL << RESET_BUTTON)
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Install ISR service
    ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Add ISR handler
    ret = gpio_isr_handler_add(RESET_BUTTON, gpio_isr_handler, (void*)RESET_BUTTON);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Create GPIO task
    BaseType_t task_ret = xTaskCreate(
        gpio_task,
        "gpio_task",
        3072,
        NULL,
        5,
        NULL
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create GPIO task");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    // Initialize button state
    memset(&button_state, 0, sizeof(button_state_t));
    button_state.is_pressed = false;

    gpio_manager_initialized = true;
    ESP_LOGI(TAG, "GPIO manager initialized successfully");
    ESP_LOGI(TAG, "Button config - Debounce: %lums, Long press: %lums, Double click: %lums",
             current_config.debounce_time_ms, 
             current_config.long_press_time_ms,
             current_config.double_click_time_ms);
    
    return ESP_OK;

cleanup:
    gpio_manager_deinit();
    return ret;
}

void gpio_manager_deinit(void) {
    if (!gpio_manager_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing GPIO manager...");

    // Remove ISR handler
    gpio_isr_handler_remove(RESET_BUTTON);

    // Cleanup timers
    cleanup_timers();

    // Delete queue
    if (gpio_event_queue != NULL) {
        vQueueDelete(gpio_event_queue);
        gpio_event_queue = NULL;
    }

    // Reset callback
    button_callback = NULL;

    gpio_manager_initialized = false;
    ESP_LOGI(TAG, "GPIO manager deinitialized");
}

bool gpio_manager_is_initialized(void) {
    return gpio_manager_initialized;
}

esp_err_t gpio_register_button_callback(void (*callback)(button_event_t*)) {
    if (!gpio_manager_initialized) {
        ESP_LOGE(TAG, "GPIO manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    button_callback = callback;
    ESP_LOGI(TAG, "Button callback %s", callback ? "registered" : "unregistered");
    return ESP_OK;
}

const char* gpio_get_event_name(button_event_type_t event_type) {
    switch (event_type) {
        case BUTTON_EVENT_PRESSED: return "PRESSED";
        case BUTTON_EVENT_RELEASED: return "RELEASED";
        case BUTTON_EVENT_SHORT_PRESS: return "SHORT_PRESS";
        case BUTTON_EVENT_LONG_PRESS: return "LONG_PRESS";
        case BUTTON_EVENT_DOUBLE_CLICK: return "DOUBLE_CLICK";
        default: return "UNKNOWN";
    }
}

void gpio_task(void* parameters) {
    button_event_t event;
    ESP_LOGI(TAG, "GPIO task started");

    while (1) {
        if (xQueueReceive(gpio_event_queue, &event, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Button event: %s, GPIO=%d, Duration=%lums", 
                    gpio_get_event_name(event.event_type),
                    event.gpio_num,
                    event.press_duration_ms);

            // Handle different button events
            switch (event.event_type) {
                case BUTTON_EVENT_SHORT_PRESS:
                    ESP_LOGI(TAG, "Performing servo reset (short press)");
                    servo_reset_all();
                    break;
                    
                case BUTTON_EVENT_LONG_PRESS:
                    ESP_LOGI(TAG, "Performing system reset (long press)");
                    // Could add system reset or other functionality here
                    servo_reset_all();
                    break;
                    
                case BUTTON_EVENT_DOUBLE_CLICK:
                    ESP_LOGI(TAG, "Performing demo sequence (double click)");
                    // Could add demo sequence here
                    break;
                    
                default:
                    // Just log press/release events
                    break;
            }

            // Call user callback if registered
            if (button_callback != NULL) {
                button_callback(&event);
            }
        }
    }
}

// Private function implementations
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if ((uint32_t)arg == RESET_BUTTON) {
        bool current_level = gpio_get_level(RESET_BUTTON) == 0; // Active low
        
        if (current_level && !button_state.is_pressed) {
            // Button pressed
            xTimerStartFromISR(button_state.debounce_timer, &xHigherPriorityTaskWoken);
        } else if (!current_level && button_state.is_pressed) {
            // Button released
            xTimerStartFromISR(button_state.debounce_timer, &xHigherPriorityTaskWoken);
        }
    }
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void debounce_timer_callback(TimerHandle_t timer) {
    bool current_level = gpio_get_level(RESET_BUTTON) == 0; // Active low
    
    if (current_level && !button_state.is_pressed) {
        // Confirmed button press
        process_button_press();
    } else if (!current_level && button_state.is_pressed) {
        // Confirmed button release
        process_button_release();
    }
}

static void process_button_press(void) {
    button_state.is_pressed = true;
    button_state.press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Start long press timer
    xTimerStart(button_state.long_press_timer, 0);
    
    // Send press event
    send_button_event(BUTTON_EVENT_PRESSED, 0);
}

static void process_button_release(void) {
    if (!button_state.is_pressed) {
        return;
    }
    
    button_state.is_pressed = false;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t press_duration = current_time - button_state.press_start_time;
    
    // Stop long press timer
    xTimerStop(button_state.long_press_timer, 0);
    
    // Send release event
    send_button_event(BUTTON_EVENT_RELEASED, press_duration);
    
    // Check for double click
    if (button_state.click_count > 0 && 
        (current_time - button_state.last_release_time) <= current_config.double_click_time_ms) {
        // Double click detected
        xTimerStop(button_state.double_click_timer, 0);
        send_button_event(BUTTON_EVENT_DOUBLE_CLICK, press_duration);
        button_state.click_count = 0;
    } else {
        // First click or too much time passed
        button_state.click_count = 1;
        button_state.last_release_time = current_time;
        xTimerStart(button_state.double_click_timer, 0);
    }
}

static void long_press_timer_callback(TimerHandle_t timer) {
    if (button_state.is_pressed) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t press_duration = current_time - button_state.press_start_time;
        send_button_event(BUTTON_EVENT_LONG_PRESS, press_duration);
    }
}

static void double_click_timer_callback(TimerHandle_t timer) {
    if (button_state.click_count == 1) {
        // Single click confirmed
        uint32_t press_duration = button_state.last_release_time - button_state.press_start_time;
        send_button_event(BUTTON_EVENT_SHORT_PRESS, press_duration);
    }
    button_state.click_count = 0;
}

static void send_button_event(button_event_type_t event_type, uint32_t duration) {
    if (gpio_event_queue == NULL) {
        return;
    }
    
    button_event_t event = {
        .gpio_num = RESET_BUTTON,
        .event_type = event_type,
        .press_duration_ms = duration,
        .timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS
    };
    
    BaseType_t result = xQueueSend(gpio_event_queue, &event, 0);
    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send button event to queue");
    }
}

static esp_err_t create_timers(void) {
    // Create debounce timer
    button_state.debounce_timer = xTimerCreate(
        "debounce_timer",
        pdMS_TO_TICKS(current_config.debounce_time_ms),
        pdFALSE,
        NULL,
        debounce_timer_callback
    );
    
    if (button_state.debounce_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create debounce timer");
        return ESP_ERR_NO_MEM;
    }

    // Create long press timer
    button_state.long_press_timer = xTimerCreate(
        "long_press_timer",
        pdMS_TO_TICKS(current_config.long_press_time_ms),
        pdFALSE,
        NULL,
        long_press_timer_callback
    );
    
    if (button_state.long_press_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create long press timer");
        return ESP_ERR_NO_MEM;
    }

    // Create double click timer
    button_state.double_click_timer = xTimerCreate(
        "double_click_timer",
        pdMS_TO_TICKS(current_config.double_click_time_ms),
        pdFALSE,
        NULL,
        double_click_timer_callback
    );
    
    if (button_state.double_click_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create double click timer");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void cleanup_timers(void) {
    if (button_state.debounce_timer != NULL) {
        xTimerDelete(button_state.debounce_timer, portMAX_DELAY);
        button_state.debounce_timer = NULL;
    }
    
    if (button_state.long_press_timer != NULL) {
        xTimerDelete(button_state.long_press_timer, portMAX_DELAY);
        button_state.long_press_timer = NULL;
    }
    
    if (button_state.double_click_timer != NULL) {
        xTimerDelete(button_state.double_click_timer, portMAX_DELAY);
        button_state.double_click_timer = NULL;
    }
}