#include    "UARTconnect.h"
#include    "string.h"
#include    "esp_log.h"


static const char* TAG = "UART_CONNECT";


static uint8_t rx_buffer[UART_RX_BUF_SIZE];  // buffer cố định
static size_t rx_index = 0;                  // con trỏ cuối buffer
static QueueHandle_t signal_queue = NULL;    // queue báo có dữ liệu


esp_err_t uart_manager_init(void) {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    esp_err_t ret = uart_param_config(UART_PORT, &uart_config);
    ret= uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_param_config(UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(ret));
        return ret;
    }
    ret= uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create signal queue
        signal_queue = xQueueCreate(UART_SIGNAL_QUEUE_SIZE, sizeof(uint8_t));
    if (signal_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create signal queue");
        return ESP_FAIL;
    }

    xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 10, NULL);
    xTaskCreate(uart_processing_task, "uart_proc_task", 4096, NULL, 9, NULL);


    ESP_LOGI(TAG, "UART manager initialized on port %d with baud rate %d", UART_PORT, UART_BAUD_RATE);
    return ESP_OK;
}

esp_err_t uart_check_signals(void) {
    // Check the status of the UART signals
    // uart_signal_inv_t inv;
    // esp_err_t ret = uart_get_signal_inv(UART_PORT, &inv);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to get UART signal inversion: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    // ESP_LOGI(TAG, "UART signal inversion: 0x%02X", inv);
    return ESP_OK;
}




esp_err_t uart_read_packet(uart_packet_t *packet, TickType_t timeout) { 
    uint8_t data[sizeof(uart_packet_t)];
    int length = uart_read_bytes(UART_PORT, data, sizeof(data), timeout);

    if(length < sizeof(uart_packet_t)) {
        ESP_LOGE(TAG, "Failed to read UART data: %s", esp_err_to_name(length));
        return ESP_FAIL;
    }

    packet->servo_id = (data[0]>>4) &0x03;
    packet->step_delay_ms = data[0] & 0x0f;
    return ESP_OK;
}

void uart_rx_task(void *param) {
    uint8_t data[UART_PACKET_MAX_SIZE];
    while (1) {
        int length = uart_read_bytes(UART_PORT, data, sizeof(data), portMAX_DELAY);
        if (length > 0) {
            xQueueSend(signal_queue, &data, portMAX_DELAY);
        }
    }
}

void uart_processing_task(void *param) {
    while (1) {
        uint8_t signal;
        if (xQueueReceive(signal_queue, &signal, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Signal received - processing UART buffer");

            size_t index = 0;
            while (index + sizeof(uart_packet_t) <= rx_index) {
                uart_packet_t pkt;
                memcpy(&pkt, &rx_buffer[index], sizeof(uart_packet_t));
                index += sizeof(uart_packet_t);

                // In log packet
                ESP_LOGI(TAG, "Decoded Packet -> Servo ID: %d, Step Delay: %d",
                         pkt.servo_id, pkt.step_delay_ms);
            }

            // Dồn phần còn lại về đầu buffer
            if (index < rx_index) {
                memmove(rx_buffer, rx_buffer + index, rx_index - index);
            }
            rx_index -= index;
        }
    }
}




void uart_manager_log_packet(const uart_packet_t *packet) {
 ESP_LOGI(TAG, "Decoded Packet -> Servo ID: %d, Step Delay: %d",
            (int) packet->servo_id, (int) packet->step_delay_ms);
}




