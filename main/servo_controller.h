#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include "esp_err.h"
#include "stdbool.h"

// Servo IDs with meaningful names
typedef enum {
        SERVO_FOREARM = 0,    // Cẳng tay - GPIO 26
    SERVO_WRIST = 1,      // Cổ tay - GPIO 27  
    SERVO_ARM = 2,        // Cánh tay - GPIO 32
    SERVO_BASE = 3,       // Bụng - GPIO 33
    SERVO_COUNT = 4
} servo_id_t;

// Legacy defines for backward compatibility
#define SERVO_1 SERVO_FOREARM
#define SERVO_2 SERVO_WRIST
#define SERVO_3 SERVO_ARM
#define SERVO_4 SERVO_BASE

// Servo angle limits
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

// Function prototypes
esp_err_t servo_init(void);
esp_err_t servo_set_angle(servo_id_t servo_id, int angle);
esp_err_t servo_set_all_angles(int angles[SERVO_COUNT]);
esp_err_t servo_reset_all(void);
esp_err_t servo_move_smooth(servo_id_t servo_id, int target_angle, int step_delay_ms);
// cần hàm hiệu chỉnh cho target angle cho hàm này
esp_err_t servo_uart_controller(servo_id_t servo_id, int8_t step_delay_ms, int8_t direct);

bool servo_is_initialized(void);
void servo_deinit(void);

// Utility functions
const char* servo_get_name(servo_id_t servo_id);
int servo_get_current_angle(servo_id_t servo_id);

#endif // SERVO_CONTROLLER_H