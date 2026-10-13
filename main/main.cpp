#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

extern "C" void app_main() {
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    while (true) {
        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}