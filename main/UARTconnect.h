#ifndef UARTCONNECT_H
#define UARTCONNECT_H

#include "esp_err.h"
#include "stdbool.h"
#include "driver/uart.h"
#include "servo_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define UART_PORT UART_NUM_0
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024
#define UART_RX_BUF_SIZE 1024
#define UART_PACKET_MAX_SIZE 64
#define UART_SIGNAL_QUEUE_SIZE 10


typedef struct {
    servo_id_t servo_id[4];
    int angle[4];
    int step_delay_ms[4];
} uart_packet_t;


esp_err_t uart_manager_init(void);
esp_err_t uart_check_signals(void);
esp_err_t uart_read_packet(uart_packet_t *packet, TickType_t timeout);
void uart_rx_task(void *param);
void uart_processing_task(void *param);
void uart_manager_log_packet(const uart_packet_t *packet);


#endif // SERVO_CONTROLLER_H