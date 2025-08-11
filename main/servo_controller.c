#include "servo_controller.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "SERVO";

// Servo configuration
typedef struct {
    int gpio_pin;
    const char* name;
    int current_angle;
    bool initialized;
} servo_config_t;

// GPIO pins and names for each servo
static servo_config_t servo_configs[SERVO_COUNT] = {
    {26, "Forearm", 0, false},  // Cẳng tay
    {27, "Wrist", 0, false},    // Cổ tay  
    {32, "Arm", 0, false},      // Cánh tay
    {33, "Base", 0, false}      // Bụng
};

// PWM configuration constants
#define SERVO_MIN_PULSEWIDTH_US (500)   
#define SERVO_MAX_PULSEWIDTH_US (2500)  
#define SERVO_MAX_DEGREE        (180)   
#define SERVO_FREQUENCY_HZ      (50)
#define SERVO_PERIOD_US         (20000)  // 20ms period
#define SERVO_DUTY_RESOLUTION   LEDC_TIMER_16_BIT

static bool servo_system_initialized = false;

// Private function prototypes
static esp_err_t servo_configure_pwm(servo_id_t servo_id);
static uint32_t servo_angle_to_duty(int angle);
static bool servo_is_valid_id(servo_id_t servo_id);
static bool servo_is_valid_angle(int angle);

esp_err_t servo_init(void) {
    if (servo_system_initialized) {
        ESP_LOGW(TAG, "Servo system already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing servo controller...");

    // Initialize all servos
    for (int i = 0; i < SERVO_COUNT; i++) {
        esp_err_t ret = servo_configure_pwm((servo_id_t)i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure servo %d (%s): %s", 
                    i, servo_configs[i].name, esp_err_to_name(ret));
            servo_deinit();
            return ret;
        }
        servo_configs[i].initialized = true;
        servo_configs[i].current_angle = 0;
    }

    // Set all servos to initial position (0 degrees)
    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_set_angle((servo_id_t)i, 0);
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between servo movements
    }

    servo_system_initialized = true;
    ESP_LOGI(TAG, "Servo controller initialized successfully");
    return ESP_OK;
}

void servo_deinit(void) {
    if (!servo_system_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing servo controller...");

    // Reset all servos to 0 position
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_configs[i].initialized) {
            servo_set_angle((servo_id_t)i, 0);
            servo_configs[i].initialized = false;
            servo_configs[i].current_angle = 0;
        }
    }

    servo_system_initialized = false;
    ESP_LOGI(TAG, "Servo controller deinitialized");
}

esp_err_t servo_set_angle(servo_id_t servo_id, int angle) {
    if (!servo_system_initialized) {
        ESP_LOGE(TAG, "Servo system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!servo_is_valid_id(servo_id)) {
        ESP_LOGE(TAG, "Invalid servo ID: %d", servo_id);
        return ESP_ERR_INVALID_ARG;
    }

    if (!servo_is_valid_angle(angle)) {
        ESP_LOGW(TAG, "Invalid angle %d, clamping to valid range", angle);
        angle = (angle < SERVO_MIN_ANGLE) ? SERVO_MIN_ANGLE : SERVO_MAX_ANGLE;
    }

    uint32_t duty = servo_angle_to_duty(angle);
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, servo_id, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty for servo %s: %s", 
                servo_configs[servo_id].name, esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, servo_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty for servo %s: %s", 
                servo_configs[servo_id].name, esp_err_to_name(ret));
        return ret;
    }

    servo_configs[servo_id].current_angle = angle;
    ESP_LOGI(TAG, "Servo %s set to %d degrees", servo_configs[servo_id].name, angle);
    
    return ESP_OK;
}

esp_err_t servo_set_all_angles(int angles[SERVO_COUNT]) {
    if (!servo_system_initialized) {
        ESP_LOGE(TAG, "Servo system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (angles == NULL) {
        ESP_LOGE(TAG, "Angles array is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting all servo angles");
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        esp_err_t ret = servo_set_angle((servo_id_t)i, angles[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set angle for servo %d", i);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between movements
    }

    return ESP_OK;
}

esp_err_t servo_reset_all(void) {
    ESP_LOGI(TAG, "Resetting all servos to 0 degrees");
    
    int reset_angles[SERVO_COUNT] = {0, 0, 0, 0};
    return servo_set_all_angles(reset_angles);
}

esp_err_t servo_move_smooth(servo_id_t servo_id, int target_angle, int step_delay_ms) {
    if (!servo_system_initialized) {
        ESP_LOGE(TAG, "Servo system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!servo_is_valid_id(servo_id)) {
        ESP_LOGE(TAG, "Invalid servo ID: %d", servo_id);
        return ESP_ERR_INVALID_ARG;
    }

    if (!servo_is_valid_angle(target_angle)) {
        ESP_LOGE(TAG, "Invalid target angle: %d", target_angle);
        return ESP_ERR_INVALID_ARG;
    }

    int current_angle = servo_configs[servo_id].current_angle;
    int step = (target_angle > current_angle) ? 1 : -1;
    
    ESP_LOGI(TAG, "Smooth move servo %s from %d to %d degrees", 
            servo_configs[servo_id].name, current_angle, target_angle);

    while (current_angle != target_angle) {
        current_angle += step;
        esp_err_t ret = servo_set_angle(servo_id, current_angle);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    return ESP_OK;
}

bool servo_is_initialized(void) {
    return servo_system_initialized;
}

const char* servo_get_name(servo_id_t servo_id) {
    if (!servo_is_valid_id(servo_id)) {
        return "Invalid";
    }
    return servo_configs[servo_id].name;
}

int servo_get_current_angle(servo_id_t servo_id) {
    if (!servo_is_valid_id(servo_id)) {
        return -1;
    }
    return servo_configs[servo_id].current_angle;
}

// Private function implementations
static esp_err_t servo_configure_pwm(servo_id_t servo_id) {
    if (!servo_is_valid_id(servo_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = servo_id,
        .duty_resolution  = SERVO_DUTY_RESOLUTION,
        .freq_hz          = SERVO_FREQUENCY_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer for servo %d: %s", 
                servo_id, esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = servo_configs[servo_id].gpio_pin,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = servo_id,
        .timer_sel      = servo_id,
        .duty           = 0,
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel for servo %d: %s", 
                servo_id, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Configured servo %d (%s) on GPIO %d", 
            servo_id, servo_configs[servo_id].name, servo_configs[servo_id].gpio_pin);
    
    return ESP_OK;
}

static uint32_t servo_angle_to_duty(int angle) {
    // Ensure angle is within valid range
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    // Calculate pulse width in microseconds
    uint32_t pulse_width_us = SERVO_MIN_PULSEWIDTH_US + 
                              (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)) / SERVO_MAX_DEGREE;
    
    // Convert to duty cycle value
    uint32_t duty = (pulse_width_us * (1 << SERVO_DUTY_RESOLUTION)) / SERVO_PERIOD_US;
    
    return duty;
}

static bool servo_is_valid_id(servo_id_t servo_id) {
    return (servo_id >= 0 && servo_id < SERVO_COUNT);
}

static bool servo_is_valid_angle(int angle) {
    return (angle >= SERVO_MIN_ANGLE && angle <= SERVO_MAX_ANGLE);
}