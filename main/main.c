#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <inttypes.h>

// Application modules
#include "servo_controller.h"
#include "gpio_manager.h"

static const char* TAG = "MAIN";

// Demo sequences
static void demo_sequence_basic(void);
static void demo_sequence_smooth(void);
static void demo_sequence_coordinated(void);

// Button event handler
static void button_event_handler(button_event_t* event);

// System initialization
static esp_err_t system_init(void);

void app_main(void) {   
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "      Robot Arm Controller v1.0        ");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize system
    if (system_init() != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed!");
        return;
    }
    
    ESP_LOGI(TAG, "System ready - Starting main application loop");
    
    // Main application loop
    uint32_t loop_count = 0;
    while (1) {
        ESP_LOGI(TAG, "=== Loop #%lu ===", ++loop_count);
        
        // Run different demo sequences
        switch (loop_count % 3) {
            case 0:
                ESP_LOGI(TAG, "Running basic demo sequence");
                demo_sequence_basic();
                break;
                
            case 1:
                ESP_LOGI(TAG, "Running smooth movement demo");
                demo_sequence_smooth();
                break;
                
            case 2:
                ESP_LOGI(TAG, "Running coordinated movement demo");
                demo_sequence_coordinated();
                break;
        }
        
        // Wait before next cycle
        ESP_LOGI(TAG, "Demo complete, waiting for next cycle...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static esp_err_t system_init(void) {
    ESP_LOGI(TAG, "Initializing system components...");
    
    // Initialize NVS (for configuration storage if needed)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ NVS initialized");
    
    // Initialize GPIO manager (handles reset button)
    ret = gpio_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO manager: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ GPIO manager initialized");
    
    // Register button event handler
    ret = gpio_register_button_callback(button_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callback: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ Button callback registered");
    
    // Initialize servo controller
    ret = servo_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize servo controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ Servo controller initialized");
    
    // System info
    ESP_LOGI(TAG, "System Information:");
    ESP_LOGI(TAG, "  - Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  - Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());
    //ESP_LOGI(TAG, "  - Reset reason: %lu", esp_reset_reason());
    
    return ESP_OK;
}

static void demo_sequence_basic(void) {
    ESP_LOGI(TAG, "Starting basic movement sequence");
    
    // Move each servo individually
    const int angles[] = {0, 45, 90, 135, 180, 135, 90, 45, 0};
    const int num_angles = sizeof(angles) / sizeof(angles[0]);
    
    for (int servo = 0; servo < SERVO_COUNT; servo++) {
        ESP_LOGI(TAG, "Moving %s servo", servo_get_name((servo_id_t)servo));
        
        for (int i = 0; i < num_angles; i++) {
            servo_set_angle((servo_id_t)servo, angles[i]);
            vTaskDelay(pdMS_TO_TICKS(800));
        }
        
        // Small pause between servos
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "Basic sequence completed");
}

static void demo_sequence_smooth(void) {
    ESP_LOGI(TAG, "Starting smooth movement sequence");
    
    // Smooth movements for wrist servo (most visible)
    servo_move_smooth(SERVO_WRIST, 90, 20);   // Move to 90° slowly
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    servo_move_smooth(SERVO_WRIST, 0, 15);    // Move back to 0° faster
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    servo_move_smooth(SERVO_WRIST, 180, 25);  // Move to max position very slowly
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    servo_move_smooth(SERVO_WRIST, 90, 10);   // Return to center quickly
    
    ESP_LOGI(TAG, "Smooth sequence completed");
}

static void demo_sequence_coordinated(void) {
    ESP_LOGI(TAG, "Starting coordinated movement sequence");
    
    // Define some coordinated positions
    int position1[] = {45, 90, 135, 90};     // Position 1
    int position2[] = {90, 45, 90, 135};     // Position 2
    int position3[] = {135, 135, 45, 45};    // Position 3
    int home[] = {0, 0, 0, 0};               // Home position
    
    // Move through positions
    ESP_LOGI(TAG, "Moving to position 1");
    servo_set_all_angles(position1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Moving to position 2");
    servo_set_all_angles(position2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Moving to position 3");
    servo_set_all_angles(position3);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Returning to home position");
    servo_set_all_angles(home);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Coordinated sequence completed");
}

static void button_event_handler(button_event_t* event) {
    // This function is called from the GPIO task context
    ESP_LOGI(TAG, "Custom button handler: %s", gpio_get_event_name(event->event_type));
    
    switch (event->event_type) {
        case BUTTON_EVENT_SHORT_PRESS:
            ESP_LOGI(TAG, "User requested reset via short press");
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "User requested system reset via long press");
            // Could add system-wide reset here
            break;
            
        case BUTTON_EVENT_DOUBLE_CLICK:
            ESP_LOGI(TAG, "User requested demo mode via double click");
            // Could trigger special demo mode
            break;
            
        default:
            // Other events are handled by the default GPIO task
            break;
    }
}