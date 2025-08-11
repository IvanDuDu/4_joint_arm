# CMakeLists.txt for Robot Arm Controller

idf_component_register(
    SRCS 
        "main.c"
        "servo_controller.c"
        "gpio_manager.c"
        "UARTConnect.c"
    INCLUDE_DIRS 
        " "
    REQUIRES 
        "driver"
        "esp_system"
        "freertos"
        "nvs_flash"
        "esp_driver_uart"
        
)